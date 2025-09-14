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


typedef uint64_t item_set_id_t;


#define AUGMENTED_GRAMMAR_PROD_ID 0
typedef int64_t production_id_t;


namespace gram {

	enum class symbol_type {
		TERMINAL,
		NON_TERMINAL
	};

	struct symbol {
		std::string name;
		symbol_type type;

		symbol(const std::string& n = "", symbol_type t = symbol_type::TERMINAL)
			: name(n), type(t) {
		}

		bool operator==(const symbol& other) const {
			return name == other.name && type == other.type;
		}

		bool operator!=(const symbol& other) const {
			return type != other.type || name != other.name;
		}

		bool operator<(const symbol& other) const {
			if (type != other.type) return type < other.type;
			return name < other.name;
		}
	};

	struct symbol_hasher {
		size_t operator()(const symbol& s) const {
			size_t h1 = std::hash<std::string>()(s.name);
			size_t h2 = std::hash<int>()(static_cast<int>(s.type));
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	struct production {

		static production_id_t prod_id_size;

		symbol left;
		std::vector<symbol> right;
		production_id_t id;
		std::string action;

		production(const symbol& l, const std::vector<symbol>& r, const std::string& a = "")
			: left(l), right(r), action(a) {

			id = production::prod_id_size++;
		}

		std::string to_string() {
			std::string result = left.name + " -> ";
			for (const auto& sym : right) {
				result += sym.name + " ";
			}
			if (!action.empty()) {
				result += "{" + action + "}";
			}
			return result;
		}

	};

	struct lr0_item {
		std::shared_ptr<production> product;
		int dot_pos;
		lr0_item(std::shared_ptr<production> prod, int dot) : product(prod), dot_pos(dot) {}

		bool operator==(const lr0_item& other) const {
			return product->id == other.product->id && dot_pos == other.dot_pos;
		}
		symbol next_symbol() const {
			if (dot_pos < product->right.size()) {
				return product->right[dot_pos];
			}
			return symbol{ "", symbol_type::TERMINAL };
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

	struct lalr1_item : lr0_item {

		std::unordered_set<symbol, symbol_hasher> lookaheads;

		//std::unordered_map<core_key, std::unordered_set<symbol, symbol_hasher>, core_key_hasher> core_lookaheads;

		lalr1_item(std::shared_ptr<production> prod, int dot, const std::unordered_set<symbol, symbol_hasher>& lookahead = {})
			: lr0_item(prod, dot), lookaheads(lookahead) {
		}

		lalr1_item(const lr0_item& item, const std::unordered_set<symbol, symbol_hasher>& lookahead = {})
			: lr0_item(item.product, item.dot_pos), lookaheads(lookahead) {
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

	struct lr_item_hasher {
		size_t operator()(const lr0_item& item) const {
			size_t h1 = std::hash<production_id_t>()(item.product->id);
			size_t h2 = std::hash<int>()(item.dot_pos);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};



	class lr0_item_set {
	public:
		std::unordered_set<lr0_item, lr_item_hasher> items;
		item_set_id_t id;

		lr0_item_set(int set_id = -1) : id(set_id) {}

		bool operator==(const lr0_item_set& other) const {
			std::set<std::pair<production_id_t, int>> core_items, other_core_items;

			for (const lr0_item& item : items) {
				core_items.insert({ item.product->id, item.dot_pos });
			}

			for (const lr0_item& item : other.items) {
				other_core_items.insert({ item.product->id, item.dot_pos });
			}

			return core_items == other_core_items;
		}
		void add_items(const lr0_item& item) {
			this->items.insert(item);
		}
		void add_items(const lr0_item_set& items) {
			for (const auto& i : items.items) {
				this->items.insert(i);
			}
		}
		lr0_item* find_item(production_id_t production_id, int dot_pos) {
			for (auto& item : items) {
				if (item.product->id == production_id && item.dot_pos == dot_pos) {
					return &const_cast<lr0_item&>(item);
				}
			}
			return nullptr;
		}

		std::unordered_set<lr0_item, lr_item_hasher>& get_items() {
			return items;
		}

		const std::unordered_set<lr0_item, lr_item_hasher>& get_items() const {
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

		std::vector<symbol> get_transition_symbols() const {
			std::unordered_set<symbol, symbol_hasher> symbols;
			for (const auto& item : items) {
				symbol next_sym = item.next_symbol();
				if (!next_sym.name.empty()) {
					symbols.insert(next_sym);
				}
			}
			return std::vector<symbol>(symbols.begin(), symbols.end());
		}
	};

	class lalr1_item_set {
	public:
		std::unordered_set<lalr1_item, lr_item_hasher> items;
		item_set_id_t id;

		lalr1_item_set(int set_id = -1) : id(set_id) {}

		bool operator==(const lalr1_item_set& other) const {
			std::set<std::pair<production_id_t, int>> core_items, other_core_items;
			for (const lalr1_item& item : items) {
				core_items.insert({ item.product->id, item.dot_pos });
			}
			for (const lalr1_item& item : other.items) {
				other_core_items.insert({ item.product->id, item.dot_pos });
			}
			return core_items == other_core_items;
		}
		void add_items(const lalr1_item& item) {
			this->items.insert(item);
		}
		void add_items(const lalr1_item_set& items) {
			for (const auto& i : items.items) {
				this->items.insert(i);
			}
		}
		lalr1_item* find_item(production_id_t production_id, int dot_pos) {
			for (auto& item : items) {
				if (item.product->id == production_id && item.dot_pos == dot_pos) {
					return &const_cast<lalr1_item&>(item);
				}
			}
			return nullptr;
		}

		std::unordered_set<lalr1_item, lr_item_hasher>& get_items() {
			return items;
		}

		const std::unordered_set<lalr1_item, lr_item_hasher>& get_items() const {
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



	class grammar {
	public:
		symbol start_symbol;
		//symbol epsilon{ "ε", symbol_type::TERMINAL };
		symbol epsilon{ "epsilon", symbol_type::TERMINAL };
		symbol end_marker{ "$", symbol_type::TERMINAL };
		symbol lookahead_sentinel{ "#", symbol_type::TERMINAL };

		std::unordered_map<symbol, std::vector<std::shared_ptr<production>>, symbol_hasher> productions;
		std::unordered_set<symbol, symbol_hasher> terminals;
		std::unordered_set<symbol, symbol_hasher> non_terminals;

		std::unordered_map<symbol, std::unordered_set<symbol, symbol_hasher>, symbol_hasher> first_sets;
		std::unordered_map<symbol, std::unordered_set<symbol, symbol_hasher>, symbol_hasher> follow_sets;

		grammar() {
			// 添加预定义的终结符
			terminals.insert(symbol("int", symbol_type::TERMINAL));
			terminals.insert(symbol("float", symbol_type::TERMINAL));
			terminals.insert(symbol("char", symbol_type::TERMINAL));
			terminals.insert(symbol("bool", symbol_type::TERMINAL));
			terminals.insert(symbol("id", symbol_type::TERMINAL));
			terminals.insert(symbol("int_lit", symbol_type::TERMINAL));
			terminals.insert(symbol("float_lit", symbol_type::TERMINAL));
			terminals.insert(symbol("char_lit", symbol_type::TERMINAL));
			terminals.insert(symbol("bool_lit", symbol_type::TERMINAL));
			terminals.insert(symbol("+", symbol_type::TERMINAL));
			terminals.insert(symbol("-", symbol_type::TERMINAL));
			terminals.insert(symbol("*", symbol_type::TERMINAL));
			terminals.insert(symbol("/", symbol_type::TERMINAL));
			terminals.insert(symbol("=", symbol_type::TERMINAL));
			terminals.insert(symbol("==", symbol_type::TERMINAL));
			terminals.insert(symbol("!=", symbol_type::TERMINAL));
			terminals.insert(symbol("<", symbol_type::TERMINAL));
			terminals.insert(symbol(">", symbol_type::TERMINAL));
			terminals.insert(symbol("<=", symbol_type::TERMINAL));
			terminals.insert(symbol(">=", symbol_type::TERMINAL));
			terminals.insert(symbol("&&", symbol_type::TERMINAL));
			terminals.insert(symbol("||", symbol_type::TERMINAL));
			terminals.insert(symbol("!", symbol_type::TERMINAL));
			terminals.insert(symbol("(", symbol_type::TERMINAL));
			terminals.insert(symbol(")", symbol_type::TERMINAL));
			terminals.insert(symbol("{", symbol_type::TERMINAL));
			terminals.insert(symbol("}", symbol_type::TERMINAL));
			terminals.insert(symbol(";", symbol_type::TERMINAL));
			terminals.insert(symbol(",", symbol_type::TERMINAL));
			terminals.insert(symbol("if", symbol_type::TERMINAL));
			terminals.insert(symbol("else", symbol_type::TERMINAL));
			terminals.insert(symbol("while", symbol_type::TERMINAL));
			terminals.insert(symbol("return", symbol_type::TERMINAL));
		}

		void add_production(const symbol& left, const std::vector<symbol>& right, const std::string& action = "") {

			std::shared_ptr<production> prod = std::make_shared<production>(left, right, action);
			productions[left].push_back(prod);
			non_terminals.insert(left);

			for (const auto& sym : right) {
				if (sym.type == symbol_type::TERMINAL && sym.name != epsilon.name) {
					terminals.insert(sym);
				}
				else if (sym.type == symbol_type::NON_TERMINAL) {
					non_terminals.insert(sym);
				}
			}
		}

		const std::vector<std::shared_ptr<production>> get_productions_for(const symbol& symbol) const {
			static std::vector<std::shared_ptr<production>> empty;
			auto it = productions.find(symbol);
			if (it != productions.end()) return it->second;
			return empty;
		}

		std::shared_ptr<production> get_production_by_id(production_id_t id) const {
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
		std::shared_ptr<lr0_item_set> lr0_go_to(const lr0_item_set& I, const symbol& X) const;
		std::vector<std::shared_ptr<lr0_item_set>> build_lr0_states();

		std::tuple<
			std::unordered_map<lalr1_item, symbol, lr_item_hasher>, 
			std::unordered_set<lalr1_item, lr_item_hasher>
		> detemine_lookaheads(
			const std::shared_ptr<gram::lr0_item_set>& I, const gram::symbol& X
		);

		std::shared_ptr<lalr1_item_set> closure(const lalr1_item_set& I);
		std::shared_ptr<lalr1_item_set> go_to(const lalr1_item_set& I, const symbol& X);

		void comp_first_sets();
		//void comp_follow_sets();
		std::unordered_set<symbol, symbol_hasher> comp_first_of_sequence(
			const std::vector<symbol>& sequence,
			const std::unordered_set<symbol, symbol_hasher>& lookaheads = {}
		);

		friend std::ostream& operator<<(std::ostream& os, const grammar& g) {
			for (const auto& [left, prods] : g.productions) {
				for (const auto& prod : prods) {
					os << left.name << " -> ";
					for (const auto& sym : prod->right) {
						os << sym.name << " ";
					}
					if (!prod->action.empty()) {
						os << "{" << prod->action << "}";
					}
					os << std::endl;
				}
			}
			return os;
		}
	};

}

namespace parse {
	enum class action_type {
		SHIFT,
		REDUCE,
		ACCEPT,
		ERROR
	};

	struct action {
		action_type type;
		int value;
		action(action_type t = action_type::ERROR, int v = -1) : type(t), value(v) {};
	};

	// 为 std::pair<int, grammar::symbol> 定义哈希器
	struct pair_int_symbol_hasher {
		size_t operator()(const std::pair<int, gram::symbol>& p) const {
			size_t h1 = std::hash<int>()(p.first);
			size_t h2 = gram::symbol_hasher()(p.second);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	class lr_parser_table {
	public:

		std::unordered_map<std::pair<int, gram::symbol>, action, pair_int_symbol_hasher> action_table;
		std::unordered_map<std::pair<int, gram::symbol>, int, pair_int_symbol_hasher> goto_table;
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
		std::vector<std::pair<std::regex, gram::symbol>> token_patterns;
		gram::symbol end_marker{ "$", gram::symbol_type::TERMINAL };

	public:
		lexer() {
			// 预定义词法规则
			add_token_pattern("\\bint\\b", gram::symbol("int", gram::symbol_type::TERMINAL));
			add_token_pattern("\\bfloat\\b", gram::symbol("float", gram::symbol_type::TERMINAL));
			add_token_pattern("\\bchar\\b", gram::symbol("char", gram::symbol_type::TERMINAL));
			add_token_pattern("\\bbool\\b", gram::symbol("bool", gram::symbol_type::TERMINAL));
			add_token_pattern("\\bif\\b", gram::symbol("if", gram::symbol_type::TERMINAL));
			add_token_pattern("\\belse\\b", gram::symbol("else", gram::symbol_type::TERMINAL));
			add_token_pattern("\\bwhile\\b", gram::symbol("while", gram::symbol_type::TERMINAL));
			add_token_pattern("\\breturn\\b", gram::symbol("return", gram::symbol_type::TERMINAL));
			add_token_pattern("[a-zA-Z_][a-zA-Z0-9_]*", gram::symbol("id", gram::symbol_type::TERMINAL));
			add_token_pattern("[0-9]+", gram::symbol("int_lit", gram::symbol_type::TERMINAL));
			add_token_pattern("[0-9]+\\.[0-9]*", gram::symbol("float_lit", gram::symbol_type::TERMINAL));
			add_token_pattern("'.'", gram::symbol("char_lit", gram::symbol_type::TERMINAL));
			add_token_pattern("\\btrue\\b|\\bfalse\\b", gram::symbol("bool_lit", gram::symbol_type::TERMINAL));
			add_token_pattern("\\+", gram::symbol("+", gram::symbol_type::TERMINAL));
			add_token_pattern("\\-", gram::symbol("-", gram::symbol_type::TERMINAL));
			add_token_pattern("\\*", gram::symbol("*", gram::symbol_type::TERMINAL));
			add_token_pattern("\\/", gram::symbol("/", gram::symbol_type::TERMINAL));
			add_token_pattern("\\=", gram::symbol("=", gram::symbol_type::TERMINAL));
			add_token_pattern("\\==", gram::symbol("==", gram::symbol_type::TERMINAL));
			add_token_pattern("\\!=", gram::symbol("!=", gram::symbol_type::TERMINAL));
			add_token_pattern("\\<", gram::symbol("<", gram::symbol_type::TERMINAL));
			add_token_pattern("\\>", gram::symbol(">", gram::symbol_type::TERMINAL));
			add_token_pattern("\\<=", gram::symbol("<=", gram::symbol_type::TERMINAL));
			add_token_pattern("\\>=", gram::symbol(">=", gram::symbol_type::TERMINAL));
			add_token_pattern("\\&\\&", gram::symbol("&&", gram::symbol_type::TERMINAL));
			add_token_pattern("\\|\\|", gram::symbol("||", gram::symbol_type::TERMINAL));
			add_token_pattern("\\!", gram::symbol("!", gram::symbol_type::TERMINAL));
			add_token_pattern("\\(", gram::symbol("(", gram::symbol_type::TERMINAL));
			add_token_pattern("\\)", gram::symbol(")", gram::symbol_type::TERMINAL));
			add_token_pattern("\\{", gram::symbol("{", gram::symbol_type::TERMINAL));
			add_token_pattern("\\}", gram::symbol("}", gram::symbol_type::TERMINAL));
			add_token_pattern("\\;", gram::symbol(";", gram::symbol_type::TERMINAL));
			add_token_pattern("\\,", gram::symbol(",", gram::symbol_type::TERMINAL));
		}

		void add_token_pattern(const std::string& pattern, const gram::symbol& symbol) {
			token_patterns.emplace_back(std::regex(pattern), symbol);
		}

		std::vector<std::pair<gram::symbol, std::string>> tokenize(const std::string& input);
	};

	class lr_parser {
	private:
		lr_parser_table table;
		gram::grammar grammar;
		std::vector<std::string> error_msg;

	public:
		lr_parser(const gram::grammar& g, const lr_parser_table& t) : grammar(g), table(t) {}

		bool parse(const std::vector<std::pair<gram::symbol, std::string>>& tokens);
		const std::vector<std::string>& get_error() const { return error_msg; }

	private:
		gram::production find_production_by_id(int id) const;

		bool error_recovery(
			std::stack<int>& state_stack,
			std::stack<gram::symbol>& symbol_stack,
			const std::vector<std::pair<gram::symbol, std::string>>& tokens,
			size_t& token_index
		);

		void add_error(const std::string& message);
		std::any execute_semantic_action(
			const gram::production& prod,
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
