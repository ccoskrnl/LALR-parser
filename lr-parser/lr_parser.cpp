#include "lr_parser.h"
#include <algorithm>
#include <iostream>

gram::lalr1_item_set gram::grammar::closure(const lalr1_item_set& I)
{
	lalr1_item_set new_I = I;
	bool changed = true;

	do
	{
		changed = false;

		for (const gram::lalr1_item& item : I.items) {
			gram::symbol next_sym = item.next_symbol();

			if (next_sym.type == gram::symbol_type::NON_TERMINAL && !next_sym.name.empty()) {

				// compute beta a (the right part after the non-terminal and the lookaheads)
				std::vector<gram::symbol> beta_a;
				for (int i = item.dot_pos + 1; i < item.product.right.size(); i++) {
					beta_a.push_back(item.product.right[i]);
				}

				// compute FIRST(beta a)
				std::unordered_set<gram::symbol, gram::symbol_hasher> lookaheads =
					comp_first_of_sequence(beta_a, item.lookaheads);

				// add new items for each production of next_sym
				const auto& prods = get_productions_for(next_sym);
				for (const auto& prod : prods) {

					gram::lalr1_item new_item(prod, 0, lookaheads);

					// Check if the new item (with same core) already exists
					auto found = new_I.items.find(new_item);
					if (found != new_I.items.end()) {

						size_t before_size = found->lookaheads.size();

						// Because std::unordered_set's iterator is const, we need to use a workaround
						// to modify the lookaheads of the found item
						gram::lalr1_item megred_item = *found; // Copy the found item
						new_I.items.erase(found); // Remove the old item

						megred_item.lookaheads.insert(lookaheads.begin(), lookaheads.end()); // Merge lookaheads
						new_I.items.insert(megred_item); // Reinsert the modified item

						if (found->lookaheads.size() > before_size) {
							changed = true;
						}
					}
					else {
						new_I.items.insert(new_item);
						changed = true;
					}
				}

			}
		}

	} while (changed);

	return new_I;
}

gram::lalr1_item_set gram::grammar::go_to(const lalr1_item_set& I, const symbol& X)
{

	lalr1_item_set new_set;

	for (const auto& item : I.items) {
		if (item.dot_pos < item.product.right.size() &&
			item.product.right[item.dot_pos] == X) {
			gram::lalr1_item moved_item(item.product, item.dot_pos + 1, item.lookaheads);
			new_set.items.insert(moved_item);
		}
	}

	return closure(new_set);

}

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
				while (i < prod.right.size() && continue_checking)
				{
					const gram::symbol& sym = prod.right[i];

					if (sym.type == gram::symbol_type::TERMINAL) {
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

				if (i == prod.right.size() && continue_checking) {
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
std::unordered_set<gram::symbol, gram::symbol_hasher> gram::grammar::comp_first_of_sequence(
	const std::vector<gram::symbol>& sequence,
	const std::unordered_set<gram::symbol, gram::symbol_hasher>& lookaheads
)
{
	std::unordered_set<gram::symbol, gram::symbol_hasher> result;

	bool all_contain_epsilon = true;

	for (const auto& sym : sequence) {
		if (sym.type == gram::symbol_type::TERMINAL) {
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

