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


production_id_t gram::production_t::prod_id_size = 1;
item_set_id_t gram::lr0_item_set::lr0_item_set_size = 0;

static std::string trim(const std::string & str) {
	size_t start = str.find_first_not_of(" \t\n\r");
	size_t end = str.find_last_not_of(" \t\n\r");
	if (start == std::string::npos || end == std::string::npos) {
		return "";
	}
	return str.substr(start, end - start + 1);
}

static size_t find_arrow_position(const std::string& line)
{
	// Try to find different arrow representations
	size_t pos = line.find("->");
	if (pos != std::string::npos) return pos;
	pos = line.find("¡ú");
	if (pos != std::string::npos) return pos;
	// Try to find UTF-8 encoded arrow (may be multi-byte character)
	pos = line.find("\xE2\x86\x92"); // UTF-8 encoded ¡ú
	if (pos != std::string::npos) return pos;
	return std::string::npos;
}

static size_t get_arrow_length(const std::string& line, size_t arrow_pos)
{
	if (arrow_pos == std::string::npos) return 0;
	// Check if it's "->"
	if (line.substr(arrow_pos, 2) == "->") return 2;
	// Check if it's "¡ú" (single character)
	if (line.substr(arrow_pos, 1) == "¡ú") return 1;
	// Check if it's UTF-8 encoded ¡ú (three bytes)
	if (arrow_pos + 2 < line.size() &&
		line.substr(arrow_pos, 3) == "\xE2\x86\x92") return 3;
	return 0;
}

static std::string to_valid_identifier(const std::string& name)
{
	if (name.empty()) return "empty";
	std::string result;
	for (char c : name) {
		if (std::isalnum(static_cast<unsigned char>(c))) {
			result += c;
		}
		else {
			result += '_';
		}
	}
	// If it starts with a digit, add a prefix
	if (!result.empty() && std::isdigit(static_cast<unsigned char>(result[0]))) {
		result = "NT_" + result;
	}
	return result;
}

static std::vector<std::string> split(const std::string& str, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(str);
	while (std::getline(tokenStream, token, delimiter)) {
		token = trim(token);
		if (!token.empty()) {
			tokens.push_back(token);
		}
	}
	return tokens;
}

static bool is_non_terminal(const std::string& symbol)
{
	if (symbol.empty()) return false;
	if (symbol == "¦Å" || symbol == "epsilon") return true;
	// Check if it's enclosed in <>
	if (symbol.size() >= 2 && symbol[0] == '<' && symbol[symbol.size() - 1] == '>') {
		return true;
	}
	// Check if it starts with an uppercase letter
	return std::isupper(static_cast<unsigned char>(symbol[0]));
}

static std::string extract_symbol_name(const std::string& symbol)
{
	if (symbol.size() >= 2 && symbol[0] == '<' && symbol[symbol.size() - 1] == '>') {
		return symbol.substr(1, symbol.size() - 2);
	}
	return symbol;
}


