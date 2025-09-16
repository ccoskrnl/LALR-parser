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

static std::string trim(const std::string& str) {
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
	pos = line.find("→");
	if (pos != std::string::npos) return pos;
	// Try to find UTF-8 encoded arrow (may be multi-byte character)
	pos = line.find("\xE2\x86\x92"); // UTF-8 encoded →
	if (pos != std::string::npos) return pos;
	return std::string::npos;
}

static size_t get_arrow_length(const std::string& line, size_t arrow_pos)
{
	if (arrow_pos == std::string::npos) return 0;
	// Check if it's "->"
	if (line.substr(arrow_pos, 2) == "->") return 2;
	// Check if it's "→" (single character)
	if (line.substr(arrow_pos, 1) == "→") return 1;
	// Check if it's UTF-8 encoded → (three bytes)
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
	if (symbol == "ε" || symbol == "epsilon") return true;
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


std::unique_ptr<parse::grammar> grammar_parser(const std::string& filename) {


	std::unique_ptr<parse::grammar> grammar = std::make_unique<parse::grammar>();

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
			grammar->start_symbol = parse::symbol_t(left_symbol, parse::symbol_type_t::NON_TERMINAL);
		}

		// record the non-terminal
		grammar->non_terminals.insert(parse::symbol_t(left_symbol, parse::symbol_type_t::NON_TERMINAL));

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
			std::vector<parse::symbol_t> right_symbols;
			if (alt == "ε" || alt == "epsilon" || alt.empty()) {
				// empty production
				right_symbols.push_back(grammar->epsilon);
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
					parse::symbol_t symbol;
					if (is_non_terminal(token)) {
						symbol = parse::symbol_t(symbol_name, parse::symbol_type_t::NON_TERMINAL);
						grammar->non_terminals.insert(symbol);
					}
					else {
						symbol = parse::symbol_t(symbol_name, parse::symbol_type_t::TERMINAL);
						if (!(symbol == grammar->epsilon)) {
							grammar->terminals.insert(symbol);
						}
					}
					right_symbols.push_back(symbol);
				}
			}
			// add to the production set
			grammar->add_production(
				parse::symbol_t(left_symbol, parse::symbol_type_t::NON_TERMINAL),
				right_symbols
			);
		}

	}

	file.close();

	return grammar;
}


