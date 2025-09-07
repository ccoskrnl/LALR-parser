#pragma once
#ifndef __LR_PARSER_H__
#define __LR_PARSER_H__

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

namespace grammar {

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

    struct lr_item {
        production product;
        int dot_pos;
        std::unordered_set<symbol, symbol_hasher> lookaheads;

        lr_item(const production& prod, int dot, const std::unordered_set<symbol, symbol_hasher>& lookahead = {})
            : product(prod), dot_pos(dot), lookaheads(lookahead) {
        }

        bool operator==(const lr_item& other) const {
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
    struct lr_item_hasher {
        size_t operator()(const lr_item& item) const {
            size_t h1 = std::hash<int>()(item.product.id);
            size_t h2 = std::hash<int>()(item.dot_pos);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    class lr_item_set {
    public:
        std::unordered_set<lr_item, lr_item_hasher> items;
        int id;

        lr_item_set(int set_id = -1) : id(set_id) {}

        bool operator==(const lr_item_set& other) const {
            std::set<std::pair<int, int>> core_items, other_core_items;

            for (const lr_item& item : items) {
                core_items.insert({ item.product.id, item.dot_pos });
            }

            for (const lr_item& item : other.items) {
                other_core_items.insert({ item.product.id, item.dot_pos });
            }

            return core_items == other_core_items;
        }
    };

    class grammar {
    public:
        symbol start_symbol;
        symbol epsilon{ "ε", symbol_type::TERMINAL };

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

        void comp_first_sets();
        void comp_follow_sets();

        std::unordered_set<symbol, symbol_hasher> comp_first_of_sequence(
            const std::vector<symbol>& sequence,
            const std::unordered_set<symbol, symbol_hasher>& lookaheads = {}
        );
    };
}

namespace parser {
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
        size_t operator()(const std::pair<int, grammar::symbol>& p) const {
            size_t h1 = std::hash<int>()(p.first);
            size_t h2 = grammar::symbol_hasher()(p.second);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    class lr_parser_table {
    public:

        std::unordered_map<std::pair<int, grammar::symbol>, action, pair_int_symbol_hasher> action_table;
        std::unordered_map<std::pair<int, grammar::symbol>, int, pair_int_symbol_hasher> goto_table;
        //std::unordered_map < std::pair<int, grammar::symbol>, action,
        //    [](const std::pair<int, grammar::symbol>& p) {
        //    return std::hash<int>()(p.first) ^ std::hash<std::string>()(p.second.name);
        //    } > action_table;

        //std::unordered_map < std::pair<int, grammar::symbol>, int,
        //    [](const std::pair<int, grammar::symbol>& p) {
        //    return std::hash<int>()(p.first) ^ std::hash<std::string>()(p.second.name);
        //    } > goto_table;

        void build_slr(const grammar::grammar& grammar, const std::vector<grammar::lr_item_set>& states);
        void build_lalr(grammar::grammar& grammar, std::vector<grammar::lr_item_set>& states);
    };

    class lexer {
    private:
        std::vector<std::pair<std::regex, grammar::symbol>> token_patterns;
        grammar::symbol end_marker{ "$", grammar::symbol_type::TERMINAL };

    public:
        lexer() {
            // 预定义词法规则
            add_token_pattern("\\bint\\b", grammar::symbol("int", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\bfloat\\b", grammar::symbol("float", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\bchar\\b", grammar::symbol("char", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\bbool\\b", grammar::symbol("bool", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\bif\\b", grammar::symbol("if", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\belse\\b", grammar::symbol("else", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\bwhile\\b", grammar::symbol("while", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\breturn\\b", grammar::symbol("return", grammar::symbol_type::TERMINAL));
            add_token_pattern("[a-zA-Z_][a-zA-Z0-9_]*", grammar::symbol("id", grammar::symbol_type::TERMINAL));
            add_token_pattern("[0-9]+", grammar::symbol("int_lit", grammar::symbol_type::TERMINAL));
            add_token_pattern("[0-9]+\\.[0-9]*", grammar::symbol("float_lit", grammar::symbol_type::TERMINAL));
            add_token_pattern("'.'", grammar::symbol("char_lit", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\btrue\\b|\\bfalse\\b", grammar::symbol("bool_lit", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\+", grammar::symbol("+", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\-", grammar::symbol("-", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\*", grammar::symbol("*", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\/", grammar::symbol("/", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\=", grammar::symbol("=", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\==", grammar::symbol("==", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\!=", grammar::symbol("!=", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\<", grammar::symbol("<", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\>", grammar::symbol(">", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\<=", grammar::symbol("<=", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\>=", grammar::symbol(">=", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\&\\&", grammar::symbol("&&", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\|\\|", grammar::symbol("||", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\!", grammar::symbol("!", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\(", grammar::symbol("(", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\)", grammar::symbol(")", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\{", grammar::symbol("{", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\}", grammar::symbol("}", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\;", grammar::symbol(";", grammar::symbol_type::TERMINAL));
            add_token_pattern("\\,", grammar::symbol(",", grammar::symbol_type::TERMINAL));
        }

        void add_token_pattern(const std::string& pattern, const grammar::symbol& symbol) {
            token_patterns.emplace_back(std::regex(pattern), symbol);
        }

        std::vector<std::pair<grammar::symbol, std::string>> tokenize(const std::string& input);
    };

    class lr_parser {
    private:
        lr_parser_table table;
        grammar::grammar grammar;
        std::vector<std::string> error_msg;

    public:
        lr_parser(const grammar::grammar& g, const lr_parser_table& t) : grammar(g), table(t) {}

        bool parse(const std::vector<std::pair<grammar::symbol, std::string>>& tokens);
        const std::vector<std::string>& get_error() const { return error_msg; }

    private:
        grammar::production find_production_by_id(int id) const;

        bool error_recovery(
            std::stack<int>& state_stack,
            std::stack<grammar::symbol>& symbol_stack,
            const std::vector<std::pair<grammar::symbol, std::string>>& tokens,
            size_t& token_index
        );

        void add_error(const std::string& message);
        std::any execute_semantic_action(
            const grammar::production& prod,
            const std::vector<std::any>& children
        );
    };
}

#endif
