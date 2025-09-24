#include "framework.h"
#include "lr_parser.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <map>
#include <set>
#include <cctype>
#include <locale>
#include <codecvt>

production_id_t parse::production_t::prod_id_size = 1;


/*
### Algorithm Features
1. **Iteration until convergence**: Use a `do-while` loop until no new symbols are added to any FIRST set (`changed` is false)
2. **Process the right part of the production**: For each production, check the symbols from left to right:
   - Encounter **terminal symbol**: Add it to the FIRST set of the left non-terminal symbol, and stop processing this production
   - Upon encountering a **non-terminal symbol**: add its FIRST set (excluding ¦Å) to the FIRST set of the left non-terminal symbol
	 - If the FIRST set of the non-terminal symbol contains ¦Å, continue to check the next symbol
	 - Otherwise, stop processing this production rule
3. **Handling the ¦Å case**: If all symbols in the production can derive ¦Å, add ¦Å to the FIRST set of the left nonterminal
*/
void parse::lalr_grammar::comp_first_sets() {
	bool changed = true;
	do {
		changed = false;

		// Iterate over all productions
		for (const auto& [left, prods] : productions) {

			for (const auto& prod : prods) {
				size_t i = 0;

				bool continue_checking = true;

				// Check each symbol in the production from left to right
				while (i < prod->right.size() && continue_checking)
				{
					const parse::symbol_t& sym = prod->right[i];

					if (sym.type == parse::symbol_type_t::TERMINAL || sym.type == parse::symbol_type_t::EPSILON) {
						// If it's a terminal, add it to the FIRST set
						if (first_sets[left].insert(sym).second) {
							// extract the boolean result of insert 
							// to see if it was actually added
							changed = true;
						}

						continue_checking = false; // Stop checking further
					}
					else
					{
						// non-terminal, add its FIRST set, excluding epsilon
						for (const auto& first_sym : first_sets[sym]) {
							if (first_sym != epsilon) {
								if (first_sets[left].insert(first_sym).second) {
									changed = true;
								}
							}
						}

						// If the non-terminal's FIRST set does not contain epsilon, stop
						if (first_sets[sym].find(epsilon) == first_sets[sym].end()) {
							continue_checking = false;
						}
						else {
							// Otherwise, continue to the next symbol
							i++;
						}
					}
				}

				if (i == prod->right.size() && continue_checking) {
					// If we reached the end and all symbols can derive epsilon, add epsilon
					if (first_sets[left].insert(epsilon).second) {
						changed = true;
					}
				}
			}
		}
	} while (changed);


}

/*
### Algorithm Features
1. **Processing symbol sequence**: For each symbol in the given sequence:
   - Encounter **terminator**: add it to the result set and stop processing
   - Encountering a **non-terminal symbol**: add its FIRST set (excluding ¦Å) to the result set
	 - If the FIRST set of the non-terminal symbol does not contain ¦Å, stop processing
	 - Otherwise, proceed to the next symbol
2. **Handling ¦Å**: If all symbols in the sequence can derive ¦Å, add the passed-in look-ahead symbol to the result set
*/
std::unordered_set<parse::symbol_t, parse::symbol_hasher> parse::lalr_grammar::comp_first_of_sequence(
	const std::vector<parse::symbol_t>& sequence,
	const std::unordered_set<parse::symbol_t, parse::symbol_hasher>& lookaheads
)
{
	std::unordered_set<parse::symbol_t, parse::symbol_hasher> result;

	bool all_contain_epsilon = true;

	for (const auto& sym : sequence) {
		if (sym.type == parse::symbol_type_t::TERMINAL) {
			result.insert(sym);
			all_contain_epsilon = false;
			break;
		}
		else
		{
			// add sym into non-terminal FIRST excluding epsilon
			for (const auto& first_sym : first_sets[sym]) {
				if (first_sym.name != epsilon.name) {
					result.insert(first_sym);
				}
			}
			if (first_sets[sym].find(epsilon) == first_sets[sym].end()) {
				all_contain_epsilon = false;
				break;
			}
		}
	}

	if (all_contain_epsilon) {
		// if all symbols can derive epsilon, add lookaheads
		for (const auto& la : lookaheads) {
			result.insert(la);
		}
	}

	return result;
}


