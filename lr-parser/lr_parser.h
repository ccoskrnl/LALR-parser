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

static_assert(std::is_same_v < parser_action_value_t, production_id_t>, "action_value_t and production_id_t must be the same type");

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
		parser_action_t(parser_action_type_t t = parser_action_type_t::ERROR, parser_action_value_t v = -1) : type(t), value(v) {}

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
			: left(l), right(r) {

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

	/*
 * LR(0) item structure representing a production with a dot position
 *
 * An LR(0) item consists of a production with a dot (•) indicating the current parsing position.
 * The dot divides the production into two parts: symbols that have been seen (left of dot)
 * and symbols that are expected next (right of dot).
 */
	struct lr0_item_t {
		std::shared_ptr<production_t> product;  // Pointer to the production rule
		int dot_pos;                            // Position of the dot in the production
		item_id_t id;                           // Unique identifier for the item

		/* Returns the current dot position in the production */
		int get_dot_pos() const {
			return dot_pos;
		}

		/* Returns the unique identifier of this item */
		item_id_t get_id() const {
			return id;
		}

		/* Returns the identifier of the production rule */
		production_id_t get_production_id() const {
			return product->id;
		}

		/*
		 * Updates the dot position and recalculates the item ID
		 * The ID is constructed by combining production ID (high 32 bits) and dot position (low 32 bits)
		 */
		void set_dot_pos(int new_dot_pos) {
			dot_pos = new_dot_pos;
			id = (static_cast<item_id_t>(product->id) << 32) | (static_cast<item_id_t>(dot_pos) & 0xFFFFFFFF);
		}

		/* Returns a shared pointer to the production rule */
		const std::shared_ptr<production_t> get_production() const {
			return product;
		}

		/* Constructor that initializes the item with a production and dot position */
		lr0_item_t(std::shared_ptr<production_t> prod, int dot) : product(prod), dot_pos(dot) {
			id = (static_cast<item_id_t>(product->id) << 32) | (static_cast<item_id_t>(dot_pos) & 0xFFFFFFFF);
		}

		/* Equality operator compares both production ID and dot position */
		bool operator==(const lr0_item_t& other) const {
			return product->id == other.product->id && dot_pos == other.dot_pos;
		}

		/* Returns the symbol immediately following the dot position */
		symbol_t next_symbol() const {
			if (dot_pos < product->right.size()) {
				return product->right[dot_pos];
			}
			return symbol_t{ "", symbol_type_t::EPSILON };
		}

		/* Returns the remaining symbols following the dot position */
		const std::vector<symbol_t> get_remaining_symbols() const {
			std::vector<symbol_t> remaining_symbols;
			
			for (int i = dot_pos; i < product->right.size(); i++) {
				remaining_symbols.push_back(this->product->right[i]);
			}

			return remaining_symbols;
		}

		/* Returns the symbol immediately preceding the dot position */
		symbol_t current_symbol() const {
			if (dot_pos > 0)
				return product->right[dot_pos - 1];
			return symbol_t{ "", symbol_type_t::TERMINAL };
		}

		/*
		 * Determines if this is a kernel item
		 * Kernel items are either the augmented start production or items with dot not at beginning
		 */
		bool is_kernel_item() const {
			return dot_pos > 0 || product->id == AUGMENTED_GRAMMAR_PROD_ID;
		}

		/*
		 * Converts the item to a human-readable string representation
		 * Format: [ID: X] Left -> symbol1 . symbol2
		 */
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

	/*
	 * LALR(1) item structure extending LR(0) item with lookahead symbols
	 *
	 * An LALR(1) item adds lookahead symbols to an LR(0) item, which are used to resolve
	 * parsing conflicts by specifying which terminal symbols can follow a production.
	 */
	struct lalr1_item_t : lr0_item_t {
		std::unordered_set<symbol_t, symbol_hasher> lookaheads;  // Set of lookahead symbols, avoid to modify.

		/* Constructor that initializes with production and dot position (empty lookaheads) */
		lalr1_item_t(std::shared_ptr<production_t> prod, int dot)
			: lr0_item_t(prod, dot), lookaheads({}) {
		}

		/* Constructor that converts an LR(0) item to LALR(1) item (empty lookaheads) */
		lalr1_item_t(const lr0_item_t& item)
			: lr0_item_t(item.product, item.dot_pos), lookaheads({ }) {
		}

		/* Copy constructor that copies both LR(0) properties and lookaheads */
		lalr1_item_t(const lalr1_item_t& item)
			: lr0_item_t(item.product, item.dot_pos), lookaheads({ }) {
			this->add_lookaheads(item.lookaheads);
		}

		/* Constructor that initializes with production, dot position, and lookaheads */
		lalr1_item_t(std::shared_ptr<production_t> prod, int dot, const std::unordered_set<symbol_t, symbol_hasher>& las)
			: lr0_item_t(prod, dot), lookaheads(las) {
		}

		/* Constructor that converts LR(0) item and adds lookaheads */
		lalr1_item_t(const lr0_item_t& item, const std::unordered_set<symbol_t, symbol_hasher>& las)
			: lr0_item_t(item.product, item.dot_pos), lookaheads(las) {
		}

		/* Default constructor */
		lalr1_item_t()
			: lr0_item_t(nullptr, 0), lookaheads({ }) {
		}

		/* Adds multiple lookahead symbols and returns true if any were new */
		bool add_lookaheads(const std::unordered_set<symbol_t, symbol_hasher>& las) {
			if (las.empty())
				return false;
			const size_t old_size = lookaheads.size();
			lookaheads.insert(las.begin(), las.end());
			return old_size != lookaheads.size();
		}

		/* Adds a single lookahead symbol and returns true if it was new */
		bool add_lookaheads(const symbol_t& la) {
			const size_t old_size = lookaheads.size();
			lookaheads.insert(la);
			return old_size != lookaheads.size();
		}

		/* Removes a specific lookahead symbol */
		void del_lookaheads(const symbol_t& la) {
			lookaheads.erase(la);
		}

		/* Removes multiple lookahead symbols */
		void del_lookaheads(const std::unordered_set<symbol_t, symbol_hasher>& las) {
			for (const auto& la : las) {
				lookaheads.erase(la);
			}
		}


		/*
		 * Converts the item to a human-readable string representation including lookaheads
		 * Format: [ID: X] Left -> symbol1 . symbol2 , { lookahead1 lookahead2 }
		 */
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

			for (const auto& la : lookaheads) {
				result += la.name + " ";
			}
			result += "}";
			return result;
		}
	};

	/* Hash function for LR(0) items based on production ID and dot position */
	struct lr0_item_hasher {
		size_t operator()(const lr0_item_t& item) const {
			size_t h1 = std::hash<production_id_t>()(item.product->id);
			size_t h2 = std::hash<int>()(item.dot_pos);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	/* Hash function for LALR(1) items based on production ID, dot position, and lookaheads */
	struct lalr1_item_hasher {
		size_t operator()(const lalr1_item_t& item) const {
			size_t seed = 0;
			hash_combine(seed, item.product->id);
			hash_combine(seed, item.dot_pos);
			hash_combine(seed, item.id);

			for (const auto& sym : item.lookaheads) {
				hash_combine(seed, symbol_hasher()(sym));
			}

			return seed;
		}

	private:
		/* Helper function to combine hash values using boost-inspired method */
		template <typename T>
		void hash_combine(size_t& seed, const T& val) const {
			std::hash<T> hasher;
			seed ^= hasher(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
	};


	/*
	 * Hash function for pairs of item_id_t and symbol_t
	 *
	 * This struct provides a hash function for std::pair<item_id_t, symbol_t> objects,
	 * combining the hash values of both elements using a method inspired by boost's hash_combine.
	 */
	struct pair_item_id_symbol_hasher {
		size_t operator()(const std::pair<item_id_t, symbol_t>& p) const {
			size_t h1 = std::hash<item_id_t>()(p.first);
			size_t h2 = symbol_hasher()(p.second);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	/*
	 * Hash function for pairs of item_set_id_t and item_id_t
	 *
	 * This struct provides a hash function for std::pair<item_set_id_t, item_id_t> objects,
	 * combining the hash values of both elements using a method inspired by boost's hash_combine.
	 */
	struct pair_items_state_item_id_hasher {
		size_t operator()(const std::pair<item_set_id_t, item_id_t>& p) const {
			size_t h1 = std::hash<item_set_id_t>()(p.first);
			size_t h2 = std::hash<item_id_t>()(p.second);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	/*
	 * Class representing a set of LR(0) items used in parser construction
	 *
	 * An LR(0) item set contains multiple LR(0) items that share the same core (production and dot position)
	 * and is used to represent states in the LR(0) automaton during parser construction.
	 */
	class lr0_item_set {
	public:
		std::unordered_set<lr0_item_t, lr0_item_hasher> items;  // Collection of LR(0) items
		item_set_id_t id;                                        // Unique identifier for this item set

		/* Constructor that optionally sets the item set ID */
		lr0_item_set(int set_id = -1) : id(set_id) {}

		/* Copy constructor */
		lr0_item_set(const lr0_item_set& other) : id(other.id), items(other.items) {}

		/* Equality comparison based on the core items (production ID and dot position) */
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

		/* Adds a single LR(0) item to the set */
		void add_items(const lr0_item_t& item) {
			this->items.insert(item);
		}

		/* Adds all items from another LR(0) item set */
		void add_items(const lr0_item_set& items) {
			for (const auto& i : items.items) {
				this->items.insert(i);
			}
		}

		/* Finds an item in the set based on its core (production ID and dot position) */
		const lr0_item_t* find_item(const lr0_item_t& core) const {
			for (auto& item : items) {
				if (item.product->id == core.product->id && item.dot_pos == core.dot_pos) {
					return &item;
				}
			}
			return nullptr;
		}

		/* Checks if the item set is empty */
		bool empty() const {
			return this->items.empty();
		}

		/* Returns a reference to the underlying unordered set of items */
		std::unordered_set<lr0_item_t, lr0_item_hasher>& get_items() {
			return items;
		}

		/* Returns a const reference to the underlying unordered set of items */
		const std::unordered_set<lr0_item_t, lr0_item_hasher>& get_items() const {
			return items;
		}

		/* Output stream operator for printing the item set */
		friend std::ostream& operator<<(std::ostream& os, const lr0_item_set& item_set) {
			os << "Item Set ID: " << item_set.id << std::endl;
			for (const auto& item : item_set.items) {
				os << "  " << item.to_string() << std::endl;
			}
			return os;
		}

		/* Converts the item set to a string representation */
		std::string to_string() const {
			std::string result = "Item Set ID: " + std::to_string(id) + "\n";
			for (const auto& item : items) {
				result += "  " + item.to_string() + "\n";
			}
			return result;
		}

		/* Returns all symbols that can be used for transitions from this item set */
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

	/*
	 * Class representing a set of LALR(1) items used in parser construction
	 *
	 * An LALR(1) item set extends LR(0) item sets with lookahead information,
	 * which is crucial for resolving parsing conflicts in LALR(1) parsers.
	 */
	class lalr1_item_set {
	public:
		std::unordered_set<lalr1_item_t, lalr1_item_hasher> items;  // Collection of LALR(1) items
		item_set_id_t id;                                            // Unique identifier for this item set

		/* Constructor that optionally sets the item set ID */
		lalr1_item_set(int set_id = -1) : id(set_id) {}

		/* Copy constructor */
		lalr1_item_set(const lalr1_item_set& other) : id(other.id), items(other.items) {}

		/* Equality comparison based on the core items (production ID and dot position) */
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

		/* Adds a single LALR(1) item to the set */
		void add_items(const lalr1_item_t& item) {
			this->items.insert(item);
		}

		/* Adds all items from another LALR(1) item set */
		void add_items(const lalr1_item_set& items) {
			for (const auto& i : items.items) {
				this->items.insert(i);
			}
		}

		/* Removes a single item from the set (alias for remove_item) */
		bool del_items(const lalr1_item_t& item) {
			return remove_item(item);
		}

		/* Removes all items from another LALR(1) item set */
		bool del_items(const lalr1_item_set& items) {
			bool result = false;
			for (const auto& i : items.items) {
				result = remove_item(i);
			}
			return result;
		}

		/* Returns a reference to the underlying unordered set of items */
		std::unordered_set<lalr1_item_t, lalr1_item_hasher>& get_items() {
			return items;
		}

		/*
		 * Adds lookahead symbols to a specific item identified by its ID
		 * Returns true if any new lookaheads were added
		 */
		bool add_lookaheads_for_item(const item_id_t id, const std::unordered_set<symbol_t, symbol_hasher>& las) {
			const lalr1_item_t* original_i = find_item_by_id(id);

			if (original_i == nullptr)
				return false;

			size_t before_size = original_i->lookaheads.size();
			lalr1_item_t modified_i = lalr1_item_t(*original_i);
			del_items(modified_i);

			// Copy the original item and add new lookaheads
			modified_i.add_lookaheads(las);
			add_items(modified_i);

			if (modified_i.lookaheads.size() != before_size)
				return true;

			return false;
		}

		/* Finds an item in the set based on its core (production ID and dot position) */
		const lalr1_item_t* find_item(const lr0_item_t& core) const {
			for (const auto& item : items) {
				if (item.product->id == core.product->id && item.dot_pos == core.dot_pos) {
					return &item;
				}
			}
			return nullptr;
		}

		/* Finds an item in the set based on its unique ID */
		const lalr1_item_t* find_item_by_id(const item_id_t& id) const {
			for (const auto& item : items) {
				if (id == item.id)
					return &item;
			}
			return nullptr;
		}

		/* Checks if the item set is empty */
		bool empty() const {
			return this->items.empty();
		}

		/* Output stream operator for printing the item set */
		friend std::ostream& operator<<(std::ostream& os, const lalr1_item_set& item_set) {
			os << "Item Set ID: " << item_set.id << std::endl;
			for (const auto& item : item_set.items) {
				os << "  " << item.to_string() << std::endl;
			}
			return os;
		}

		/* Converts the item set to a string representation */
		std::string to_string() const {
			std::string result = "Item Set ID: " + std::to_string(id) + "\n";
			for (const auto& item : items) {
				result += "  " + item.to_string() + "\n";
			}
			return result;
		}

	private:
		/*
		 * Removes a specific item from the set
		 * First attempts direct hash-based removal, then falls back to content-based search
		 */
		bool remove_item(const lalr1_item_t& target) {
			// First try direct removal (if hash is correct)
			if (items.erase(target) > 0) return true;

			// If that fails, search for item with matching content
			auto it = std::find_if(items.begin(), items.end(), [&](const auto& item) {
				return item.product->id == target.product->id &&
					item.dot_pos == target.dot_pos &&
					item.id == target.id &&
					item.lookaheads == target.lookaheads; // Direct set comparison
				});

			if (it != items.end()) {
				items.erase(it);
				return true;
			}

			return false;
		}
	};

	/*
	 * Hash function for pairs of item_set_id_t and symbol_t
	 *
	 * This struct provides a hash function for std::pair<item_set_id_t, symbol_t> objects,
	 * combining the hash values of both elements using a method inspired by boost's hash_combine.
	 */
	struct pair_item_set_symbol_hasher {
		size_t operator()(const std::pair<item_set_id_t, symbol_t>& p) const {
			size_t h1 = std::hash<item_set_id_t>()(p.first);
			size_t h2 = symbol_hasher()(p.second);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};

	/*
 * Main class representing an LALR(1) grammar and its associated parsing tables
 *
 * This class encapsulates a complete LALR(1) grammar, including productions, symbols,
 * and the algorithms to compute FIRST sets, build LR(0) and LALR(1) states, and
 * construct the ACTION and GOTO tables used by the parser.
 */
	class lalr_grammar {
	public:
		symbol_t start_symbol;  // The start symbol of the grammar

		// Special symbols used in parsing
		symbol_t epsilon{ "", symbol_type_t::EPSILON };          // Epsilon (empty) symbol
		symbol_t end_marker{ "$", symbol_type_t::TERMINAL };     // End of input marker
		symbol_t lookahead_sentinel{ "#", symbol_type_t::TERMINAL };  // Special symbol for lookahead computation

		// Grammar components
		std::unordered_map<symbol_t, std::vector<std::shared_ptr<production_t>>, symbol_hasher> productions;  // Productions organized by left-hand side
		std::unordered_set<symbol_t, symbol_hasher> terminals;     // Set of terminal symbols
		std::unordered_set<symbol_t, symbol_hasher> non_terminals; // Set of non-terminal symbols

		// Computed sets and tables
		std::unordered_map<symbol_t, std::unordered_set<symbol_t, symbol_hasher>, symbol_hasher> first_sets;  // FIRST sets for symbols
		std::vector<std::shared_ptr<lalr1_item_set>> lalr1_states;  // LALR(1) states
		std::unordered_map<item_set_id_t, std::unordered_map<parse::symbol_t, parse::parser_action_t, parse::symbol_hasher>> action_table;  // ACTION table
		std::unordered_map<std::pair<item_set_id_t, symbol_t>, item_set_id_t, pair_item_set_symbol_hasher> goto_table;  // GOTO table

		/* Returns all terminal symbols in the grammar */
		const std::unordered_set<symbol_t, symbol_hasher>& all_symbols() const {
			return terminals;
		}

		/* Adds a production to the grammar with the given left-hand side and right-hand side symbols */
		void add_production(const symbol_t& left, const std::vector<symbol_t>& right) {
			std::shared_ptr<production_t> prod = std::make_shared<production_t>(left, right);
			productions[left].push_back(prod);
			non_terminals.insert(left);

			// Add symbols to appropriate sets
			for (const auto& sym : right) {
				if (sym.type == symbol_type_t::TERMINAL && sym.name != epsilon.name) {
					terminals.insert(sym);
				}
				else if (sym.type == symbol_type_t::NON_TERMINAL) {
					non_terminals.insert(sym);
				}
			}
		}

		/* Returns all productions for a given symbol (left-hand side) */
		const std::vector<std::shared_ptr<production_t>> get_productions_for(const symbol_t& symbol) const {
			static std::vector<std::shared_ptr<production_t>> empty;
			auto it = productions.find(symbol);
			if (it != productions.end()) return it->second;
			return empty;
		}

		/* Finds a production by its unique identifier */
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

		// Core grammar algorithms
		std::shared_ptr<lr0_item_set> lr0_closure(const lr0_item_set& I) const;  // Computes LR(0) closure
		std::shared_ptr<lr0_item_set> lr0_go_to(const lr0_item_set& I, const symbol_t& X) const;  // Computes LR(0) GOTO
		std::shared_ptr<lalr1_item_set> closure(const lalr1_item_set& I);  // Computes LALR(1) closure
		std::shared_ptr<lalr1_item_set> go_to(const lalr1_item_set& I, const symbol_t& X);  // Computes LALR(1) GOTO

		const bool can_derive_epsilon(const symbol_t& non_terminal) const {
			if (non_terminal.type != symbol_type_t::NON_TERMINAL)
				return false;


			if (first_sets.count(non_terminal)) {
				const auto& current_sym_first_sym = first_sets.at(non_terminal);

				// Deal with special case when the following non-terminals also can derive epsilon.
				return current_sym_first_sym.find(epsilon) != current_sym_first_sym.end();
			}

			return false;
		}

		void comp_first_sets();  // Computes FIRST sets for all symbols
		std::unordered_set<symbol_t, symbol_hasher> comp_first_of_sequence(  // Computes FIRST set for a sequence
			const std::vector<symbol_t>& sequence,
			const std::unordered_set<symbol_t, symbol_hasher>& lookaheads = {}
		);

		std::unique_ptr<std::vector<std::shared_ptr<lr0_item_set>>> build_lr0_states();  // Builds LR(0) states
		void initialize_lalr1_states();  // Initializes LALR(1) states from LR(0) states

		void determine_lookaheads(  // Determines lookaheads for LALR(1) items
			const item_set_id_t I_id,
			const symbol_t X,
			std::unordered_map<std::pair<item_set_id_t, item_id_t>, std::vector<std::pair<item_set_id_t, item_id_t>>, pair_items_state_item_id_hasher>& propagation_graph,
			std::unordered_map<std::pair<item_set_id_t, item_id_t>, std::unordered_set<parse::symbol_t, parse::symbol_hasher>, pair_items_state_item_id_hasher>& spontaneous_lookaheads
		);

		void set_lalr1_items_lookaheads();  // Sets lookaheads for all LALR(1) items
		void build_action_table();  // Builds the ACTION table from LALR(1) states

		/* Main build function that constructs all components of the LALR(1) parser */
		void build() {
			comp_first_sets();
			initialize_lalr1_states();
			set_lalr1_items_lookaheads();
			build_action_table();
		}

		/* Converts all productions to a string representation */
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

		/* Converts all LALR(1) states to a string representation */
		std::string lalr1_states_to_string() {
			std::string result;
			for (const std::shared_ptr<lalr1_item_set> state : lalr1_states) {
				result += closure(*state)->to_string() + "\n";
			}
			return result;
		}

		std::string action_table_to_string() const {
			std::stringstream ss;

			// 收集所有状态ID并排序
			std::vector<item_set_id_t> state_ids;
			for (const auto& state_entry : action_table) {
				state_ids.push_back(state_entry.first);
			}
			std::sort(state_ids.begin(), state_ids.end());

			// 收集所有符号并排序（按名称）
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

			// 按符号名称排序
			std::sort(symbols.begin(), symbols.end(),
				[](const parse::symbol_t& a, const parse::symbol_t& b) {
					return a.name < b.name;
				});

			// 输出表头
			ss << "ACTION Table:\n";
			ss << std::setw(8) << "State";
			for (const auto& sym : symbols) {
				ss << std::setw(12) << sym.name;
			}
			ss << "\n";

			// 输出每个状态的行
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

			// 收集所有状态ID并排序
			std::vector<item_set_id_t> state_ids;
			for (const auto& state_entry : action_table) {
				state_ids.push_back(state_entry.first);
			}
			std::sort(state_ids.begin(), state_ids.end());

			for (const auto& state_id : state_ids) {
				ss << "State " << state_id << ":\n";

				auto state_it = action_table.find(state_id);
				if (state_it != action_table.end()) {
					// 收集符号并排序
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

			// 收集所有状态ID
			std::set<item_set_id_t> state_ids;
			std::set<symbol_t> symbols;

			for (const auto& entry : goto_table) {
				state_ids.insert(entry.first.first);
				symbols.insert(entry.first.second);
			}

			// 转换为排序后的向量以便有序输出
			std::vector<item_set_id_t> sorted_states(state_ids.begin(), state_ids.end());
			std::sort(sorted_states.begin(), sorted_states.end());

			std::vector<symbol_t> sorted_symbols(symbols.begin(), symbols.end());
			std::sort(sorted_symbols.begin(), sorted_symbols.end(),
				[](const symbol_t& a, const symbol_t& b) {
					return a.name < b.name;
				});

			// 输出表头
			ss << "GOTO Table:\n";
			ss << std::setw(8) << "State";
			for (const auto& sym : sorted_symbols) {
				ss << std::setw(12) << sym.name;
			}
			ss << "\n\n";

			// 输出每个状态的行
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

		// 如果需要更详细的输出，可以添加一个更详细的版本
		std::string goto_table_to_string_detailed() const {
			std::stringstream ss;

			ss << "Detailed GOTO Table:\n";
			ss << "====================\n";

			// 收集所有状态ID并排序
			std::set<item_set_id_t> state_ids;
			for (const auto& entry : goto_table) {
				state_ids.insert(entry.first.first);
			}

			std::vector<item_set_id_t> sorted_states(state_ids.begin(), state_ids.end());
			std::sort(sorted_states.begin(), sorted_states.end());

			for (const auto& state : sorted_states) {
				ss << "State " << state << ":\n";

				// 收集该状态的所有GOTO条目
				std::vector<std::pair<symbol_t, item_set_id_t>> entries;
				for (const auto& entry : goto_table) {
					if (entry.first.first == state) {
						entries.emplace_back(entry.first.second, entry.second);
					}
				}

				// 按符号名称排序
				std::sort(entries.begin(), entries.end(),
					[](const std::pair<symbol_t, item_set_id_t>& a,
						const std::pair<symbol_t, item_set_id_t>& b) {
							return a.first.name < b.first.name;
					});

				// 输出每个GOTO条目
				for (const auto& entry : entries) {
					if (entry.second != 0)
						ss << "  " << std::setw(10) << entry.first.name << " : "
						<< entry.second << "\n";
				}

				ss << "\n";
			}

			return ss.str();
		}

		// 按符号分组的GOTO表视图
		std::string goto_table_to_string_by_symbol() const {
			std::stringstream ss;

			ss << "GOTO Table Grouped by Symbol:\n";
			ss << "=============================\n";

			// 收集所有符号并排序
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

				// 收集该符号的所有GOTO条目
				std::vector<std::pair<item_set_id_t, item_set_id_t>> entries;
				for (const auto& entry : goto_table) {
					if (entry.first.second == sym) {
						entries.emplace_back(entry.first.first, entry.second);
					}
				}

				// 按源状态排序
				std::sort(entries.begin(), entries.end(),
					[](const std::pair<item_set_id_t, item_set_id_t>& a,
						const std::pair<item_set_id_t, item_set_id_t>& b) {
							return a.first < b.first;
					});

				// 输出每个GOTO条目
				for (const auto& entry : entries) {
					ss << "  " << std::setw(4) << entry.first << " -> "
						<< entry.second << "\n";
				}

				ss << "\n";
			}

			return ss.str();
		}

		/* Output stream operator for printing the grammar */
		friend std::ostream& operator<<(std::ostream& os, const lalr_grammar& g) {
			for (const auto& [left, prods] : g.productions) {
				for (const auto& prod : prods) {
					os << left.name << " -> ";
					for (const auto& sym : prod->right) {
						os << sym.name << " ";
					}
					os << std::endl;
				}
			}
			return os;
		}
	};

	/*
	 * Lexical analyzer class that converts input strings into tokens
	 *
	 * This class uses regular expressions to match tokens in the input stream,
	 * handling whitespace and comments, and reporting lexical errors.
	 */
	class lexer {
	private:
		std::vector<std::pair<std::regex, parse::symbol_t>> token_patterns;  // Regex patterns for tokens
		parse::symbol_t end_marker{ "$", parse::symbol_type_t::TERMINAL };   // End of input marker
		std::vector<std::string> errors;      // Collection of error messages
		size_t line_number = 1;               // Current line number in input
		size_t column_number = 1;             // Current column number in input

		/* Skips whitespace and comments in the input string */
		void skip_whitespace_and_comments(const std::string& input, size_t& pos) {
			while (pos < input.size()) {
				// Skip whitespace characters
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

				// Handle single-line comments
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

				// Handle multi-line comments
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

				// Exit when non-whitespace, non-comment content is found
				break;
			}
		}

		/* Adds an error message to the error collection */
		void add_error(const std::string& message) {
			std::string error_msg = "Line " + std::to_string(line_number) +
				", Column " + std::to_string(column_number) +
				": " + message;
			errors.push_back(error_msg);
			std::cerr << "Lexer Error: " << error_msg << std::endl;
		}

	public:
		/* Constructor that initializes the lexer with default token patterns */
		lexer() {
			// Add token patterns for common programming language constructs
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

		/* Adds a token pattern to the lexer */
		void add_token_pattern(const std::string& pattern, const parse::symbol_t& symbol) {
			try {
				token_patterns.emplace_back(std::regex(pattern), symbol);
			}
			catch (const std::regex_error& e) {
				add_error("Invalid regex pattern: " + pattern + " - " + e.what());
			}
		}

		/* Tokenizes an input string into a sequence of tokens */
		std::vector<std::pair<parse::symbol_t, std::string>> tokenize(const std::string& input);
	};

	/*
	 * LALR(1) parser class that uses the grammar and ACTION/GOTO tables to parse input
	 *
	 * This class implements the LALR(1) parsing algorithm using a stack-based approach
	 * and provides error reporting and recovery mechanisms.
	 */
	class lr_parser {
	private:
		std::stack<item_set_id_t> state_stack;        // Stack of parser states
		std::stack<parse::symbol_t> symbol_stack;     // Stack of symbols

		std::vector<std::string> parse_history;       // History of parsing actions
		std::vector<std::string> error_msg;           // Collection of error messages

	public:
		std::unique_ptr<parse::lalr_grammar> grammar;  // The grammar used for parsing

		/* Constructor that takes a grammar and builds the parsing tables */
		lr_parser(std::unique_ptr<parse::lalr_grammar> g) {
			grammar = std::move(g);
			grammar->build();
			state_stack.push(0);  // Start with initial state
		}

		/* Structure to hold the result of a parsing operation */
		struct parse_result {
			bool success = false;                 // Whether parsing was successful
			std::string error_message;            // Error message if parsing failed
			std::vector<std::string> parse_history;  // History of parsing actions
		};

		/* Parses a sequence of tokens and returns the result */
		parse_result parse(const std::vector<std::pair<parse::symbol_t, std::string>>& input_tokens);

		/* Returns the collection of error messages */
		const std::vector<std::string>& get_error() const { return error_msg; }

		/* Returns the parsing history */
		const std::vector<std::string>& get_parse_history() const {
			return parse_history;
		}

		/* Converts the parsing history to a string */
		const std::string parse_history_to_string() const {
			std::string result;
			for (const auto& info : parse_history) {
				result += info + "\n";
			}
			return result;
		}

	private:
		/* Attempts to recover from a parsing error */
		bool error_recovery(
			std::stack<int>& state_stack,
			std::stack<parse::symbol_t>& symbol_stack,
			const std::vector<std::pair<parse::symbol_t, std::string>>& tokens,
			size_t& token_index
		);

		/* Adds an error message to the error collection */
		void add_error(const std::string& message);

		/* Executes semantic actions associated with productions */
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

std::unique_ptr<parse::lalr_grammar> grammar_parser(const std::string& filename);

#endif  // __LR_PARSER_H__