gram::grammar grammar_parser(const std::string& filename) {


	gram::grammar grammar;
	
	std::ifstream file(filename);

	if (!file.is_open())
	{
		std::cerr << "Failed to open grammar file: " << filename << std::endl;
	}


	std::string line;
	bool first_production = true;

	while (std::getline(file, line)) {

		// trim the line
		line = trim(line);

		// skip empty lines and comments
		if (line.empty() || line[0] == '#') {
			continue;
		}

		// find the arrow position
		size_t arrow_pos = find_arrow_position(line);
		if (arrow_pos == std::string::npos) {
			std::cerr << "Error: Invalid production format (no arrow found): " << line << std::endl;
			continue;
		}

		size_t arrow_len = get_arrow_length(line, arrow_pos);
		if (arrow_len == 0) {
			std::cerr << "Error: Invalid arrow format: " << line << std::endl;
			continue;
		}

		std::string left_str = trim(line.substr(0, arrow_pos));
		std::string right_str = trim(line.substr(arrow_pos + arrow_len));

		// extract the left non-terminal
		std::string left_symbol = extract_symbol_name(left_str);
		if (left_symbol.empty()) {
			std::cerr << "Error: Invalid left symbol: " << left_str << std::endl;
			continue;
		}

		// set the start symbol (the first non-terminal)
		if (first_production) {
			first_production = false;
			grammar.start_symbol = gram::symbol_t(left_symbol, gram::symbol_type_t::NON_TERMINAL);
		}

		// record the non-terminal
		grammar.non_terminals.insert(gram::symbol_t(left_symbol, gram::symbol_type_t::NON_TERMINAL));

		// handle multiple productions on the right side (separated by |)
		std::vector<std::string> alternatives;
		size_t pipe_pos = right_str.find('|');

		if (pipe_pos == std::string::npos) {
			alternatives.push_back(right_str);
		}
		else {
			// split multiple productions
			size_t start = 0;
			while (pipe_pos != std::string::npos) {
				alternatives.push_back(trim(right_str.substr(start, pipe_pos - start)));
				start = pipe_pos + 1;
				pipe_pos = right_str.find('|', start);
			}
			alternatives.push_back(trim(right_str.substr(start)));
		}

		// handle each production
		for (const auto& alt : alternatives) {
			std::vector<gram::symbol_t> right_symbols;
			if (alt == "¦Å" || alt == "epsilon" || alt.empty()) {
				// empty production
				right_symbols.push_back(grammar.epsilon);
			}
			else {
				// split the right side symbols by space
				std::vector<std::string> tokens = split(alt, ' ');
				for (const auto& token : tokens) {
					std::string symbol_name = extract_symbol_name(token);
					if (symbol_name.empty()) {
						std::cerr << "Error: Invalid symbol: " << token << std::endl;
						continue;
					}
					gram::symbol_t symbol;
					if (is_non_terminal(token)) {
						symbol = gram::symbol_t(symbol_name, gram::symbol_type_t::NON_TERMINAL);
						grammar.non_terminals.insert(symbol);
					}
					else {
						symbol = gram::symbol_t(symbol_name, gram::symbol_type_t::TERMINAL);
						if (!(symbol == grammar.epsilon)) {
							grammar.terminals.insert(symbol);
						}
					}
					right_symbols.push_back(symbol);
				}
			}
			// add to the production set
			grammar.add_production(
				gram::symbol_t(left_symbol, gram::symbol_type_t::NON_TERMINAL),
				right_symbols
			);
		}

	}

	file.close();
	
	return grammar;
}