std::shared_ptr<parse::lalr1_item_set> parse::lalr_grammar::closure(const parse::lalr1_item_set& I)
{

	if (I.items.empty())
		return std::make_shared<lalr1_item_set>();

	std::shared_ptr<lalr1_item_set> new_I = std::make_shared<lalr1_item_set>(I);
	std::unordered_map<lalr1_item_t, bool, lalr1_item_hasher> item_handled;

	bool changed = true;

	do
	{
		changed = false;

		// Copy current items to avoid modification during iteration
		std::unordered_set<lalr1_item_t, lalr1_item_hasher> current_items = new_I->get_items();

		for (const parse::lalr1_item_t& item : current_items) {

#ifdef __DEBUG_OUTPUT__
			//std::cout << "Current item: " << item.to_string() << std::endl;
#endif

			auto it_handled = item_handled.find(item);
			if (it_handled != item_handled.end() && it_handled->second) {
				continue; // Already processed
			}

			item_handled[item] = true; // Mark as processed


			parse::symbol_t next_sym = item.next_symbol();

			if (next_sym.type == parse::symbol_type_t::NON_TERMINAL && !next_sym.name.empty()) {

				// add new items for each production of next_sym
				std::vector<std::shared_ptr<parse::production_t>> prods = get_productions_for(next_sym);


				// compute beta a (the right part after the non-terminal and the lookaheads)
				std::vector<parse::symbol_t> beta_a;
				for (int i = item.dot_pos + 1; i < item.product->right.size(); i++) {
					beta_a.push_back(item.product->right[i]);
				}
				// compute FIRST(beta a)
				std::unordered_set<parse::symbol_t, parse::symbol_hasher> lookaheads =
					comp_first_of_sequence(beta_a, item.lookaheads);
				// merge with existing lookaheads
				lookaheads.insert(item.lookaheads.begin(), item.lookaheads.end());



				for (const std::shared_ptr<parse::production_t> prod : prods) {

					int next_sym_prod_dot_pos = 0;
					bool all_can_derive_epsilon = true;

					while (next_sym_prod_dot_pos < prod->right.size() && all_can_derive_epsilon) {
						parse::symbol_t current_sym = prod->right[next_sym_prod_dot_pos];
						parse::lalr1_item_t new_item(prod, next_sym_prod_dot_pos, lookaheads);

						const lalr1_item_t* existing_item = new_I->find_item_by_id(new_item.id);
						if (existing_item != nullptr) {

							// Item with same core exists, merge lookaheads
							new_I->add_lookaheads_for_item(new_item.id, lookaheads);

						}
						else {
							// New item, simply add it
							//new_I->items.insert(new_item);
							new_I->add_items(new_item);
							changed = true;
						}

						if (current_sym.type != parse::symbol_type_t::NON_TERMINAL) {
							all_can_derive_epsilon = false;
							break;
						}

						bool can_derive_epsilon = false;
						if (first_sets.count(current_sym)) {
							const auto& current_sym_first_sym = first_sets.at(current_sym);

							if (current_sym_first_sym.find(epsilon) != current_sym_first_sym.end()) {
								can_derive_epsilon = true;

								std::vector<std::shared_ptr<parse::production_t>> sub_prods = get_productions_for(current_sym);
								for (const auto& sub_prod : sub_prods) {
									parse::lalr1_item_t sub_new_item(sub_prod, 0, lookaheads);

									const parse::lalr1_item_t* found = new_I->find_item_by_id(sub_new_item.id);
									if (found == nullptr) {
										new_I->add_items(sub_new_item);
										changed = true;
									}
									else
									{
										new_I->add_lookaheads_for_item(sub_new_item.id, lookaheads);
									}
								}

							}
						}

						if (!can_derive_epsilon) {
							all_can_derive_epsilon = false;
							break;
						}


						next_sym_prod_dot_pos++;
					}

				}

			}
		}

	} while (changed);

#ifdef __DEBUG_OUTPUT__
	std::cout << "LALR(1) Closure of start state:" << std::endl;
	std::cout << *new_I << std::endl;
#endif
	return new_I;
}

