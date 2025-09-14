#pragma once
#ifndef __LR_PARSER_H__
#define __LR_PARSER_H__

#include <stdint.h>
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <stack>
#include <memory>
#include <any>
#include <functional>


//typedef uint64_t item_set_id_t;
using item_set_id_t = uint16_t;

using production_id_t = int64_t;
constexpr production_id_t AUGMENTED_GRAMMAR_PROD_ID = 0;


namespace gram {

	enum class action_type_t {
		SHIFT,
		REDUCE,
		ACCEPT,
		ERROR
	};

	struct action_t {
		action_type_t type;
		int value;
		action_t(action_type_t t = action_type_t::ERROR, int v = -1) : type(t), value(v) { }

		bool operator==(const action_t& other) const {
			return type == other.type && value == other.value;
		}
		bool operator!=(const action_t& other) const {
			return !(*this == other);
		}
	};

	static_assert(
		std::is_constructible_v<action_t, action_type_t, int>,
		"action_t must be constructible from action_type_t and int"
	);

	enum class symbol_type_t {
		TERMINAL,
		NON_TERMINAL
	};

	struct symbol_t {
		std::string name;
		symbol_type_t type;

		explicit symbol_t(const std::string& n = "", symbol_type_t t = symbol_type_t::TERMINAL)
			: name(n), type(t) {
		}

		bool operator==(const symbol_t& other) const {
			return name == other.name && type == other.type;
		}

		bool operator!=(const symbol_t& other) const {
			return type != other.type || name != other.name;
		}

		bool operator<(const symbol_t& other) const {
			if (type != other.type) return type < other.type;
			return name < other.name;
		}
	};

