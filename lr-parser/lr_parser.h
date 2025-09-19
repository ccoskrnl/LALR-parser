#pragma once
#ifndef __LR_PARSER_H__
#define __LR_PARSER_H__

#include "framework.h"

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
#include <sstream>
#include <iomanip>

//typedef uint64_t item_set_id_t;
using item_set_id_t = int32_t;
using item_id_t = uint64_t;
using parser_action_value_t = int32_t;
using production_id_t = parser_action_value_t;
constexpr production_id_t AUGMENTED_GRAMMAR_PROD_ID = 0;

static_assert(std::is_same_v < parser_action_value_t, production_id_t> , "action_value_t and production_id_t must be the same type");

namespace parse {

	enum class parser_action_type_t {
		SHIFT,
		REDUCE,
		ACCEPT,
		ERROR
	};

	struct parser_action_t {
		parser_action_type_t type;
		parser_action_value_t value;
		parser_action_t(parser_action_type_t t = parser_action_type_t::ERROR, parser_action_value_t v = -1) : type(t), value(v) { }

		bool operator==(const parser_action_t& other) const {
			return type == other.type && value == other.value;
		}
		bool operator!=(const parser_action_t& other) const {
			return !(*this == other);
		}

		std::string to_string() const {
			std::stringstream ss;

			switch (type) {
			case parser_action_type_t::SHIFT:
				ss << "SHIFT(" << value << ")";
				break;
			case parser_action_type_t::REDUCE:
				ss << "REDUCE(" << value << ")";
				break;
			case parser_action_type_t::ACCEPT:
				ss << "ACCEPT";
				break;
			case parser_action_type_t::ERROR:
				ss << "ERROR";
				break;
			default:
				ss << "UNKNOWN";
				break;
			}

			return ss.str();
		}
	};

	static_assert(
		std::is_constructible_v<parser_action_t, parser_action_type_t, int>,
		"action_t must be constructible from action_type_t and int"
	);




	enum class symbol_type_t {
		TERMINAL,
		NON_TERMINAL,
		EPSILON
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

	struct symbol_less {
		bool operator()(const parse::symbol_t& a, const parse::symbol_t& b) const {
			return a < b;
		}
	};

	struct production_t {

		static production_id_t prod_id_size;

		symbol_t left;
		std::vector<symbol_t> right;
		production_id_t id;

		production_t(const symbol_t& l, const std::vector<symbol_t>& r)
			: left(l), right(r){

			id = production_t::prod_id_size++;
		}


		std::string to_string() {
			std::string result = "[ID: " + std::to_string(id) + " ]  " + left.name + " -> ";
			for (const auto& sym : right) {
				result += sym.name + " ";
			}

			return result;
		}

	};

	struct lr0_item_t {

		std::shared_ptr<production_t> product;
		int dot_pos;
		item_id_t id;

		
		int get_dot_pos() const {
			return dot_pos;
		}

		item_id_t get_id() const {
			return id;
		}

		production_id_t get_production_id() const {
			return product->id;
		}

		void set_dot_pos(int new_dot_pos){
			dot_pos = new_dot_pos;
			id = (static_cast<item_id_t>(product->id) << 32) | (static_cast<item_id_t>(dot_pos) & 0xFFFFFFFF);
		}

		const std::shared_ptr<production_t> get_production() const {
			return product;
		}

		lr0_item_t(std::shared_ptr<production_t> prod, int dot) : product(prod), dot_pos(dot) {
			id = (static_cast<item_id_t>(product->id) << 32) | (static_cast<item_id_t>(dot_pos) & 0xFFFFFFFF);
		}

		bool operator==(const lr0_item_t& other) const {
			return product->id == other.product->id && dot_pos == other.dot_pos;
		}
		symbol_t next_symbol() const {
			if (dot_pos < product->right.size()) {
				return product->right[dot_pos];
			}
			return symbol_t{ "", symbol_type_t::EPSILON };
		}

		symbol_t current_symbol() const {
			if (dot_pos > 0)
				return product->right[dot_pos - 1];
			return symbol_t{ "", symbol_type_t::TERMINAL };
		}