std::shared_ptr<gram::lalr1_item_set> gram::grammar::closure(const gram::lalr1_item_set& I)
{

	if (I.items.empty())
		return std::make_shared<lalr1_item_set>();

	std::shared_ptr<lalr1_item_set> new_I = std::make_shared<lalr1_item_set>(I);
	new_I->id = I.id;
	std::unordered_map<lalr1_item_t, bool, lr0_item_hasher> item_handled;

	bool changed = true;

	do
	{
		changed = false;

		// Copy current items to avoid modification during iteration
		auto current_items = new_I->get_items(); 

		for (const gram::lalr1_item_t& item : current_items) {

			auto it_handled = item_handled.find(item);
			if (it_handled != item_handled.end() && it_handled->second) {
				continue; // Already processed
			}

			item_handled[item] = true; // Mark as processed
			

			gram::symbol_t next_sym = item.next_symbol();

			if (next_sym.type == gram::symbol_type_t::NON_TERMINAL && !next_sym.name.empty()) {

				// compute beta a (the right part after the non-terminal and the lookaheads)
				std::vector<gram::symbol_t> beta_a;
				for (int i = item.dot_pos + 1; i < item.product->right.size(); i++) {
					beta_a.push_back(item.product->right[i]);
				}

				// compute FIRST(beta a)
				std::unordered_set<gram::symbol_t, gram::symbol_hasher> lookaheads =
					comp_first_of_sequence(beta_a, *item.lookaheads);

				// merge with existing lookaheads
				lookaheads.insert(item.lookaheads->begin(), item.lookaheads->end());

				// add new items for each production of next_sym
				std::vector<std::shared_ptr<gram::production_t>> prods = get_productions_for(next_sym);

				for (const std::shared_ptr<gram::production_t> prod : prods) {

                    gram::lalr1_item_t new_item(prod, 0, lookaheads);

					//auto existing_item_iter = std::find_if(
					//	new_I->items.begin(), new_I->items.end(),
					//	[&new_item](const lalr1_item& existing) {
					//		return existing.product->id == new_item.product->id &&
					//			existing.dot_pos == new_item.dot_pos;
					//	});

					const lalr1_item_t* existing_item = new_I->find_item(new_item);

					//if (existing_item_iter != new_I->items.end()) {
					if (existing_item != nullptr) {
						// Item with same core exists, merge lookaheads
						//size_t before_size = existing_item_iter->lookaheads.size();
						size_t before_size = existing_item->lookaheads->size();

						//gram::lalr1_item merged_item = *existing_item_iter; // Copy existing item
						gram::lalr1_item_t merged_item = *existing_item; // Copy existing item


						new_I->items.erase(*existing_item); // Remove old item
						//new_I->items.erase(*existing_item_iter); // Remove old item
						//merged_item.lookaheads.insert(lookaheads.begin(), lookaheads.end()); // Merge lookaheads
						merged_item.add_lookaheads(lookaheads);
						new_I->items.insert(merged_item); // Reinsert modified item
						if (merged_item.lookaheads->size() > before_size) {
							changed = true;
						}
					}
					else {
						// New item, simply add it
						new_I->items.insert(new_item);
						changed = true;
					}

				}

			}
		}

	} while (changed);

	//std::cout << "LALR(1) Closure of start state:" << std::endl;
	//std::cout << *new_I << std::endl;

	return new_I;
}

std::shared_ptr<gram::lalr1_item_set> gram::grammar::go_to(const lalr1_item_set& I, const symbol_t& X)
{

	std::shared_ptr<lalr1_item_set> new_set = std::make_shared<lalr1_item_set>();

	for (const auto& item : I.items) {

		//std::cout << item.to_string() << std::endl;

		if (item.dot_pos < item.product->right.size() &&
			item.next_symbol() == X) {
			gram::lalr1_item_t moved_item(item.product, item.dot_pos + 1, *item.lookaheads);
			new_set->items.insert(moved_item);
		}
	}

	return closure(*new_set);

}


std::shared_ptr<gram::lr0_item_set> gram::grammar::lr0_closure(const lr0_item_set& I) const {

	std::shared_ptr<lr0_item_set> new_I = std::make_shared<lr0_item_set>(I);
	new_I->add_items(I);

	bool changed = true;
	do
	{
		changed = false;

		std::unordered_set<lr0_item_t, lr0_item_hasher> current_items = new_I->get_items(); // Copy current items to avoid modification during iteration

		for (const gram::lr0_item_t& i : current_items) {
			gram::symbol_t next_sym = i.next_symbol();

			if (next_sym.type == gram::symbol_type_t::NON_TERMINAL && !next_sym.name.empty()) {
				// add new items for each production of next_sym
				std::vector<std::shared_ptr<gram::production_t>> prods = get_productions_for(next_sym);
				for (auto& prod : prods) {
					//gram::lr0_item new_item(const_cast<gram::production*>(&prod), 0);
					gram::lr0_item_t new_item(prod, 0);

					// Check if the new item already exists
						
					const gram::lr0_item_t* found = new_I->find_item(new_item);
					if (found == nullptr) {
						new_I->add_items(new_item);
						changed = true;
					}

				}
			}

		}


	} while (changed);

	return new_I;
}