std::shared_ptr<parse::lalr1_item_set> parse::lalr_grammar::go_to(const lalr1_item_set& I, const symbol_t& X)
{

	lalr1_item_set new_set = lalr1_item_set();

	for (const auto& item : I.items) {

		//std::cout << item.to_string() << std::endl;

		if (item.dot_pos < item.product->right.size() &&
			item.next_symbol() == X) {
			parse::lalr1_item_t moved_item(item.product, item.dot_pos + 1, item.lookaheads);
			new_set.add_items(moved_item);
		}

		//// skip epsilon
		//int pos = item.get_dot_pos();
		//while (pos < item.product->right.size()) {
		//	symbol_t current = item.product->right[pos];

		//	if (current.type == symbol_type_t::NON_TERMINAL && can_derive_epsilon(current)) {
		//		pos++;

		//		if (pos < item.product->right.size() && item.product->right[pos] == X) {
		//			lalr1_item_t new_item(item.product, pos + 1, item.lookaheads);
		//			new_set.add_items(new_item);
		//		}
		//	}
		//	else
		//	{
		//		break;
		//	}
		//}
	}

	return closure(new_set);

}


std::shared_ptr<parse::lr0_item_set> parse::lalr_grammar::lr0_closure(const lr0_item_set& I) const {

	std::shared_ptr<lr0_item_set> new_I = std::make_shared<lr0_item_set>(I);

	bool changed = true;
	do
	{
		changed = false;

		// Copy current items to avoid modification during iteration
		std::unordered_set<lr0_item_t, lr0_item_hasher> current_items = new_I->get_items(); 

		for (const parse::lr0_item_t& i : current_items) {
			parse::symbol_t next_sym = i.next_symbol();

			if (next_sym.type == parse::symbol_type_t::NON_TERMINAL && !next_sym.name.empty()) {

				// add new items for each production of next_sym
				std::vector<std::shared_ptr<parse::production_t>> prods = get_productions_for(next_sym);
				for (auto& prod : prods) {

					int next_sym_prod_dot_pos = 0;
					bool all_can_derive_epsilon = true;

					while (next_sym_prod_dot_pos < prod->right.size() && all_can_derive_epsilon) {
						parse::symbol_t current_sym = prod->right[next_sym_prod_dot_pos];

						parse::lr0_item_t new_item(prod, next_sym_prod_dot_pos);

						// Check if the new item already exists
						const parse::lr0_item_t* found = new_I->find_item(new_item);
						if (found == nullptr) {
							new_I->add_items(new_item);
							changed = true;
						}


						if (current_sym.type != parse::symbol_type_t::NON_TERMINAL) {
							all_can_derive_epsilon = false;
							break;
						}

						bool can_derive_epsilon = false;
						if (first_sets.count(current_sym)) {
							const auto& current_sym_first_sym = first_sets.at(current_sym);

							// Deal with special case when the following non-terminals also can derive epsilon.
							if (current_sym_first_sym.find(epsilon) != current_sym_first_sym.end()) {
								can_derive_epsilon = true;

								std::vector<std::shared_ptr<parse::production_t>> sub_prods = get_productions_for(current_sym);
								for (const auto& sub_prod : sub_prods) {

									parse::lr0_item_t sub_new_item(sub_prod, 0);

									// Check if the new item already exists
									const parse::lr0_item_t* found = new_I->find_item(sub_new_item);
									if (found == nullptr) {
										new_I->add_items(sub_new_item);
										changed = true;
									}

								}

							}
						}

						if (!can_derive_epsilon) {
							all_can_derive_epsilon = false;
							break;
						}


						next_sym_prod_dot_pos++;
					}

				}

			}

		}


	} while (changed);

	return new_I;
}