		bool is_kernel_item() const {
			return dot_pos > 0 || product->id == AUGMENTED_GRAMMAR_PROD_ID;
		}

		std::string to_string() const {

			std::string result = "[ID: " + std::to_string(product->id) + " ]  " + product->left.name + " -> ";
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

		std::shared_ptr<std::unordered_set<symbol_t, symbol_hasher>> lookaheads;

		lalr1_item_t(std::shared_ptr<production_t> prod, int dot)
			: lr0_item_t(prod, dot), lookaheads(std::make_shared<std::unordered_set<symbol_t, symbol_hasher>>()) {
		}

		lalr1_item_t(const lr0_item_t& item)
			: lr0_item_t(item.product, item.dot_pos), lookaheads(std::make_shared<std::unordered_set<symbol_t, symbol_hasher>>()) {
		}
		
		lalr1_item_t(const lalr1_item_t& item)
			: lr0_item_t(item.product, item.dot_pos), lookaheads(std::make_shared<std::unordered_set<symbol_t, symbol_hasher>>()) {
			this->add_lookaheads(*item.lookaheads);
		}

		lalr1_item_t(std::shared_ptr<production_t> prod, int dot, const std::unordered_set<symbol_t, symbol_hasher>& lookahead)
			: lr0_item_t(prod, dot), lookaheads(std::make_shared<std::unordered_set<symbol_t, symbol_hasher>>(lookahead)) {
		}

		lalr1_item_t(const lr0_item_t& item, const std::unordered_set<symbol_t, symbol_hasher>& lookahead)
			: lr0_item_t(item.product, item.dot_pos), lookaheads(std::make_shared<std::unordered_set<symbol_t, symbol_hasher>>(lookahead)) {
		}

		lalr1_item_t()
			: lr0_item_t(nullptr, 0), lookaheads(std::make_shared<std::unordered_set<symbol_t, symbol_hasher>>()) {
		}


		bool add_lookaheads(const std::unordered_set<symbol_t, symbol_hasher>& las) {
			if (las.empty())
				return false;
			const size_t old_size = lookaheads->size();
			lookaheads->insert(las.begin(), las.end());
			return old_size != lookaheads->size();
		}

		bool add_lookahead(const symbol_t& la) {
			const size_t old_size = lookaheads->size();
			lookaheads->insert(la);
			return old_size != lookaheads->size();
		}

		void del_lookahead(const symbol_t& la) {
			lookaheads->erase(la);
		}

		void del_lookaheads(const std::unordered_set<symbol_t, symbol_hasher>& las) {
			for (const auto& la : las) {
				lookaheads->erase(la);
			}
		}

		std::string to_string() const {
			std::string result = "[ID: " + std::to_string(product->id) + " ]  " + product->left.name + " -> ";
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

			for (const auto& la : *lookaheads) {
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
			for (const auto& sym : *item.lookaheads) {
				h3 ^= symbol_hasher()(sym) + 0x9e3779b9 + (h3 << 6) + (h3 >> 2);
			}
			return h1 ^ (h2 << 1);
		}
	private:
		// ���������ϲ���ϣֵ
		template <typename T>
		void hash_combine(size_t& seed, const T& val) const {
			std::hash<T> hasher;
			seed ^= hasher(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
	};

	//bool operator==(const lalr1_item_t& lhs, const lalr1_item_t& rhs) {
	//	return lhs.product->id == rhs.product->id &&
	//		lhs.dot_pos == rhs.dot_pos &&
	//		lhs.id == rhs.id &&
	//		(lhs.lookaheads == rhs.lookaheads ||
	//			(lhs.lookaheads && rhs.lookaheads &&
	//				*lhs.lookaheads == *rhs.lookaheads));
	//};

	struct pair_item_id_symbol_hasher {
		size_t operator()(const std::pair<item_id_t, symbol_t>& p) const {
			size_t h1 = std::hash<item_id_t>()(p.first);
			size_t h2 = symbol_hasher()(p.second);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	struct pair_items_state_item_id_hasher {
		size_t operator()(const std::pair<item_set_id_t, item_id_t>& p) const {
			size_t h1 = std::hash<item_set_id_t>()(p.first);
			size_t h2 = std::hash<item_id_t>()(p.second);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};


	class lr0_item_set {

	public:
		std::unordered_set<lr0_item_t, lr0_item_hasher> items;
		item_set_id_t id;

		lr0_item_set(int set_id = -1) : id(set_id) {}
		lr0_item_set(const lr0_item_set& other) : id(other.id), items(other.items) {}

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

		bool empty() const {
			return this->items.empty();
		}


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
		lalr1_item_set(const lalr1_item_set& other) : id(other.id), items(other.items) {}

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
		bool del_items(const lalr1_item_t& item) {
			return remove_item(item);
		}
		bool del_items(const lalr1_item_set& items) {
			bool result = false;
			for (const auto& i : items.items) {
				result = remove_item(i);
			}
			return result;
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

		const lalr1_item_t* find_item(const item_id_t& id) const {
			for (const auto& item : items) {
				if (id == item.id)
					return &item;
			}
			return nullptr;
		}
		
		lalr1_item_t* find_mutable_item(const lr0_item_t& core){
			for (auto& item : items) {
				if (item.product->id == core.product->id && item.dot_pos == core.dot_pos) {
					return &const_cast<lalr1_item_t&>(item);
					//return item;
				}
			}
			return nullptr;
		}

		lalr1_item_t* find_mutable_item(const item_id_t id){
			for (auto& item : items) {
				if (id == item.id) {
					return &const_cast<lalr1_item_t&>(item);
					//return item;
				}
			}
			return nullptr;
		}

		bool empty() const {
			return this->items.empty();
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

	private:
		bool remove_item(const lalr1_item_t& target) {
			// ���ȳ���ֱ��ɾ��
			if (items.erase(target) > 0) return true;

			// ���ʧ�ܣ�����������ͬ����
			auto it = std::find_if(items.begin(), items.end(), [&](const auto& item) {
				return item.product->id == target.product->id &&
					item.dot_pos == target.dot_pos &&
					item.id == target.id &&
					item.lookaheads && target.lookaheads &&
					*item.lookaheads == *target.lookaheads;
				});

			if (it != items.end()) {
				items.erase(it);
				return true;
			}

			return false;
		}


	};


	struct pair_item_set_symbol_hasher {
		size_t operator()(const std::pair<item_set_id_t, symbol_t>& p) const {
			size_t h1 = std::hash<item_set_id_t>()(p.first);
			size_t h2 = symbol_hasher()(p.second);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};


	class grammar {
	public:
		symbol_t start_symbol;

		//symbol epsilon{ "��", symbol_type::TERMINAL };

		symbol_t epsilon{ "", symbol_type_t::EPSILON };
		symbol_t end_marker{ "$", symbol_type_t::TERMINAL };
		symbol_t lookahead_sentinel{ "#", symbol_type_t::TERMINAL };

		std::unordered_map<symbol_t, std::vector<std::shared_ptr<production_t>>, symbol_hasher> productions;
		std::unordered_set<symbol_t, symbol_hasher> terminals;
		std::unordered_set<symbol_t, symbol_hasher> non_terminals;

		std::unordered_map<symbol_t, std::unordered_set<symbol_t, symbol_hasher>, symbol_hasher> first_sets;
		std::unordered_map<symbol_t, std::unordered_set<symbol_t, symbol_hasher>, symbol_hasher> follow_sets;



		std::unordered_map<item_set_id_t, std::unordered_map<parse::symbol_t, parse::parser_action_t, parse::symbol_hasher>> action_table;
		std::unordered_map<std::pair<item_set_id_t, symbol_t>, item_set_id_t, pair_item_set_symbol_hasher> goto_table;

		std::vector<std::shared_ptr<lr0_item_set>> lr0_states;
		std::unordered_map<std::pair<item_set_id_t, symbol_t>, item_set_id_t, pair_item_set_symbol_hasher> lr0_goto_cache_table;
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

		void add_production(const symbol_t& left, const std::vector<symbol_t>& right) {

			std::shared_ptr<production_t> prod = std::make_shared<production_t>(left, right);
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
		std::shared_ptr<lalr1_item_set> closure(const lalr1_item_set& I);
		std::shared_ptr<lalr1_item_set> go_to(const lalr1_item_set& I, const symbol_t& X);

		void comp_first_sets();
		std::unordered_set<symbol_t, symbol_hasher> comp_first_of_sequence(
			const std::vector<symbol_t>& sequence,
			const std::unordered_set<symbol_t, symbol_hasher>& lookaheads = {}
		);

		void build_lr0_states();
		void initialize_lalr1_states();

		void propagate_lookaheads(
			const lalr1_item_t& i, const parse::symbol_t& X, item_set_id_t set_id,
			std::unordered_map<item_id_t, std::vector<std::pair<item_set_id_t, item_id_t>>>& propagation_graph,
			std::unordered_map<item_id_t, std::unordered_set<parse::symbol_t, parse::symbol_hasher>>& spontaneous_la
		);

		void determine_lookaheads(
			const item_set_id_t I_id,
			const symbol_t X,
			std::unordered_map<std::pair<item_set_id_t, item_id_t>, std::vector<std::pair<item_set_id_t, item_id_t>>, pair_items_state_item_id_hasher>& propagation_graph,
			std::unordered_map<std::pair<item_set_id_t, item_id_t>, std::unordered_set<parse::symbol_t, parse::symbol_hasher>, pair_items_state_item_id_hasher>& spontaneous_lookaheads
		);

		void set_lalr1_items_lookaheads();

		void build_action_table();

		void build();

		std::string productions_to_string() const {
			std::string result;
			for (const auto& sym : productions) {
				result += "Symbol: " + sym.first.name + "\n";
				for (const auto& prod : sym.second) {
					result += "\t" + prod->to_string() + "\n";
				}
				
			}
			return result;
		}

		std::string lalr1_states_to_string() {
			std::string result;
			for (const std::shared_ptr<lalr1_item_set> state : lalr1_states) {
				result += closure(*state)->to_string() + "\n";
			}
			return result;
		}

		std::string action_table_to_string() const {
			std::stringstream ss;

			// �ռ�����״̬ID������
			std::vector<item_set_id_t> state_ids;
			for (const auto& state_entry : action_table) {
				state_ids.push_back(state_entry.first);
			}
			std::sort(state_ids.begin(), state_ids.end());

			// �ռ����з��Ų����򣨰����ƣ�
			std::vector<parse::symbol_t> symbols;
			std::unordered_set<std::string> symbol_names;

			for (const auto& state_entry : action_table) {
				for (const auto& action_entry : state_entry.second) {
					if (symbol_names.find(action_entry.first.name) == symbol_names.end()) {
						symbols.push_back(action_entry.first);
						symbol_names.insert(action_entry.first.name);
					}
				}
			}

			// ��������������
			std::sort(symbols.begin(), symbols.end(),
				[](const parse::symbol_t& a, const parse::symbol_t& b) {
					return a.name < b.name;
				});

			// �����ͷ
			ss << "ACTION Table:\n";
			ss << std::setw(8) << "State";
			for (const auto& sym : symbols) {
				ss << std::setw(12) << sym.name;
			}
			ss << "\n";

			// ���ÿ��״̬����
			for (const auto& state_id : state_ids) {
				ss << std::setw(8) << state_id;

				for (const auto& sym : symbols) {
					auto state_it = action_table.find(state_id);
					if (state_it != action_table.end()) {
						auto action_it = state_it->second.find(sym);
						if (action_it != state_it->second.end()) {
							ss << std::setw(12) << action_it->second.to_string();
						}
						else {
							ss << std::setw(12) << "";
						}
					}
					else {
						ss << std::setw(12) << "";
					}
				}
				ss << "\n";
			}

			return ss.str();
		}

		std::string action_table_to_string_detailed() const {
			std::stringstream ss;

			ss << "Detailed ACTION Table:\n";
			ss << "======================\n";

			// �ռ�����״̬ID������
			std::vector<item_set_id_t> state_ids;
			for (const auto& state_entry : action_table) {
				state_ids.push_back(state_entry.first);
			}
			std::sort(state_ids.begin(), state_ids.end());

			for (const auto& state_id : state_ids) {
				ss << "State " << state_id << ":\n";

				auto state_it = action_table.find(state_id);
				if (state_it != action_table.end()) {
					// �ռ����Ų�����
					std::vector<parse::symbol_t> symbols;
					for (const auto& action_entry : state_it->second) {
						symbols.push_back(action_entry.first);
					}

					std::sort(symbols.begin(), symbols.end(),
						[](const parse::symbol_t& a, const parse::symbol_t& b) {
							return a.name < b.name;
						});

					for (const auto& sym : symbols) {
						auto action_it = state_it->second.find(sym);
						if (action_it != state_it->second.end()) {
							ss << "  " << std::setw(10) << sym.name << " : "
								<< action_it->second.to_string() << "\n";
						}
					}
				}
				ss << "\n";
			}
			
			return ss.str();
		}

		std::string goto_table_to_string() const {
			std::stringstream ss;

			// �ռ�����״̬ID
			std::set<item_set_id_t> state_ids;
			std::set<symbol_t> symbols;

			for (const auto& entry : goto_table) {
				state_ids.insert(entry.first.first);
				symbols.insert(entry.first.second);
			}

			// ת��Ϊ�����������Ա��������
			std::vector<item_set_id_t> sorted_states(state_ids.begin(), state_ids.end());
			std::sort(sorted_states.begin(), sorted_states.end());

			std::vector<symbol_t> sorted_symbols(symbols.begin(), symbols.end());
			std::sort(sorted_symbols.begin(), sorted_symbols.end(),
				[](const symbol_t& a, const symbol_t& b) {
					return a.name < b.name;
				});

			// �����ͷ
			ss << "GOTO Table:\n";
			ss << std::setw(8) << "State";
			for (const auto& sym : sorted_symbols) {
				ss << std::setw(12) << sym.name;
			}
			ss << "\n\n";

			// ���ÿ��״̬����
			for (const auto& state : sorted_states) {
				ss << std::setw(8) << state;

				for (const auto& sym : sorted_symbols) {
					auto key = std::make_pair(state, sym);
					auto it = goto_table.find(key);
					if (it != goto_table.end()) {
						if (it->second == 0)
							ss << std::setw(12) << " ";
						else
							ss << std::setw(12) << it->second;
					}
					else {
						ss << std::setw(12) << "";
					}
				}
				ss << "\n\n";
			}

			return ss.str();
		}

		// �����Ҫ����ϸ��������������һ������ϸ�İ汾
		std::string goto_table_to_string_detailed() const {
			std::stringstream ss;

			ss << "Detailed GOTO Table:\n";
			ss << "====================\n";

			// �ռ�����״̬ID������
			std::set<item_set_id_t> state_ids;
			for (const auto& entry : goto_table) {
				state_ids.insert(entry.first.first);
			}

			std::vector<item_set_id_t> sorted_states(state_ids.begin(), state_ids.end());
			std::sort(sorted_states.begin(), sorted_states.end());

			for (const auto& state : sorted_states) {
				ss << "State " << state << ":\n";

				// �ռ���״̬������GOTO��Ŀ
				std::vector<std::pair<symbol_t, item_set_id_t>> entries;
				for (const auto& entry : goto_table) {
					if (entry.first.first == state) {
						entries.emplace_back(entry.first.second, entry.second);
					}
				}

				// ��������������
				std::sort(entries.begin(), entries.end(),
					[](const std::pair<symbol_t, item_set_id_t>& a,
						const std::pair<symbol_t, item_set_id_t>& b) {
							return a.first.name < b.first.name;
					});

				// ���ÿ��GOTO��Ŀ
				for (const auto& entry : entries) {
					if (entry.second != 0)
						ss << "  " << std::setw(10) << entry.first.name << " : "
							<< entry.second << "\n";
				}

				ss << "\n";
			}

			return ss.str();
		}

		// �����ŷ����GOTO����ͼ
		std::string goto_table_to_string_by_symbol() const {
			std::stringstream ss;

			ss << "GOTO Table Grouped by Symbol:\n";
			ss << "=============================\n";

			// �ռ����з��Ų�����
			std::set<symbol_t> symbols;
			for (const auto& entry : goto_table) {
				symbols.insert(entry.first.second);
			}

			std::vector<symbol_t> sorted_symbols(symbols.begin(), symbols.end());
			std::sort(sorted_symbols.begin(), sorted_symbols.end(),
				[](const symbol_t& a, const symbol_t& b) {
					return a.name < b.name;
				});

			for (const auto& sym : sorted_symbols) {
				ss << "Symbol " << sym.name << ":\n";

				// �ռ��÷��ŵ�����GOTO��Ŀ
				std::vector<std::pair<item_set_id_t, item_set_id_t>> entries;
				for (const auto& entry : goto_table) {
					if (entry.first.second == sym) {
						entries.emplace_back(entry.first.first, entry.second);
					}
				}

				// ��Դ״̬����
				std::sort(entries.begin(), entries.end(),
					[](const std::pair<item_set_id_t, item_set_id_t>& a,
						const std::pair<item_set_id_t, item_set_id_t>& b) {
							return a.first < b.first;
					});

				// ���ÿ��GOTO��Ŀ
				for (const auto& entry : entries) {
					ss << "  " << std::setw(4) << entry.first << " -> "
						<< entry.second << "\n";
				}

				ss << "\n";
			}

			return ss.str();
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

	class lexer {
	private:

		std::vector<std::pair<std::regex, parse::symbol_t>> token_patterns;
		parse::symbol_t end_marker{ "$", parse::symbol_type_t::TERMINAL };
		std::vector<std::string> errors;
		size_t line_number = 1;
		size_t column_number = 1;

		// �����հ��ַ���ע��
		void skip_whitespace_and_comments(const std::string& input, size_t& pos) {
			while (pos < input.size()) {
				// �����հ��ַ�
				if (std::isspace(input[pos])) {
					if (input[pos] == '\n') {
						line_number++;
						column_number = 1;
					}
					else {
						column_number++;
					}
					pos++;
					continue;
				}

				// ��鵥��ע��
				if (pos + 1 < input.size() && input[pos] == '/' && input[pos + 1] == '/') {
					pos += 2;
					column_number += 2;
					while (pos < input.size() && input[pos] != '\n') {
						pos++;
						column_number++;
					}
					if (pos < input.size() && input[pos] == '\n') {
						line_number++;
						column_number = 1;
						pos++;
					}
					continue;
				}

				// ������ע��
				if (pos + 1 < input.size() && input[pos] == '/' && input[pos + 1] == '*') {
					pos += 2;
					column_number += 2;
					while (pos + 1 < input.size() && !(input[pos] == '*' && input[pos + 1] == '/')) {
						if (input[pos] == '\n') {
							line_number++;
							column_number = 1;
						}
						else {
							column_number++;
						}
						pos++;
					}
					if (pos + 1 >= input.size()) {
						add_error("Unterminated multi-line comment");
						return;
					}
					pos += 2;
					column_number += 2;
					continue;
				}

				// ���ǿհ׻�ע�ͣ��˳�ѭ��
				break;
			}
		}

		void add_error(const std::string& message) {
			std::string error_msg = "Line " + std::to_string(line_number) +
				", Column " + std::to_string(column_number) +
				": " + message;
			errors.push_back(error_msg);
			std::cerr << "Lexer Error: " << error_msg << std::endl;
		}

	public:
		lexer() {
			add_token_pattern("\\bint\\b", parse::symbol_t("int", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\bfloat\\b", parse::symbol_t("float", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\bchar\\b", parse::symbol_t("char", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\bbool\\b", parse::symbol_t("bool", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\bif\\b", parse::symbol_t("if", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\belse\\b", parse::symbol_t("else", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\bwhile\\b", parse::symbol_t("while", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\breturn\\b", parse::symbol_t("return", parse::symbol_type_t::TERMINAL));
			add_token_pattern("[a-zA-Z_][a-zA-Z0-9_]*", parse::symbol_t("id", parse::symbol_type_t::TERMINAL));
			add_token_pattern("[0-9]+", parse::symbol_t("int_lit", parse::symbol_type_t::TERMINAL));
			add_token_pattern("[0-9]+\\.[0-9]*", parse::symbol_t("float_lit", parse::symbol_type_t::TERMINAL));
			add_token_pattern("'.'", parse::symbol_t("char_lit", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\btrue\\b|\\bfalse\\b", parse::symbol_t("bool_lit", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\+", parse::symbol_t("+", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\-", parse::symbol_t("-", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\*", parse::symbol_t("*", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\/", parse::symbol_t("/", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\=", parse::symbol_t("=", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\==", parse::symbol_t("==", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\!=", parse::symbol_t("!=", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\<", parse::symbol_t("<", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\>", parse::symbol_t(">", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\<=", parse::symbol_t("<=", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\>=", parse::symbol_t(">=", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\&\\&", parse::symbol_t("&&", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\|\\|", parse::symbol_t("||", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\!", parse::symbol_t("!", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\(", parse::symbol_t("(", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\)", parse::symbol_t(")", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\{", parse::symbol_t("{", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\}", parse::symbol_t("}", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\;", parse::symbol_t(";", parse::symbol_type_t::TERMINAL));
			add_token_pattern("\\,", parse::symbol_t(",", parse::symbol_type_t::TERMINAL));
		}

		void add_token_pattern(const std::string& pattern, const parse::symbol_t& symbol) {
			try {
				token_patterns.emplace_back(std::regex(pattern), symbol);
			}
			catch (const std::regex_error& e) {
				add_error("Invalid regex pattern: " + pattern + " - " + e.what());
			}
		}

		//void print_token_patterns() const {
		//	std::cout << "Token Patterns:\n";
		//	for (const auto& pattern : token_patterns) {
		//		std::cout << "  " << pattern.second.name << " : " << pattern.first.str() << "\n";
		//	}
		//}


		std::vector<std::pair<parse::symbol_t, std::string>> tokenize(const std::string& input);
	};

	class lr_parser {


	private:

		std::stack<item_set_id_t> state_stack;
		std::stack<parse::symbol_t> symbol_stack;
	

		std::vector<std::string> parse_history;
		std::vector<std::string> error_msg;

	public:

		std::unique_ptr<parse::grammar> grammar;

		lr_parser(std::unique_ptr<parse::grammar> g) {
			grammar = std::move(g);
			grammar->build();
			state_stack.push(0);
		}

		struct parse_result {
			bool success;
			std::string error_message;
			std::vector<std::string> parse_history;
		};

		parse_result parse(const std::vector<std::pair<parse::symbol_t, std::string>>& input_tokens);
		const std::vector<std::string>& get_error() const { return error_msg; }
		const std::vector<std::string>& get_parse_history() const {
			return parse_history;
		}
		const std::string parse_history_to_string() const {
			std::string result;
			for (const auto& info : parse_history) {
				result += info + "\n";
			}
			return result;
		}

	private:
		bool error_recovery(
			std::stack<int>& state_stack,
			std::stack<parse::symbol_t>& symbol_stack,
			const std::vector<std::pair<parse::symbol_t, std::string>>& tokens,
			size_t& token_index
		);

		void add_error(const std::string& message);
		std::any execute_semantic_action(
			const parse::production_t& prod,
			const std::vector<std::any>& children
		);
	};

}

//namespace parse {
//
//	class lr_automation_builder {
//	public:
//		static void build(gram::grammar& grammar,
//			std::vector<gram::lr0_item_set>& states,
//			//lalr1_parser_table& table,
//			bool lalr = false
//		);
//	};
//}

std::unique_ptr<parse::grammar> grammar_parser(const std::string& filename);

#endif  // __LR_PARSER_H__