std::shared_ptr<gram::lr0_item_set> gram::grammar::lr0_go_to(const lr0_item_set& I, const symbol_t& X) const
{

	auto result = std::make_shared<lr0_item_set>();

	for (const auto& item : I.get_items()) {
		if (item.next_symbol() == X) {
			gram::lr0_item_t moved_item(item.product, item.dot_pos + 1);
			result->add_items(moved_item);
		}
	}

	if (result->get_items().empty()) {
		return nullptr;
	}

	return lr0_closure(*result);
}

//std::vector<std::shared_ptr<gram::lr0_item_set>> gram::grammar::build_lr0_states()
void gram::grammar::build_lr0_states()
{
	//std::vector<std::shared_ptr<gram::lr0_item_set>> lr0_states;

	// Create the augmented grammar
    std::shared_ptr<gram::production_t> augmented_prod = std::make_shared<gram::production_t>(
        gram::symbol_t(start_symbol.name + "'", gram::symbol_type_t::NON_TERMINAL),
        std::vector<gram::symbol_t>{ start_symbol },
		action_t{}  // Empty action
    );

	// set the start symbol to the new augmented start symbol
	augmented_prod->id = AUGMENTED_GRAMMAR_PROD_ID;
	productions.insert({ augmented_prod->left, { augmented_prod } });
	
	// initialize the first item set with the augmented production
	lr0_item_set start_set;
	start_set.id = 0;
	start_set.items.insert(gram::lalr1_item_t(augmented_prod, 0, { end_marker }));

	// compute its closure
	std::shared_ptr<lr0_item_set> closured_start_set = lr0_closure(start_set);
	lr0_states.push_back(closured_start_set);

	for (size_t i = 0; i < lr0_states.size(); i++) {
		std::shared_ptr<gram::lr0_item_set> current_set = lr0_states[i];

		// collect all symbols that can be transitioned on
		std::unordered_set<gram::symbol_t, gram::symbol_hasher> transition_symbols;

		for (const auto& item : current_set->get_items()) {
			gram::symbol_t next_sym = item.next_symbol();
			if (!next_sym.name.empty()) {
				transition_symbols.insert(next_sym);
			}
		}

		// for each symbol, compute the GOTO set
		for (const auto& symbol : transition_symbols) {
			std::shared_ptr<lr0_item_set> goto_set = lr0_go_to(*current_set, symbol);
			if (goto_set != nullptr && !goto_set->get_items().empty()) {
				// check if this set already exists
				bool exists = false;
				for (const auto& I : lr0_states) {
					if (*I == *goto_set) {
						exists = true;
						break;
					}
				}
				if (!exists) {

					// assign a new ID
					goto_set->id = static_cast<item_set_id_t>(lr0_states.size());

					lr0_states.push_back(goto_set);
					goto_table[{ current_set->id, symbol }] = static_cast<item_set_id_t>(goto_set->id);
				}
			}
		}

	}

#ifdef __DEBUG__
	
	std::cout << "Total LR(0) states: " << lr0_states.size() << std::endl;
	for (const auto& state : lr0_states) {
		std::cout << *state << std::endl;
	}

	std::cout << "GOTO transitions:" << std::endl;
	for (const auto& from_state : lr0_states) {
		for (const auto& symbol : from_state->get_transition_symbols()) {
			std::shared_ptr<lr0_item_set> to_state = lr0_go_to(*from_state, symbol);
			if (to_state != nullptr) {
				// find the state in lr0_states
				for (const auto& s : lr0_states) {
					if (*s == *to_state) {
						std::cout << "  From state " << from_state->id
							<< " to state " << s->id
							<< " on symbol '" << symbol.name << "'" << std::endl;
						break;
					}
				}
			}
		}
	}

#endif

	//// Remove non-kernel items from each state
	//for (auto& state_ptr : lr0_states) {
	//	auto& items = state_ptr->get_items();

	//	// Use erase-remove idiom to remove non-kernel items
	//	items.erase(
	//		std::remove_if(items.begin(), items.end(),
	//			[](const lr0_item& item) {
	//				// Keep only kernel items
	//				return !item.is_kernel_item(); 
	//			}),
	//		items.end()
	//	);
	//}

	//return lr0_states;
}