/*
	return CLOSURE( GOTO[I, X] );
*/
std::shared_ptr<parse::lr0_item_set> parse::lalr_grammar::lr0_go_to(const lr0_item_set& I, const symbol_t& X) const
{

	lr0_item_set result = lr0_item_set();

	// for each [A -> ¦Á . Y ¦Â] in I
	for (const auto& item : I.get_items()) {
		if (item.next_symbol() == X) {
			parse::lr0_item_t moved_item(item.product, item.dot_pos + 1);
			result.add_items(moved_item);
		}

		//// skip epsilon
		//int pos = item.get_dot_pos();
		//while (pos < item.product->right.size()) {
		//	symbol_t current = item.product->right[pos];

		//	if (current.type == symbol_type_t::NON_TERMINAL && can_derive_epsilon(current)) {
		//		pos++;

		//		if (pos < item.product->right.size() && item.product->right[pos] == X) {
		//			lalr1_item_t new_item(item.product, pos + 1);
		//			result.add_items(new_item);
		//		}
		//	}
		//	else
		//	{
		//		break;
		//	}
		//}
	}

	return lr0_closure(result);
}

std::unique_ptr<std::vector<std::shared_ptr<parse::lr0_item_set>>> parse::lalr_grammar::build_lr0_states()
{

	auto lr0_states = std::make_unique<std::vector<std::shared_ptr<parse::lr0_item_set>>>();

	// Create the augmented grammar
	std::shared_ptr<parse::production_t> augmented_prod = std::make_shared<parse::production_t>(
		parse::symbol_t(start_symbol.name + "'", parse::symbol_type_t::NON_TERMINAL),
		std::vector<parse::symbol_t>{ start_symbol }
	);
	augmented_prod->id = AUGMENTED_GRAMMAR_PROD_ID;
	productions.insert({ augmented_prod->left, { augmented_prod } });

	// Initialize the first item set with the augmented production (using lr0_item_t)
	lr0_item_set start_set;
	start_set.id = 0;
	start_set.items.insert(parse::lr0_item_t(augmented_prod, 0)); // Correct item type

	// Compute closure for the start set
	std::shared_ptr<lr0_item_set> closured_start_set = lr0_closure(start_set);
	lr0_states->push_back(closured_start_set);

	// Use an index-based loop to process all states
	for (size_t i = 0; i < lr0_states->size(); i++) {
		std::shared_ptr<parse::lr0_item_set> current_set = (*lr0_states)[i];

		// Collect all symbols that can be transitioned on from current state
		std::unordered_set<parse::symbol_t, parse::symbol_hasher> transition_symbols;
		for (const auto& item : current_set->get_items()) {

			int dot_pos = item.dot_pos;
			while (dot_pos < item.product->right.size()) {
				parse::symbol_t next_sym = item.product->right[dot_pos];
				if (next_sym.type == parse::symbol_type_t::TERMINAL) {
					transition_symbols.insert(next_sym);
					break;
				}

				if (next_sym.type == parse::symbol_type_t::NON_TERMINAL) {
					transition_symbols.insert(next_sym);

					if (!can_derive_epsilon(next_sym))
						break;
				}

				dot_pos++;
				
			}
		}

		// For each symbol, compute GOTO and add new states
		for (const auto& symbol : transition_symbols) {
			std::shared_ptr<lr0_item_set> goto_set = lr0_go_to(*current_set, symbol);
			if (goto_set == nullptr || goto_set->get_items().empty()) {
				continue; // Skip empty GOTO sets
			}

			// Check if the GOTO set already exists in lr0_states
			bool exists = false;
			item_set_id_t existing_id = 0;
			for (const auto& state : *lr0_states) {
				if (*state == *goto_set) {
					exists = true;
					existing_id = state->id;
					break;
				}
			}

			if (!exists) {
				// Add new state
				goto_set->id = lr0_states->size();
				lr0_states->push_back(goto_set);
				existing_id = goto_set->id;
			}

			// Record the GOTO transition in the cache table
			goto_table[{current_set->id, symbol}] = existing_id;
		}
	}

	// construct goto_table
	//for (const auto& I : *lr0_states) {

	//	for (const auto& nt : non_terminals) {
	//		if (goto_table.count({ I->id, nt }) == 0) {
	//			const auto& goto_set = lr0_go_to(*I, nt);
	//			item_set_id_t existing_id = -1;

	//			for (const auto& state : *lr0_states) {
	//				if (*state == *goto_set) {
	//					existing_id = state->id;
	//					break;
	//				}
	//			}

	//			if (existing_id != -1) {
	//				goto_table[{I->id, nt}] = existing_id;
	//			}

	//		}

	//	}

	//	for (const auto& t : terminals) {
	//		if (goto_table.count({ I->id, t }) == 0) {
	//			const auto& goto_set = lr0_go_to(*I, t);
	//			item_set_id_t existing_id = -1;

	//			for (const auto& state : *lr0_states) {
	//				if (*state == *goto_set) {
	//					existing_id = state->id;
	//					break;
	//				}
	//			}

	//			if (existing_id != -1) {
	//				goto_table[{I->id, t}] = existing_id;
	//			}

	//		}

	//	}

	//}

	// Debug output: print states and GOTO table

//#ifdef __DEBUG_OUTPUT__
	std::cout << "Total LR(0) states: " << lr0_states->size() << std::endl;
	for (const auto& state : *lr0_states) {
		std::cout << *state << std::endl;
	}

	std::cout << "GOTO transitions:" << std::endl;
	for (const auto& entry : goto_table) {
		std::cout << "  From state " << entry.first.first
			<< " to state " << entry.second
			<< " on symbol '" << entry.first.second.name << "'" << std::endl;
	}

//#endif

	return std::move(lr0_states);
}