/*
### Algorithm Features
1. **Iteration until convergence**: Use a `do-while` loop until no new symbols are added to any FIRST set (`changed` is false)
2. **Process the right part of the production**: For each production, check the symbols from left to right:
   - Encounter **terminal symbol**: Add it to the FIRST set of the left non-terminal symbol, and stop processing this production
   - Upon encountering a **non-terminal symbol**: add its FIRST set (excluding ε) to the FIRST set of the left non-terminal symbol
	 - If the FIRST set of the non-terminal symbol contains ε, continue to check the next symbol
	 - Otherwise, stop processing this production rule
3. **Handling the ε case**: If all symbols in the production can derive ε, add ε to the FIRST set of the left nonterminal
*/
void parse::grammar::comp_first_sets() {
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

					if (sym.type == parse::symbol_type_t::TERMINAL) {
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
   - Encountering a **non-terminal symbol**: add its FIRST set (excluding ε) to the result set
	 - If the FIRST set of the non-terminal symbol does not contain ε, stop processing
	 - Otherwise, proceed to the next symbol
2. **Handling ε**: If all symbols in the sequence can derive ε, add the passed-in look-ahead symbol to the result set
*/
std::unordered_set<parse::symbol_t, parse::symbol_hasher> parse::grammar::comp_first_of_sequence(
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


std::shared_ptr<parse::lalr1_item_set> parse::grammar::closure(const parse::lalr1_item_set& I)
{

	if (I.items.empty())
		return std::make_shared<lalr1_item_set>();

	std::shared_ptr<lalr1_item_set> new_I = std::make_shared<lalr1_item_set>(I);
	std::unordered_map<lalr1_item_t, bool, lr0_item_hasher> item_handled;

	bool changed = true;

	do
	{
		changed = false;

		// Copy current items to avoid modification during iteration
		std::unordered_set<lalr1_item_t, lr0_item_hasher>& current_items = new_I->get_items();

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

				// compute beta a (the right part after the non-terminal and the lookaheads)
				std::vector<parse::symbol_t> beta_a;
				for (int i = item.dot_pos + 1; i < item.product->right.size(); i++) {
					beta_a.push_back(item.product->right[i]);
				}

				// compute FIRST(beta a)
				std::unordered_set<parse::symbol_t, parse::symbol_hasher> lookaheads =
					comp_first_of_sequence(beta_a, *item.lookaheads);

				// merge with existing lookaheads
				lookaheads.insert(item.lookaheads->begin(), item.lookaheads->end());

				// add new items for each production of next_sym
				std::vector<std::shared_ptr<parse::production_t>> prods = get_productions_for(next_sym);

				for (const std::shared_ptr<parse::production_t> prod : prods) {

					parse::lalr1_item_t new_item(prod, 0, lookaheads);

					const lalr1_item_t* existing_item = new_I->find_item(new_item.id);

					if (existing_item != nullptr) {
						// Item with same core exists, merge lookaheads
						//size_t before_size = existing_item_iter->lookaheads.size();
						size_t before_size = existing_item->lookaheads->size();

						//gram::lalr1_item merged_item = *existing_item_iter; // Copy existing item
						parse::lalr1_item_t merged_item = *existing_item; // Copy existing item


						//new_I->items.erase(*existing_item); // Remove old item
						merged_item.add_lookaheads(lookaheads);
						//new_I->items.insert(merged_item); // Reinsert modified item
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

#ifdef __DEBUG_OUTPUT__
	//std::cout << "LALR(1) Closure of start state:" << std::endl;
	//std::cout << *new_I << std::endl;
#endif
	return new_I;
}

std::shared_ptr<parse::lalr1_item_set> parse::grammar::go_to(const lalr1_item_set& I, const symbol_t& X)
{

	std::shared_ptr<lalr1_item_set> new_set = std::make_shared<lalr1_item_set>();

	for (const auto& item : I.items) {

		//std::cout << item.to_string() << std::endl;

		if (item.dot_pos < item.product->right.size() &&
			item.next_symbol() == X) {
			parse::lalr1_item_t moved_item(item.product, item.dot_pos + 1, *item.lookaheads);
			new_set->items.insert(moved_item);
		}
	}

	return closure(*new_set);

}


std::shared_ptr<parse::lr0_item_set> parse::grammar::lr0_closure(const lr0_item_set& I) const {

	std::shared_ptr<lr0_item_set> new_I = std::make_shared<lr0_item_set>(I);

	bool changed = true;
	do
	{
		changed = false;

		std::unordered_set<lr0_item_t, lr0_item_hasher> current_items = new_I->get_items(); // Copy current items to avoid modification during iteration

		for (const parse::lr0_item_t& i : current_items) {
			parse::symbol_t next_sym = i.next_symbol();

			if (next_sym.type == parse::symbol_type_t::NON_TERMINAL && !next_sym.name.empty()) {
				// add new items for each production of next_sym
				std::vector<std::shared_ptr<parse::production_t>> prods = get_productions_for(next_sym);
				for (auto& prod : prods) {
					//gram::lr0_item new_item(const_cast<gram::production*>(&prod), 0);
					parse::lr0_item_t new_item(prod, 0);

					// Check if the new item already exists

					const parse::lr0_item_t* found = new_I->find_item(new_item);
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

/*
	return CLOSURE( GOTO[I, X] );
*/
std::shared_ptr<parse::lr0_item_set> parse::grammar::lr0_go_to(const lr0_item_set& I, const symbol_t& X) const
{

	auto result = std::make_shared<lr0_item_set>();

	for (const auto& item : I.get_items()) {
		if (item.next_symbol() == X) {
			parse::lr0_item_t moved_item(item.product, item.dot_pos + 1);
			result->add_items(moved_item);
		}
	}

	if (result->get_items().empty()) {
		return nullptr;
	}

	return lr0_closure(*result);
}

void parse::grammar::build_lr0_states()
{
	//std::vector<std::shared_ptr<gram::lr0_item_set>> lr0_states;

	// Create the augmented grammar
	std::shared_ptr<parse::production_t> augmented_prod = std::make_shared<parse::production_t>(
		parse::symbol_t(start_symbol.name + "'", parse::symbol_type_t::NON_TERMINAL),
		std::vector<parse::symbol_t>{ start_symbol }
	);

	// set the start symbol to the new augmented start symbol
	augmented_prod->id = AUGMENTED_GRAMMAR_PROD_ID;
	productions.insert({ augmented_prod->left, { augmented_prod } });

	// initialize the first item set with the augmented production
	lr0_item_set start_set;
	start_set.id = 0;
	start_set.items.insert(parse::lalr1_item_t(augmented_prod, 0, { end_marker }));

	// compute its closure
	std::shared_ptr<lr0_item_set> closured_start_set = lr0_closure(start_set);
	lr0_states.push_back(closured_start_set);

	for (size_t i = 0; i < lr0_states.size(); i++) {
		std::shared_ptr<parse::lr0_item_set> current_set = lr0_states[i];

		// collect all symbols that can be transitioned on
		std::unordered_set<parse::symbol_t, parse::symbol_hasher> transition_symbols;

		for (const auto& item : current_set->get_items()) {
			parse::symbol_t next_sym = item.next_symbol();
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
					lr0_goto_cache_table[{ current_set->id, symbol }] = static_cast<item_set_id_t>(goto_set->id);
				}
			}
		}

	}

#ifdef __DEBUG_OUTPUT__

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
void parse::grammar::initialize_lalr1_states() {

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

/*
	We apple this routine for each kernel item and each symbol( terminals and non terminals).
*/
void parse::grammar::propagate_lookaheads(
	const lalr1_item_t& i, const parse::symbol_t& X, item_set_id_t set_id,
	std::unordered_map<item_id_t, std::vector<std::pair<item_set_id_t, item_id_t>>>& propagation_graph
)
{
#ifdef __DEBUG_OUTPUT__
	std::cout << "Determining lookaheads for item: " << i.to_string() << " on symbol: " << X.name << " in state: " << set_id << std::endl;
#endif


	lalr1_item_set original_item_set;
	original_item_set.add_items(i);
	std::shared_ptr<lalr1_item_set> J = closure(original_item_set);

	item_set_id_t target_item_set_id = lr0_goto_cache_table[std::make_pair(set_id, X)];
	for (const auto& item : lalr1_states[target_item_set_id]->get_items())
		propagation_graph[i.id].push_back(std::make_pair(target_item_set_id, item.id));


#ifdef __DEBUG_OUTPUT__
	std::cout << "Closure B Items: \n" << J->to_string() << std::endl;
#endif


	for (auto& B : J->get_items()) {


		if (B.dot_pos >= B.product->right.size() || B.next_symbol() != X)
			continue;

		auto goto_B = go_to(*J, X);

#ifdef __DEBUG_OUTPUT__
		std::cout << "Goto B Items: \n" << goto_B->to_string() << std::endl;
#endif

		for (auto& la : *B.lookaheads)
		{

			for (const auto& goto_B_item : goto_B->get_items())
			{

#ifdef __DEBUG_OUTPUT__
				std::cout << "GOTO B item: " << goto_B_item.to_string() << std::endl;

#endif
				if (goto_B_item.product->id == B.product->id && goto_B_item.dot_pos == B.dot_pos + 1)
				{

					auto found = lalr1_states[target_item_set_id]->find_no_const_item(goto_B_item.id);
					if (found != nullptr)
					{

						/*
							We can obtain all the spontaneous generated lookaheads in J through closure( { i } ).
							So, we don't need to compute the first (δ) in [C→γX⋅δ,n]
						*/

						//std::vector<gram::symbol_t> beta_a;
						//for (int i = B.dot_pos + 1; i < B.product->right.size(); i++) {
						//	beta_a.push_back(B.product->right[i]);
						//}
						//std::unordered_set<gram::symbol_t, gram::symbol_hasher> lookaheads =
						//	comp_first_of_sequence(beta_a, *B.lookaheads);
						//lookaheads.insert(la);

						//found->add_lookaheads(lookaheads);
						found->add_lookahead(la);


					}

				}
			}
		}
	}

}

void parse::grammar::set_lalr1_items_lookaheads()
{

	std::unordered_map<item_id_t, std::vector<std::pair<item_set_id_t, item_id_t>>> propagation_graph;

#ifdef __DEBUG_OUTPUT__
	std::cout << "\n\n=============== LALR(1) Parsing Table Building... ===============\n\n" << std::endl;
#endif
	for (item_set_id_t i = 0; i < lalr1_states.size(); i++)
	{
		for (const lalr1_item_t& lalr_i : lalr1_states[i]->get_items())
		{
			for (const auto& X : terminals)
				propagate_lookaheads(lalr_i, X, i,
					propagation_graph
				);

			for (const auto& X : non_terminals)
				propagate_lookaheads(lalr_i, X, i,
					propagation_graph
				);
		}


	}

#ifdef __DEBUG_OUTPUT__
	std::cout << "LALR(1) States Built. Total States: " << lalr1_states.size() << std::endl;
	std::cout << lalr1_states_to_string() << std::endl;
#endif
	//	bool changed = true;
	//
	//	do
	//	{
	//
	//		changed = false;
	//		item_set_id_t i = 0;
	//
	//		// iterate all lalr1 items in all lalr1 states
	//		for (; i < lalr1_states.size(); i++)
	//		{
	//
	//#ifdef __DEBUG_OUTPUT__
	//			std::cout << lalr1_states[i]->to_string() << std::endl;
	//#endif
	//
	//			for (const lalr1_item_t& lalr_i : lalr1_states[i]->get_items())
	//			{
	//				for (auto& [to_item_set_id, item_id] : propagation_graph[lalr_i.id])
	//				{ 
	//					auto found = lalr1_states[to_item_set_id]->find_no_const_item(item_id);
	//					if (*found->lookaheads != *lalr_i.lookaheads)
	//						changed = found->add_lookaheads(*lalr_i.lookaheads);
	//				}
	//
	//			}
	//		}
	//
	//
	//	} while (changed);

#ifdef __DEBUG__
	std::cout << "LALR(1) States Built. Total States: " << lalr1_states.size() << std::endl;
	std::cout << lalr1_states_to_string() << std::endl;
#endif


	/* We don't store non-kernel items. */

	//for (item_set_id_t i = 0; i < lalr1_states.size(); i++) {
	//	auto& state = lalr1_states[i];
	//	auto J = closure(*state);
	//	state->add_items(*J);
	//}

}

void parse::grammar::build_action_table()
{
	for (item_set_id_t i = 0; i < lalr1_states.size(); i++) {
		auto& state = lalr1_states[i];
		auto J = closure(*state);
		for (auto& item : J->get_items())
		//for (auto& item : state->get_items())
		{
			auto& prod = item.product;
			if (item.dot_pos >= prod->right.size())
			{
				for (auto& la : *item.lookaheads) {

					if (action_table[i].count(la)) {
						auto& existing_action = action_table[i][la];
						if (existing_action.type == parser_action_type_t::SHIFT) {
							std::cerr << "Shift-Reduce conflict at state " << i
								<< " on symbol " << la.name
								<< " between shift to " << existing_action.value
								<< " and reduce by production " << get_production_by_id(prod->id) << std::endl;
							throw std::runtime_error("Shift-Reduce conflict detected");
						}
						else if (existing_action.type == parser_action_type_t::REDUCE) {
							std::cerr << "Reduce-Reduce conflict at state " << i
								<< " on symbol " << la.name
								<< " between production " << existing_action.value
								<< " and production " << get_production_by_id(prod->id) << std::endl;
							throw std::runtime_error("Reduce-Reduce conflict detected");
						}

					}

					if (prod->left == start_symbol && la == end_marker)
						// The first production_id must be 0.
						action_table[i][la] = { parser_action_type_t::ACCEPT, AUGMENTED_GRAMMAR_PROD_ID };
					else
						action_table[i][la] = { parser_action_type_t::REDUCE, prod->id };

				}
			}
			// if there is a symbol after the dot (shift action)
			else
			{
				const auto& next_symbol = prod->right[item.dot_pos];

				// if there is a symbol follows closely behind the dot and the symbol is terminal.
				if (next_symbol.type == symbol_type_t::TERMINAL && next_symbol != epsilon) {


					auto goto_key = std::make_pair(i, next_symbol);

					if (lr0_goto_cache_table.find(goto_key) != lr0_goto_cache_table.end())
					{
						// I_j <- GOTO(I_i, a)
						auto next_state = lr0_goto_cache_table[{i, next_symbol}];

						// check whether the action already exists.
						if (action_table[i].count(next_symbol)) {
							auto& existing_action = action_table[i][next_symbol];
							if (existing_action.type != parser_action_type_t::SHIFT || existing_action.value != next_state) {
								std::cerr << "Shift-Shift conflict at state " << i
									<< " on symbol " << next_symbol.name
									<< " between shift to " << existing_action.value
									<< " and shift to " << next_state << std::endl;
								throw std::runtime_error("Shift-Shift conflict detected");
							}
						}
						else
						{
							action_table[i][next_symbol] = { parser_action_type_t::SHIFT, static_cast<parser_action_value_t>(next_state) };
						}
					}

				}

			}

			
			for (auto& entry : lr0_goto_cache_table) {
				if (entry.first.second.type == symbol_type_t::NON_TERMINAL) {
					goto_table[entry.first] = entry.second;
				}
			}
		}
	}

}

void parse::grammar::build() {

#ifdef __DEBUG_OUTPUT__
	std::cout << "\n\n=============== LALR(1) Build Starting... ===============\n\n" << std::endl;
#endif

	build_lr0_states();
	initialize_lalr1_states();
	set_lalr1_items_lookaheads();

	build_action_table();
}


parse::lr_parser::parse_result parse::lr_parser::parse(const std::vector<parse::symbol_t>& input_tokens)
{
	parse_history.clear();

	size_t index = 0;
	std::vector<parse::symbol_t> tokens = input_tokens;
	tokens.push_back(grammar->end_marker);

#ifdef __LALR1_PARSER_HISTORY_INFO__
	parse_history.push_back("Start parsing...");
#endif

	while (true)
	{
		item_set_id_t current_state = state_stack.top();

		parse::symbol_t current_token = tokens[index];

#ifdef __LALR1_PARSER_HISTORY_INFO__
		std::string state_info = " State: " + std::to_string(current_state) + 
			", Input: " + current_token.name +
			", State stack size: " + std::to_string(state_stack.size()) +
			", Symbol stack size: " + std::to_string(symbol_stack.size());

		parse_history.push_back(state_info);
#endif

		
		if (grammar->action_table[current_state].find(current_token) != grammar->action_table[current_state].end()) {

			// If we found the corresponding action.

			parser_action_t a = grammar->action_table[current_state][current_token];

			switch (a.type)
			{
			case parser_action_type_t::SHIFT: {

				state_stack.push(a.value);
				symbol_stack.push(current_token);
				index++;

#ifdef __LALR1_PARSER_HISTORY_INFO__
				parse_history.push_back("Shift to state " + std::to_string(a.value));
#endif

				break;
			}

			case parser_action_type_t::REDUCE: {

				// find the corresponding production.
				auto prod = grammar->get_production_by_id(a.value);

#ifdef __LALR1_PARSER_HISTORY_INFO__
				parse_history.push_back("Reduce: " + prod->to_string());
#endif
				for (size_t i = 0; i < prod->right.size(); i++) {
					if (state_stack.empty() || symbol_stack.empty()) {
						return { false, "fatal: symbol stack empty !", parse_history };
					}
					state_stack.pop();
					symbol_stack.pop();
				}

				if (state_stack.empty()) {
					return { false, "fatal: state stack empty !", parse_history };
				}

				item_set_id_t new_state = state_stack.top();
				symbol_t non_terminal = prod->left;
				auto goto_key = std::make_pair( new_state, non_terminal );
				if (grammar->goto_table.find(goto_key) != grammar->goto_table.end())
				{
					item_set_id_t next_state = grammar->goto_table[goto_key];
					state_stack.push(next_state);
					symbol_stack.push(non_terminal);
#ifdef __LALR1_PARSER_HISTORY_INFO__
					parse_history.push_back("Shift to state: " + std::to_string(next_state));
#endif
				}
				else {
					return { false, "[ " + std::to_string(new_state) + 
									" , " + non_terminal.name + " ] not found in GOTO table."
						, parse_history };
				}

				break;
			}

			case parser_action_type_t::ACCEPT: {

#ifdef __LALR1_PARSER_HISTORY_INFO__
				parse_history.push_back("Accept input.");
#endif
				return { true, "", parse_history };
			}
						

			case parser_action_type_t::ERROR: {
				return { false, "Error action in ACTION table.", parse_history };
			}
			}

		}
		else
		{
			return { false, "ACTION(" + std::to_string(current_state) + 
				", " + current_token.name + ") doesn't have corresponding entry.", parse_history };
		}
	}
	

	return parse_result();
}