/*
	We initialize the LALR(1) states based on the LR(0) states.
	All lalr(1) items are initialized with empty lookahead sets,
	except for the start item in state 0, which is initialized with the end marker ($).
*/
void gram::grammar::initialize_lalr1_states() {

	//lr0_states = build_lr0_states();
	lalr1_states.resize(lr0_states.size());

	lalr1_states[0] = std::make_shared<lalr1_item_set>(0);

	for (const auto& item : lr0_states[0]->get_items()) {
		if (item.product->id == AUGMENTED_GRAMMAR_PROD_ID) {

			lalr1_item_t start_item(item, { end_marker });
			lalr1_states[0]->add_items(start_item);

			break;
		}
	}

	// initialize for other states
	for (item_set_id_t i = 1; i < lr0_states.size(); i++) {
		lalr1_states[i] = std::make_shared<lalr1_item_set>(i);
		for (const auto& item : lr0_states[i]->get_items()) {

			if (item.is_kernel_item()) {
				lalr1_item_t la_item(item);
				lalr1_states[i]->add_items(la_item);
			}
		}
	}

}

void gram::grammar::propagate_lookaheads() {
	bool changed;

	std::vector<std::tuple<int, lr0_item_t, int, lr0_item_t>> propagation_edges;

	for (item_set_id_t i = 0; i < lr0_states.size(); i++)
	{
		for (const auto& item : lr0_states[i]->get_items()) {

			if (item.is_kernel_item())
				continue;

			if (item.dot_pos >= item.product->right.size())
				continue;


			// X is the symbol after the dot
			symbol_t X = item.next_symbol();

			// get target state j
			auto goto_key = std::make_pair(i, X);
			if (goto_table.find(goto_key) == goto_table.end())
				continue;

			item_set_id_t j = goto_table[goto_key];

			lalr1_item_t target_item(item.product, item.dot_pos + 1);

			std::cout << "Processing item in state " << i << ": " << item.to_string() << std::endl;
			std::cout << "Target item in state " << j << ": " << target_item.to_string() << std::endl;
			std::cout << std::endl;

			// compute beta
			std::vector<symbol_t> beta;
			for (int k = item.dot_pos + 1; k < item.product->right.size(); k++) {
				beta.push_back(item.product->right[k]);
			}

			// compute FIRST(beta)
			std::unordered_set<symbol_t, symbol_hasher> first_beta = comp_first_of_sequence(beta);

			// compute lookaheads to propagate
			// use pointer to avoid modifying the original item
			const lalr1_item_t* source_la_item = lalr1_states[j]->find_item(target_item);
			if (source_la_item != nullptr) {

				if (first_beta.size() != 0) {

					lalr1_states[j]->del_items(target_item);
					for (const auto& la : first_beta) {
						if (la != epsilon) {
							//target_item.lookaheads.insert(la);
							target_item.add_lookahead(la);
						}
					}
					lalr1_states[j]->add_items(target_item);

				}
			}

			// if FIRST(beta) contains epsilon, add propagation edge
			if (first_beta.size() == 0 || first_beta.find(epsilon) != first_beta.end()) {
				propagation_edges.push_back(std::make_tuple(i, item, j, target_item));
			}

		}
	}

	//std::cout << lalr1_states_to_string() << std::endl;

	do
	{
		changed = false;

		for (const auto& [from_set_id, from_item, to_set_id, to_item] : propagation_edges) {

			const lalr1_item_t* from_la_item = lalr1_states[from_set_id]->find_item(from_item);
			const lalr1_item_t* to_la_item = lalr1_states[to_set_id]->find_item(to_item);
			if (from_la_item == nullptr || to_la_item == nullptr)
				continue;

			size_t before_size = to_la_item->lookaheads->size();
			// merge lookaheads
			for (const auto& la : *from_la_item->lookaheads) {
				if (to_la_item->lookaheads->find(la) == to_la_item->lookaheads->end()) {
					lalr1_item_t updated_to_item = *to_la_item;
					//updated_to_item.lookaheads.insert(la);
					updated_to_item.add_lookahead(la);
					lalr1_states[to_set_id]->items.erase(*to_la_item);
					lalr1_states[to_set_id]->add_items(updated_to_item);
				}
			}
			if (to_la_item->lookaheads->size() > before_size) {
				changed = true;
			}
		}

	} while (changed);

	//std::cout << lalr1_states_to_string() << std::endl;
}