/*
	We initialize the LALR(1) states based on the LR(0) states.
	All lalr(1) items are initialized with empty lookahead sets,
	except for the start item in state 0, which is initialized with the end marker ($).
*/
void parse::lalr_grammar::initialize_lalr1_states() {

	auto lr0_states = build_lr0_states();

	lalr1_states.resize(lr0_states->size());

	lalr1_states[0] = std::make_shared<lalr1_item_set>(0);

	for (const auto& item : (*lr0_states)[0]->get_items()) {
		if (item.product->id == AUGMENTED_GRAMMAR_PROD_ID) {

			lalr1_item_t start_item(item);
			lalr1_states[0]->add_items(start_item);

			break;
		}
	}

	// initialize for other states
	for (item_set_id_t i = 1; i < lr0_states->size(); i++) {
		lalr1_states[i] = std::make_shared<lalr1_item_set>(i);
		for (const auto& item : (*lr0_states)[i]->get_items()) {

			if (item.is_kernel_item()) {
				lalr1_item_t la_item(item);
				lalr1_states[i]->add_items(la_item);
			}
		}
	}

}


// Parses the input and constructs LALR(1) parsing tables
void parse::lalr_grammar::determine_lookaheads(
	const item_set_id_t I_id,
	const symbol_t X,
	std::unordered_map<std::pair<item_set_id_t, item_id_t>, std::vector<std::pair<item_set_id_t, item_id_t>>, pair_items_state_item_id_hasher>& propagation_graph,
	std::unordered_map<std::pair<item_set_id_t, item_id_t>, std::unordered_set<parse::symbol_t, parse::symbol_hasher>, pair_items_state_item_id_hasher>& spontaneous_lookaheads)
{

	// Retrieve LALR(1) state corresponding to I_id
	std::shared_ptr<lalr1_item_set> I = lalr1_states[I_id];

	// Process each kernel item in the state
	for (const auto& kernel : I->get_items()) {
		// Create a copy of kernel item with sentinel lookahead
		lalr1_item_t kernel_with_sentinel = lalr1_item_t(kernel);
		kernel_with_sentinel.add_lookaheads(lookahead_sentinel);

		// Create item set containing the kernel with sentinel
		lalr1_item_set original_item_set;
		original_item_set.add_items(kernel_with_sentinel);

		// Compute closure of the item set (TODO: Add caching)
		std::shared_ptr<lalr1_item_set> J = closure(original_item_set);

		// Process each item in the closure
		for (const auto& B : J->get_items()) {
			// Skip items where next symbol isn't X
			if (B.next_symbol() != X)
				continue;

			// Process each lookahead symbol in item B
			for (const auto& la : B.lookaheads) {
				if (la != lookahead_sentinel) {
					// Handle spontaneous lookahead propagation
					auto closured_goto_B_set = go_to(*J, X);
					auto target_item_id = goto_table[{I_id, X}];

					// Find corresponding item in GOTO set and record spontaneous lookahead
					for (const auto& goto_B : closured_goto_B_set->get_items()) {
						if (goto_B.product->id == B.product->id && goto_B.dot_pos == B.dot_pos + 1)
							spontaneous_lookaheads[{target_item_id, goto_B.id}].insert(la);
					}
				}

				if (la == lookahead_sentinel) {
					// Handle lookahead propagation through sentinel
					auto closured_goto_B_set = go_to(*J, X);
					auto target_item_id = goto_table[{I_id, X}];

					// Record propagation relationship in graph
					for (const auto& goto_B : closured_goto_B_set->get_items()) {
						if (goto_B.product->id == B.product->id && goto_B.dot_pos == B.dot_pos + 1)
							propagation_graph[{I_id, kernel.id}].push_back({ target_item_id, goto_B.id });
					}
				}
			}
		}
	}
}

