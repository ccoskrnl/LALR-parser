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

		bool operator<(const symbol& other) const {
			if (type != other.type) return type < other.type;
			return name < other.name;
		}
	};

	// 自定义哈希器
	struct symbol_hasher {
		size_t operator()(const symbol& s) const {
			size_t h1 = std::hash<std::string>()(s.name);
			size_t h2 = std::hash<int>()(static_cast<int>(s.type));
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	struct production {
		symbol left;
		std::vector<symbol> right;
		int id;
		std::string action;

		production(const symbol& l, const std::vector<symbol>& r, int i, const std::string& a = "")
			: left(l), right(r), id(i), action(a) {
		}
	};

	//// core item key
 //   struct core_key {
 //       int production_id;
 //       int dot_position;

 //       core_key(int pid, int dot) : production_id(pid), dot_position(dot) {}

 //       bool operator==(const core_key& other) const {
 //           return production_id == other.production_id && dot_position == other.dot_position;
 //       }

 //       bool operator<(const core_key& other) const {
 //           if (production_id != other.production_id)
 //               return production_id < other.production_id;
 //           return dot_position < other.dot_position;
 //       }
 //   };

	//// core item key hasher
 //   struct core_key_hasher {
 //       size_t operator()(const core_key& key) const {
 //           return std::hash<int>()(key.production_id) ^
 //               (std::hash<int>()(key.dot_position) << 1);
 //       }
 //   };

	struct lalr1_item {
		production product;
		int dot_pos;

		std::unordered_set<symbol, symbol_hasher> lookaheads;

		//std::unordered_map<core_key, std::unordered_set<symbol, symbol_hasher>, core_key_hasher> core_lookaheads;

		lalr1_item(const production& prod, int dot, const std::unordered_set<symbol, symbol_hasher>& lookahead = {})
			: product(prod), dot_pos(dot), lookaheads(lookahead) {
		}

		bool operator==(const lalr1_item& other) const {
			return product.id == other.product.id && dot_pos == other.dot_pos;
		}

		symbol next_symbol() const {
			if (dot_pos < product.right.size()) {
				return product.right[dot_pos];
			}
			return symbol{ "", symbol_type::TERMINAL };
		}
	};

	// 自定义哈希器
	struct lalr1_item_hasher {
		size_t operator()(const lalr1_item& item) const {
			size_t h1 = std::hash<int>()(item.product.id);
			size_t h2 = std::hash<int>()(item.dot_pos);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	class lalr1_item_set {
	public:
		std::unordered_set<lalr1_item, lalr1_item_hasher> items;
		uint64_t id;

		lalr1_item_set(int set_id = -1) : id(set_id) {}

		bool operator==(const lalr1_item_set& other) const {
			std::set<std::pair<int, int>> core_items, other_core_items;

			for (const lalr1_item& item : items) {
				core_items.insert({ item.product.id, item.dot_pos });
			}

			for (const lalr1_item& item : other.items) {
				other_core_items.insert({ item.product.id, item.dot_pos });
			}

			return core_items == other_core_items;
		}
	};


	class grammar {
	public:
		symbol start_symbol;
		//symbol epsilon{ "ε", symbol_type::TERMINAL };
		symbol epsilon{ "epsilon", symbol_type::TERMINAL };

		std::unordered_map<symbol, std::vector<production>, symbol_hasher> productions;
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
			production prod{ left, right, static_cast<int>(productions.size()), action };

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

		const std::vector<production>& get_productions_for(const symbol& symbol) const {
			static std::vector<production> empty;
			auto it = productions.find(symbol);
			if (it != productions.end()) return it->second;
			return empty;
		}


		lalr1_item_set closure(const lalr1_item_set& I);
		lalr1_item_set go_to(const lalr1_item_set& I, const symbol& X);

		void comp_first_sets();
		//void comp_follow_sets();

		std::unordered_set<symbol, symbol_hasher> comp_first_of_sequence(
			const std::vector<symbol>& sequence,
			const std::unordered_set<symbol, symbol_hasher>& lookaheads = {}
		);
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

		void build_slr(const gram::grammar& grammar, const std::vector<gram::lalr1_item_set>& states);
		void build_lalr(gram::grammar& grammar, std::vector<gram::lalr1_item_set>& states);
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
			std::vector<gram::lalr1_item_set>& states,
			lr_parser_table& table,
			bool lalr = false
		);
	};
}

#endif  // __LR_PARSER_H__