	struct symbol_hasher {
		size_t operator()(const symbol_t& s) const {
			size_t h1 = std::hash<std::string>()(s.name);
			size_t h2 = std::hash<int>()(static_cast<int>(s.type));
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	struct production_t {

		static production_id_t prod_id_size;

		symbol_t left;
		std::vector<symbol_t> right;
		production_id_t id;
		action_t action;

		production_t(const symbol_t& l, const std::vector<symbol_t>& r)
			: production_t(l, r, action_t{}) {
		}

		production_t(const symbol_t& l, const std::vector<symbol_t>& r, action_t a = {})
			: left(l), right(r), action(a) {

			id = production_t::prod_id_size++;
		}


		std::string to_string() {
			std::string result = left.name + " -> ";
			for (const auto& sym : right) {
				result += sym.name + " ";
			}

			// TODO
			//if (!action.empty()) {
			//	result += "{" + action + "}";
			//}
			return result;
		}

	};

	struct lr0_item_t {
		std::shared_ptr<production_t> product;
		int dot_pos;
		lr0_item_t(std::shared_ptr<production_t> prod, int dot) : product(prod), dot_pos(dot) {}

		bool operator==(const lr0_item_t& other) const {
			return product->id == other.product->id && dot_pos == other.dot_pos;
		}
		symbol_t next_symbol() const {
			if (dot_pos < product->right.size()) {
				return product->right[dot_pos];
			}
			return symbol_t{ "", symbol_type_t::TERMINAL };
		}

		bool is_kernel_item() const {
			return dot_pos > 0 || product->id == AUGMENTED_GRAMMAR_PROD_ID;
		}

		std::string to_string() const {
			std::string result = product->left.name + " -> ";
			for (size_t i = 0; i < product->right.size(); i++) {
				if (i == dot_pos) {
					result += ". ";
				}
				result += product->right[i].name + " ";
			}
			if (dot_pos == product->right.size()) {
				result += ".";
			}
			return result;
		}
	};

	struct lalr1_item_t : lr0_item_t {

		std::unordered_set<symbol_t, symbol_hasher> lookaheads;

		//std::unordered_map<core_key, std::unordered_set<symbol, symbol_hasher>, core_key_hasher> core_lookaheads;

		lalr1_item_t(std::shared_ptr<production_t> prod, int dot, const std::unordered_set<symbol_t, symbol_hasher>& lookahead = {})
			: lr0_item_t(prod, dot), lookaheads(lookahead) {
		}

		lalr1_item_t(const lr0_item_t& item, const std::unordered_set<symbol_t, symbol_hasher>& lookahead = {})
			: lr0_item_t(item.product, item.dot_pos), lookaheads(lookahead) {
		}

		std::string to_string() const {
			std::string result = product->left.name + " -> ";
			for (size_t i = 0; i < product->right.size(); i++) {
				if (i == dot_pos) {
					result += ". ";
				}
				result += product->right[i].name + " ";
			}
			if (dot_pos == product->right.size()) {
				result += ".";
			}

			result += " , { ";

			for (const auto& la : lookaheads) {
				result += la.name + " ";
			}
			result += "}";
			return result;
		}

	};

	struct lr0_item_hasher {
		size_t operator()(const lr0_item_t& item) const {
			size_t h1 = std::hash<production_id_t>()(item.product->id);
			size_t h2 = std::hash<int>()(item.dot_pos);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	struct lalr1_item_hasher {
		size_t operator()(const lalr1_item_t& item) const {

			size_t h1 = std::hash<production_id_t>()(item.product->id);
			size_t h2 = std::hash<int>()(item.dot_pos);
			size_t h3 = 0;
			for (const auto& sym : item.lookaheads) {
				h3 ^= symbol_hasher()(sym) + 0x9e3779b9 + (h3 << 6) + (h3 >> 2);
			}
			return h1 ^ (h2 << 1);
		}
	};



	class lr0_item_set {

		static item_set_id_t lr0_item_set_size;

	public:
		std::unordered_set<lr0_item_t, lr0_item_hasher> items;
		item_set_id_t id;

		//lr0_item_set(int set_id = -1) : id(set_id) {}
		lr0_item_set() { id = lr0_item_set_size++; }

		bool operator==(const lr0_item_set& other) const {
			std::set<std::pair<production_id_t, int>> core_items, other_core_items;

			for (const lr0_item_t& item : items) {
				core_items.insert({ item.product->id, item.dot_pos });
			}

			for (const lr0_item_t& item : other.items) {
				other_core_items.insert({ item.product->id, item.dot_pos });
			}

			return core_items == other_core_items;
		}
		void add_items(const lr0_item_t& item) {
			this->items.insert(item);
		}
		void add_items(const lr0_item_set& items) {
			for (const auto& i : items.items) {
				this->items.insert(i);
			}
		}

		const lr0_item_t* find_item(const lr0_item_t& core) const {
			for (auto& item : items) {
				if (item.product->id == core.product->id && item.dot_pos == core.dot_pos) {
					//return &const_cast<lr0_item&>(item);
					return &item;
				}
			}
			return nullptr;
		}

		//lr0_item* find_item(production_id_t production_id, int dot_pos) {
		//	for (auto& item : items) {
		//		if (item.product->id == production_id && item.dot_pos == dot_pos) {
		//			return &const_cast<lr0_item&>(item);
		//		}
		//	}
		//	return nullptr;
		//}

		std::unordered_set<lr0_item_t, lr0_item_hasher>& get_items() {
			return items;
		}

		const std::unordered_set<lr0_item_t, lr0_item_hasher>& get_items() const {
			return items;
		}

		friend std::ostream& operator<<(std::ostream& os, const lr0_item_set& item_set) {
			os << "Item Set ID: " << item_set.id << std::endl;
			for (const auto& item : item_set.items) {
				os << "  " << item.to_string() << std::endl;
			}
			return os;
		}

		std::string to_string() const {
			std::string result = "Item Set ID: " + std::to_string(id) + "\n";
			for (const auto& item : items) {
				result += "  " + item.to_string() + "\n";
			}
			return result;
		}

		std::vector<symbol_t> get_transition_symbols() const {
			std::unordered_set<symbol_t, symbol_hasher> symbols;
			for (const auto& item : items) {
				symbol_t next_sym = item.next_symbol();
				if (!next_sym.name.empty()) {
					symbols.insert(next_sym);
				}
			}
			return std::vector<symbol_t>(symbols.begin(), symbols.end());
		}
	};

	class lalr1_item_set {
	public:
		std::unordered_set<lalr1_item_t, lalr1_item_hasher> items;
		item_set_id_t id;

		lalr1_item_set(int set_id = -1) : id(set_id) {}

		bool operator==(const lalr1_item_set& other) const {
			std::set<std::pair<production_id_t, int>> core_items, other_core_items;
			for (const lalr1_item_t& item : items) {
				core_items.insert({ item.product->id, item.dot_pos });
			}
			for (const lalr1_item_t& item : other.items) {
				other_core_items.insert({ item.product->id, item.dot_pos });
			}
			return core_items == other_core_items;
		}
		void add_items(const lalr1_item_t& item) {
			this->items.insert(item);
		}
		void add_items(const lalr1_item_set& items) {
			for (const auto& i : items.items) {
				this->items.insert(i);
			}
		}

		std::unordered_set<lalr1_item_t, lalr1_item_hasher>& get_items() {
			return items;
		}


		const lalr1_item_t* find_item(const lr0_item_t& core) const {
			for (const auto& item : items) {
				if (item.product->id == core.product->id && item.dot_pos == core.dot_pos) {
					//return &const_cast<lalr1_item&>(item);
					return &item;
				}
			}
			return nullptr;
		}

		const std::unordered_set<lalr1_item_t, lalr1_item_hasher>& get_items() const {
			return items;
		}

		friend std::ostream& operator<<(std::ostream& os, const lalr1_item_set& item_set) {

			os << "Item Set ID: " << item_set.id << std::endl;
			for (const auto& item : item_set.items) {
				os << "  " << item.to_string() << std::endl;
			}
			return os;
		}

		std::string to_string() const {
			std::string result = "Item Set ID: " + std::to_string(id) + "\n";
			for (const auto& item : items) {
				result += "  " + item.to_string() + "\n";
			}
			return result;
		}
	};


	struct pair_item_set_symbol_hasher {
		size_t operator()(const std::pair<item_set_id_t, symbol_t>& p) const {
			size_t h1 = std::hash<item_set_id_t>()(p.first);
			size_t h2 = symbol_hasher()(p.second);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	class lalr1_parsing_table {

	private:

		// action table: state x terminal -> action
		std::vector<std::unordered_map<symbol_t, action_t, symbol_hasher>> action_table;

		// goto table: state x non-terminal -> state
		std::vector<std::unordered_map<symbol_t, item_set_id_t, symbol_hasher>> goto_table;

		// production info: production_id -> (left_symbol, right_size)
		std::vector<std::pair<symbol_t, size_t>> productions_info;

	public:

		lalr1_parsing_table(int num_states, const std::vector<production_t>& productions)
		: action_table(num_states), goto_table(num_states) 
		{
			for (const auto& prod : productions) {

				// initialize production info
				productions_info.emplace_back(prod.left, prod.right.size());
			}
		}

		void set_action(item_set_id_t state, const symbol_t& sym, const action_t& action) {
			action_table[state][sym] = action;
		}

		action_t get_action(int state, const symbol_t& sym) const {
			auto it = action_table[state].find(sym);
			if (it != action_table[state].end()) {
				return it->second;
			}
			return action_t(action_type_t::ERROR, -1);
		}

		void set_goto(item_set_id_t state, const symbol_t& sym, item_set_id_t next_state) {
			goto_table[state][sym] = next_state;
		}

		int get_goto(int state, const symbol_t& sym) const {
			auto it = goto_table[state].find(sym);
			if (it != goto_table[state].end()) {
				return it->second;
			}
			return -1;
		}

		//std::pair<symbol_t, size_t> get_production_info(production_id_t prod_id) const {
		//	if (prod_id >= 0 && prod_id < productions_info.size()) {
		//		return productions_info[prod_id];
		//	}
		//	return { symbol_t(), 0 };
		//}

		size_t num_states() const {
			return action_table.size();
		}

		void to_string() const {
			std::cout << "LALR(1) Parsing Table:" << std::endl;
			for (size_t state = 0; state < action_table.size(); state++) {
				std::cout << "State " << state << ":" << std::endl;
				std::cout << "  Actions:" << std::endl;
				for (const auto& [sym, action] : action_table[state]) {
					std::string action_str;
					switch (action.type) {
					case action_type_t::SHIFT:
						action_str = "S" + std::to_string(action.value);
						break;
					case action_type_t::REDUCE:
						action_str = "R" + std::to_string(action.value);
						break;
					case action_type_t::ACCEPT:
						action_str = "ACC";
						break;
					default:
						action_str = "ERR";
						break;
					}
					std::cout << "    " << sym.name << " : " << action_str << std::endl;
				}
				std::cout << "  Gotos:" << std::endl;
				for (const auto& [sym, next_state] : goto_table[state]) {
					std::cout << "    " << sym.name << " : " << next_state << std::endl;
				}
			}
		}

	};


	class grammar {
	public:
		symbol_t start_symbol;
		//symbol epsilon{ "ε", symbol_type::TERMINAL };
		symbol_t epsilon{ "epsilon", symbol_type_t::TERMINAL };
		symbol_t end_marker{ "$", symbol_type_t::TERMINAL };
		symbol_t lookahead_sentinel{ "#", symbol_type_t::TERMINAL };

		std::unordered_map<symbol_t, std::vector<std::shared_ptr<production_t>>, symbol_hasher> productions;
		std::unordered_set<symbol_t, symbol_hasher> terminals;
		std::unordered_set<symbol_t, symbol_hasher> non_terminals;

		std::unordered_map<symbol_t, std::unordered_set<symbol_t, symbol_hasher>, symbol_hasher> first_sets;
		std::unordered_map<symbol_t, std::unordered_set<symbol_t, symbol_hasher>, symbol_hasher> follow_sets;


		std::unordered_map<std::pair<item_set_id_t, symbol_t>, item_set_id_t, pair_item_set_symbol_hasher> goto_table;

		std::vector<std::shared_ptr<lr0_item_set>> lr0_states;
		std::vector<std::shared_ptr<lalr1_item_set>> lalr1_states;


		//grammar() {
		//	terminals.insert(symbol("int", symbol_type::TERMINAL));
		//	terminals.insert(symbol("float", symbol_type::TERMINAL));
		//	terminals.insert(symbol("char", symbol_type::TERMINAL));
		//	terminals.insert(symbol("bool", symbol_type::TERMINAL));
		//	terminals.insert(symbol("id", symbol_type::TERMINAL));
		//	terminals.insert(symbol("int_lit", symbol_type::TERMINAL));
		//	terminals.insert(symbol("float_lit", symbol_type::TERMINAL));
		//	terminals.insert(symbol("char_lit", symbol_type::TERMINAL));
		//	terminals.insert(symbol("bool_lit", symbol_type::TERMINAL));
		//	terminals.insert(symbol("+", symbol_type::TERMINAL));
		//	terminals.insert(symbol("-", symbol_type::TERMINAL));
		//	terminals.insert(symbol("*", symbol_type::TERMINAL));
		//	terminals.insert(symbol("/", symbol_type::TERMINAL));
		//	terminals.insert(symbol("=", symbol_type::TERMINAL));
		//	terminals.insert(symbol("==", symbol_type::TERMINAL));
		//	terminals.insert(symbol("!=", symbol_type::TERMINAL));
		//	terminals.insert(symbol("<", symbol_type::TERMINAL));
		//	terminals.insert(symbol(">", symbol_type::TERMINAL));
		//	terminals.insert(symbol("<=", symbol_type::TERMINAL));
		//	terminals.insert(symbol(">=", symbol_type::TERMINAL));
		//	terminals.insert(symbol("&&", symbol_type::TERMINAL));
		//	terminals.insert(symbol("||", symbol_type::TERMINAL));
		//	terminals.insert(symbol("!", symbol_type::TERMINAL));
		//	terminals.insert(symbol("(", symbol_type::TERMINAL));
		//	terminals.insert(symbol(")", symbol_type::TERMINAL));
		//	terminals.insert(symbol("{", symbol_type::TERMINAL));
		//	terminals.insert(symbol("}", symbol_type::TERMINAL));
		//	terminals.insert(symbol(";", symbol_type::TERMINAL));
		//	terminals.insert(symbol(",", symbol_type::TERMINAL));
		//	terminals.insert(symbol("if", symbol_type::TERMINAL));
		//	terminals.insert(symbol("else", symbol_type::TERMINAL));
		//	terminals.insert(symbol("while", symbol_type::TERMINAL));
		//	terminals.insert(symbol("return", symbol_type::TERMINAL));
		//}

		const std::unordered_set<symbol_t, symbol_hasher>& all_symbols() const {
			return terminals;
		}

		void add_production(const symbol_t& left, const std::vector<symbol_t>& right, const action_t& action = {}) {

			std::shared_ptr<production_t> prod = std::make_shared<production_t>(left, right, action);
			productions[left].push_back(prod);
			non_terminals.insert(left);

			for (const auto& sym : right) {
				if (sym.type == symbol_type_t::TERMINAL && sym.name != epsilon.name) {
					terminals.insert(sym);
				}
				else if (sym.type == symbol_type_t::NON_TERMINAL) {
					non_terminals.insert(sym);
				}
			}
		}

		const std::vector<std::shared_ptr<production_t>> get_productions_for(const symbol_t& symbol) const {
			static std::vector<std::shared_ptr<production_t>> empty;
			auto it = productions.find(symbol);
			if (it != productions.end()) return it->second;
			return empty;
		}

		std::shared_ptr<production_t> get_production_by_id(production_id_t id) const {
			for (const auto& [left, prods] : productions) {
				for (const auto& prod : prods) {
					if (prod->id == id) {
						return prod;
					}
				}
			}
			return nullptr;
		}

		std::shared_ptr<lr0_item_set> lr0_closure(const lr0_item_set& I) const;
		std::shared_ptr<lr0_item_set> lr0_go_to(const lr0_item_set& I, const symbol_t& X) const;
		std::vector<std::shared_ptr<lr0_item_set>> build_lr0_states();

		std::tuple<
			std::unordered_map<lalr1_item_t, symbol_t, lr0_item_hasher>, 
			std::unordered_set<lalr1_item_t, lr0_item_hasher>
		> detemine_lookaheads(
			const std::shared_ptr<gram::lr0_item_set>& I, const gram::symbol_t& X
		);

		std::shared_ptr<lalr1_item_set> closure(const lalr1_item_set& I);
		std::shared_ptr<lalr1_item_set> go_to(const lalr1_item_set& I, const symbol_t& X);

		void comp_first_sets();
		//void comp_follow_sets();
		std::unordered_set<symbol_t, symbol_hasher> comp_first_of_sequence(
			const std::vector<symbol_t>& sequence,
			const std::unordered_set<symbol_t, symbol_hasher>& lookaheads = {}
		);

		void initialize_lalr1_states() {

			lr0_states = build_lr0_states();
			lalr1_states.resize(lr0_states.size());

			lalr1_states[0] = std::make_shared<lalr1_item_set>(0);

			for (const auto& item: lr0_states[0]->get_items()) {
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
					lalr1_item_t la_item(item);
					lalr1_states[i]->add_items(la_item);
				}
			}

			
		}

		void propagate_lookaheads() {
			bool changed;

			std::vector<std::tuple<int, lr0_item_t, int, lr0_item_t>> propagation_edges;

			for (item_set_id_t i = 0; i < lr0_states.size(); i++)
			{
				for (const auto& item: lr0_states[i]->get_items()) {

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

					// compute beta
					std::vector<symbol_t> beta;
					for (int k = item.dot_pos + 1; k < item.product->right.size(); k++) {
						beta.push_back(item.product->right[k]);
					}

					// compute FIRST(beta)
					std::unordered_set<symbol_t, symbol_hasher> first_beta = comp_first_of_sequence(beta);

					// compute lookaheads to propagate
					// use pointer to avoid modifying the original item
					const lalr1_item_t* source_la_item = lalr1_states[i]->find_item(target_item);
					if (source_la_item != nullptr) {
						for (const auto& la : first_beta) {
							if (la != epsilon) {
								target_item.lookaheads.insert(la);
							}
						}

						lalr1_states[j]->add_items(target_item);
					}

					if (first_beta.find(epsilon) != first_beta.end()) {
						propagation_edges.push_back(std::make_tuple(i, item, j, target_item));
					}

				}
			}

			do
			{
				changed = false;

				for (const auto& [from_set_id, from_item, to_set_id, to_item] : propagation_edges) {

					const lalr1_item_t* from_la_item = lalr1_states[from_set_id]->find_item(from_item);
					const lalr1_item_t* to_la_item = lalr1_states[to_set_id]->find_item(to_item);
					if (from_la_item == nullptr || to_la_item == nullptr)
						continue;

					size_t before_size = to_la_item->lookaheads.size();
					// merge lookaheads
					for (const auto& la : from_la_item->lookaheads) {
						if (to_la_item->lookaheads.find(la) == to_la_item->lookaheads.end()) {
							lalr1_item_t updated_to_item = *to_la_item;
							updated_to_item.lookaheads.insert(la);
							lalr1_states[to_set_id]->items.erase(*to_la_item);
							lalr1_states[to_set_id]->add_items(updated_to_item);
						}
					}
					if (to_la_item->lookaheads.size() > before_size) {
						changed = true;
					}
				}

			} while (changed);
		}

		void build() {

			initialize_lalr1_states();
			propagate_lookaheads();


		}

		friend std::ostream& operator<<(std::ostream& os, const grammar& g) {
			for (const auto& [left, prods] : g.productions) {
				for (const auto& prod : prods) {
					os << left.name << " -> ";
					for (const auto& sym : prod->right) {
						os << sym.name << " ";
					}
					//// TODO
					//if (!prod->action.empty()) {
					//	os << "{" << prod->action.to_string() << "}";
					//}
					os << std::endl;
				}
			}
			return os;
		}
	};
}

namespace parse {

	// 为 std::pair<int, grammar::symbol> 定义哈希器
	struct pair_int_symbol_hasher {
		size_t operator()(const std::pair<int, gram::symbol_t>& p) const {
			size_t h1 = std::hash<int>()(p.first);
			size_t h2 = gram::symbol_hasher()(p.second);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	class lr_parser_table {
	public:

		std::unordered_map<std::pair<int, gram::symbol_t>, gram::action_t, pair_int_symbol_hasher> action_table;
		std::unordered_map<std::pair<int, gram::symbol_t>, int, pair_int_symbol_hasher> goto_table;
		//std::unordered_map < std::pair<int, grammar::symbol>, action,
		//    [](const std::pair<int, grammar::symbol>& p) {
		//    return std::hash<int>()(p.first) ^ std::hash<std::string>()(p.second.name);
		//    } > action_table;

		//std::unordered_map < std::pair<int, grammar::symbol>, int,
		//    [](const std::pair<int, grammar::symbol>& p) {
		//    return std::hash<int>()(p.first) ^ std::hash<std::string>()(p.second.name);
		//    } > goto_table;

		void build_slr(const gram::grammar& grammar, const std::vector<gram::lr0_item_set>& states);
		void build_lalr(gram::grammar& grammar, std::vector<gram::lr0_item_set>& states);
	};

	class lexer {
	private:
		std::vector<std::pair<std::regex, gram::symbol_t>> token_patterns;
		gram::symbol_t end_marker{ "$", gram::symbol_type_t::TERMINAL };

	public:
		lexer() {
			// 预定义词法规则
			add_token_pattern("\\bint\\b", gram::symbol_t("int", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\bfloat\\b", gram::symbol_t("float", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\bchar\\b", gram::symbol_t("char", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\bbool\\b", gram::symbol_t("bool", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\bif\\b", gram::symbol_t("if", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\belse\\b", gram::symbol_t("else", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\bwhile\\b", gram::symbol_t("while", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\breturn\\b", gram::symbol_t("return", gram::symbol_type_t::TERMINAL));
			add_token_pattern("[a-zA-Z_][a-zA-Z0-9_]*", gram::symbol_t("id", gram::symbol_type_t::TERMINAL));
			add_token_pattern("[0-9]+", gram::symbol_t("int_lit", gram::symbol_type_t::TERMINAL));
			add_token_pattern("[0-9]+\\.[0-9]*", gram::symbol_t("float_lit", gram::symbol_type_t::TERMINAL));
			add_token_pattern("'.'", gram::symbol_t("char_lit", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\btrue\\b|\\bfalse\\b", gram::symbol_t("bool_lit", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\+", gram::symbol_t("+", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\-", gram::symbol_t("-", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\*", gram::symbol_t("*", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\/", gram::symbol_t("/", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\=", gram::symbol_t("=", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\==", gram::symbol_t("==", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\!=", gram::symbol_t("!=", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\<", gram::symbol_t("<", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\>", gram::symbol_t(">", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\<=", gram::symbol_t("<=", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\>=", gram::symbol_t(">=", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\&\\&", gram::symbol_t("&&", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\|\\|", gram::symbol_t("||", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\!", gram::symbol_t("!", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\(", gram::symbol_t("(", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\)", gram::symbol_t(")", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\{", gram::symbol_t("{", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\}", gram::symbol_t("}", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\;", gram::symbol_t(";", gram::symbol_type_t::TERMINAL));
			add_token_pattern("\\,", gram::symbol_t(",", gram::symbol_type_t::TERMINAL));
		}

		void add_token_pattern(const std::string& pattern, const gram::symbol_t& symbol) {
			token_patterns.emplace_back(std::regex(pattern), symbol);
		}

		std::vector<std::pair<gram::symbol_t, std::string>> tokenize(const std::string& input);
	};

	class lr_parser {
	private:
		lr_parser_table table;
		gram::grammar grammar;
		std::vector<std::string> error_msg;

	public:
		lr_parser(const gram::grammar& g, const lr_parser_table& t) : grammar(g), table(t) {}

		bool parse(const std::vector<std::pair<gram::symbol_t, std::string>>& tokens);
		const std::vector<std::string>& get_error() const { return error_msg; }

	private:
		gram::production_t find_production_by_id(int id) const;

		bool error_recovery(
			std::stack<int>& state_stack,
			std::stack<gram::symbol_t>& symbol_stack,
			const std::vector<std::pair<gram::symbol_t, std::string>>& tokens,
			size_t& token_index
		);

		void add_error(const std::string& message);
		std::any execute_semantic_action(
			const gram::production_t& prod,
			const std::vector<std::any>& children
		);
	};

	class lr_automation_builder {
	public:
		static void build(gram::grammar& grammar,
			std::vector<gram::lr0_item_set>& states,
			lr_parser_table& table,
			bool lalr = false
		);
	};
}

gram::grammar grammar_parser(const std::string& filename);

#endif  // __LR_PARSER_H__