// Computes and propagates lookaheads for all LALR(1) states
void parse::lalr_grammar::set_lalr1_items_lookaheads()
{


#ifdef __DEBUG__
	//std::cout << "LALR(1) States Built. Total States: " << lalr1_states.size() << std::endl;
	//std::cout << lalr1_states_to_string() << std::endl;
#endif

	// Data structures for tracking lookahead propagation
	std::unordered_map<std::pair<item_set_id_t, item_id_t>, std::vector<std::pair<item_set_id_t, item_id_t>>, pair_items_state_item_id_hasher> propagation_graph;
	std::unordered_map<std::pair<item_set_id_t, item_id_t>, std::unordered_set<parse::symbol_t, parse::symbol_hasher>, pair_items_state_item_id_hasher> spontaneous_lookaheads;

	// Process all states for both terminal and non-terminal symbols
	for (item_set_id_t i = 0; i < lalr1_states.size(); i++) {
		for (const auto& X : terminals)
			determine_lookaheads(i, X, propagation_graph, spontaneous_lookaheads);

		for (const auto& X : non_terminals)
			determine_lookaheads(i, X, propagation_graph, spontaneous_lookaheads);
	}


	// Apply spontaneous lookaheads to items
	for (const auto& spon : spontaneous_lookaheads) {
		auto [I_id, i_id] = spon.first;
		lalr1_states[I_id]->add_lookaheads_for_item(i_id, spon.second);
	}

	item_id_t start_item_id = 0;
	for (const auto& item : lalr1_states[0]->get_items()) {
		if (item.product->id == AUGMENTED_GRAMMAR_PROD_ID) {
			start_item_id = item.id;
			break;
		}
	}

	lalr1_states[0]->add_lookaheads_for_item(start_item_id, { end_marker });



#ifdef __DEBUG__
	//std::cout << "LALR(1) States Built. Total States: " << lalr1_states.size() << std::endl;
	//std::cout << lalr1_states_to_string() << std::endl;
#endif



	// Propagate lookaheads until no changes occur
	bool changed = true;
	do {
		changed = false;
		for (item_set_id_t I_id = 0; I_id < lalr1_states.size(); I_id++) {
			for (const auto& kernel : lalr1_states[I_id]->get_items()) {
				// Check if current item has propagation entries
				auto prop_it = propagation_graph.find({ I_id, kernel.id });
				if (prop_it != propagation_graph.end()) {
					for (const auto& target_pair : prop_it->second) {
						item_set_id_t target_set_id = target_pair.first;
						item_id_t target_item_id = target_pair.second;

						const auto target_item = lalr1_states[target_set_id]->find_item_by_id(target_item_id);
						if (!target_item) continue;

						// Collect lookaheads that need to be added
						std::unordered_set<parse::symbol_t, parse::symbol_hasher> to_add;
						for (const auto& la : kernel.lookaheads) {
							if (target_item->lookaheads.find(la) == target_item->lookaheads.end()) {
								to_add.insert(la);
							}
						}

						// Add new lookaheads and mark change if necessary
						if (!to_add.empty()) {
							lalr1_states[target_set_id]->add_lookaheads_for_item(target_item_id, to_add);
							changed = true;
						}
					}
				}
			}
		}
	} while (changed);

#ifdef __DEBUG__
	std::cout << "LALR(1) States Built. Total States: " << lalr1_states.size() << std::endl;
	std::cout << lalr1_states_to_string() << std::endl;
#endif

}