//std::tuple<
//	std::unordered_map<gram::lalr1_item_t, gram::symbol_t, gram::lr0_item_hasher>,
//	std::unordered_set<gram::lalr1_item_t, gram::lr0_item_hasher>
//> gram::grammar::detemine_lookaheads(
//	const std::shared_ptr<gram::lr0_item_set>& I, const gram::symbol_t& X
//)
//{
//	std::unordered_map<gram::lalr1_item_t, gram::symbol_t, gram::lr0_item_hasher> spontaneous_lookaheads;
//	std::unordered_set<gram::lalr1_item_t, gram::lr0_item_hasher> propagated_lookaheads;
//
//	//std::vector<gram::lr0_item*> kernel_item_ptrs;
//	//// collect kernel items
//	//for (const auto& item : I->get_items()) {
//	//	if (item.is_kernel_item()) {
//	//		kernel_item_ptrs.push_back(const_cast<gram::lr0_item*>(&item));
//	//	}
//	//}
//
//	//for (const gram::lr0_item* item_ptr : kernel_item_ptrs) {
//	//	if (item_ptr->next_symbol() == X) {
//
//	//	}
//	//}
//
//
//	for (const auto& lr0item : I->get_items()) {
//
//		if (!lr0item.is_kernel_item())
//			continue;
//
//		gram::lalr1_item_set original_item_set(0);
//		gram::lalr1_item_t lr0item_with_lookahead(
//			lr0item,
//			{ lookahead_sentinel } // initial lookahead
//		);
//
//		original_item_set.add_items(lr0item_with_lookahead);
//
//		auto J = closure(original_item_set);
//
//		//// compute beta, dot_pos + 1 to end
//		//std::vector<gram::symbol> beta;
//		//for (int j = lr0item.dot_pos + 1; j < lr0item.product->right.size(); j++) {
//		//	beta.push_back(lr0item.product->right[j]);
//		//}
//
//		//// compute FIRST(beta)
//		//if (beta.empty()) {
//		//	beta.push_back(lookahead_sentinel);
//		//}
//		//std::unordered_set<gram::symbol, gram::symbol_hasher> first_beta = comp_first_of_sequence(beta);
//		//
//
//		for (const auto& item_in_J : J->items) {
//			// for each [B -> ¦Ã . X ¦È, a] in J 
//			if (item_in_J.next_symbol() == X) {
//
//
//				for (const auto& la : item_in_J.lookaheads) {
//					if (la != lookahead_sentinel)
//					{
//						//auto goto_items = go_to(*J, X);
//						spontaneous_lookaheads[item_in_J] = la;
//					}
//					else
//					{
//						propagated_lookaheads.insert(item_in_J);
//					}
//				}
//
//			}
//		}
//
//	}
//
//	return { spontaneous_lookaheads, propagated_lookaheads };
//
//}

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
void gram::grammar::comp_first_sets() {
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
					const gram::symbol_t& sym = prod->right[i];

					if (sym.type == gram::symbol_type_t::TERMINAL) {
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
							if (first_sym.name != epsilon.name) {
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
std::unordered_set<gram::symbol_t, gram::symbol_hasher> gram::grammar::comp_first_of_sequence(
	const std::vector<gram::symbol_t>& sequence,
	const std::unordered_set<gram::symbol_t, gram::symbol_hasher>& lookaheads
)
{
	std::unordered_set<gram::symbol_t, gram::symbol_hasher> result;

	bool all_contain_epsilon = true;

	for (const auto& sym : sequence) {
		if (sym.type == gram::symbol_type_t::TERMINAL) {
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