// Constructs the ACTION table from LALR(1) states
void parse::lalr_grammar::build_action_table()
{
	// Process each state in the LALR(1) state machine
	for (item_set_id_t i = 0; i < lalr1_states.size(); i++) {
		auto& state = lalr1_states[i];
		// Compute closure of the state
		auto J = closure(*state);

		// Process each item in the closure
		for (auto& item : J->get_items()) {
			auto& prod = item.product;

			// Handle reduce actions (dot at end of production)
			if (item.dot_pos >= prod->right.size() || (prod->right.size() == 1 && prod->right[0] == epsilon && item.dot_pos == 0)) {
				for (auto& la : item.lookaheads) {
					// Check for conflicts with existing actions
					if (action_table[i].count(la)) {
						auto& existing_action = action_table[i][la];
						if (existing_action.type == parser_action_type_t::SHIFT) {
							std::cerr << "Shift-Reduce conflict at state " << i
								<< " on symbol " << la.name
								<< " between shift to " << existing_action.value
								<< " and reduce by production " << get_production_by_id(prod->id)->to_string() << std::endl;
							throw std::runtime_error("Shift-Reduce conflict detected");
						}
						else if (existing_action.type == parser_action_type_t::REDUCE) {
							std::cerr << "Reduce-Reduce conflict at state " << i
								<< " on symbol " << la.name
								<< " between production " << existing_action.value
								<< " and production " << get_production_by_id(prod->id)->to_string() << std::endl;
							throw std::runtime_error("Reduce-Reduce conflict detected");
						}
					}

					// Handle accept action (start symbol with end marker)
					if (prod->left == start_symbol && la == end_marker)
						action_table[i][la] = { parser_action_type_t::ACCEPT, AUGMENTED_GRAMMAR_PROD_ID };
					else
						action_table[i][la] = { parser_action_type_t::REDUCE, prod->id };
				}
			}
			// Handle shift actions (dot before terminal symbol)
			else {
				const auto& next_symbol = prod->right[item.dot_pos];

				if (next_symbol.type == symbol_type_t::TERMINAL) {
					if (next_symbol != epsilon) {
						// Check if GOTO entry exists for this symbol
						auto goto_key = std::make_pair(i, next_symbol);
						if (goto_table.find(goto_key) != goto_table.end()) {
							auto next_state = goto_table[{i, next_symbol}];

							// Check for shift-shift conflicts
							if (action_table[i].count(next_symbol)) {
								auto& existing_action = action_table[i][next_symbol];
								if (!(existing_action.type == parser_action_type_t::SHIFT && existing_action.value == next_state)) {

									std::cerr << (existing_action.type != parser_action_type_t::SHIFT ? "Reduce" : "Shift")
										<< "-Shift conflict at state " << i
										<< " on symbol " << next_symbol.name
										<< " between " + existing_action.to_string() + "{" + get_production_by_id(existing_action.value)->to_string() + "}"
										<< " and shift to " << next_state << std::endl;
									throw std::runtime_error("Shift-Shift conflict detected");
								}
							}
							else {
								// Add shift action to table
								action_table[i][next_symbol] = { parser_action_type_t::SHIFT, static_cast<parser_action_value_t>(next_state) };
							}
						}
					}
					else {
						// Handle epsilon productions by adding reduce actions
						for (const auto& la : item.lookaheads) {
							action_table[i][la] = { parser_action_type_t::REDUCE, prod->id };
						}
					}
				}
			}
		}
	}
}
