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



parse::lr_parser::parse_result parse::lr_parser::parse(const std::vector<std::pair<parse::symbol_t, std::string>>& input_tokens)
{
	parse_history.clear();
	symbol_stack.push(parse::symbol_t{ "$", parse::symbol_type_t::TERMINAL });

	size_t index = 0;
	std::vector<std::pair<parse::symbol_t, std::string>> tokens = input_tokens;
	tokens.push_back({ grammar->end_marker, "$" });

#ifdef __LALR1_PARSER_HISTORY_INFO__
	parse_history.push_back("Start parsing...");
#endif

	while (true)
	{
		item_set_id_t current_state = state_stack.top();

		parse::symbol_t current_token = tokens[index].first;

#ifdef __LALR1_PARSER_HISTORY_INFO__
		std::string state_info = " State: " + std::to_string(current_state) +
			" , Input: " + current_token.name +
			" , State stack size: " + std::to_string(state_stack.size()) + " , Top state: " + std::to_string(state_stack.top()) +
			" , Symbol stack size: " + std::to_string(symbol_stack.size()) + " , Top symbol: " + symbol_stack.top().name;

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

				auto prod = grammar->get_production_by_id(a.value);
				bool is_epsilon = (prod->right.size() == 1 && prod->right[0] == grammar->epsilon);

#ifdef __LALR1_PARSER_HISTORY_INFO__
				parse_history.push_back("Reduce: " + prod->to_string());
				if (is_epsilon) {
					parse_history.push_back("Epsilon production - no symbols to pop");
				}
#endif

				// 处理弹出操作 - 对于 ε-产生式，不弹出任何符号
				if (!is_epsilon) {
					for (size_t i = 0; i < prod->right.size(); i++) {
						if (state_stack.empty() || symbol_stack.empty()) {
							return { false, "fatal: symbol stack empty !", parse_history };
						}

#ifdef __LALR1_PARSER_HISTORY_INFO__
						parse_history.push_back("Pop: State " + std::to_string(state_stack.top()));
#endif

						state_stack.pop();
						symbol_stack.pop();
					}
				}

				if (state_stack.empty()) {
					return { false, "fatal: state stack empty !", parse_history };
				}


				item_set_id_t new_state = state_stack.top();
				symbol_t non_terminal = prod->left;
				auto goto_key = std::make_pair(new_state, non_terminal);
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
			parse_history.push_back("\n\n");
			std::stack<std::string> s_s;
			std::string info;
			for (int i = state_stack.size() - 1; i >= 0; i--) {
				s_s.push(std::to_string(state_stack.top()));
				state_stack.pop();
			}
			for (int i = s_s.size() - 1; i >= 0; i--) {
				info += s_s.top() + " ";
				s_s.pop();
			}

			parse_history.push_back("State Stack: " + info);
			for (int i = symbol_stack.size() - 1; i >= 0; i--) {
				s_s.push(symbol_stack.top().name);
				symbol_stack.pop();
			}

			info.clear();
			for (int i = s_s.size() - 1; i >= 0; i--) {
				info += s_s.top() + " ";
				s_s.pop();
			}
			parse_history.push_back("Symbol Stack: " + info);

			return { false, "ACTION(" + std::to_string(current_state) +
				", " + current_token.name + ") doesn't have the corresponding entry.", parse_history };
		}
	}


	return parse_result();
}

std::vector<std::pair<parse::symbol_t, std::string>> parse::lexer::tokenize(const std::string& input)
{
	std::vector<std::pair<parse::symbol_t, std::string>> tokens;
	size_t pos = 0;
	line_number = 1;
	column_number = 1;
	errors.clear();

	while (pos < input.size()) {
		// 跳过空白字符和注释
		skip_whitespace_and_comments(input, pos);
		if (pos >= input.size()) break;

		// 尝试匹配所有模式
		bool matched = false;
		size_t max_match_length = 0;
		parse::symbol_t matched_symbol;
		std::string matched_lexeme;

		for (const auto& pattern : token_patterns) {
			std::smatch match;
			std::string remaining_input = input.substr(pos);

			if (std::regex_search(remaining_input, match, pattern.first,
				std::regex_constants::match_continuous)) {
				if (match.length() > max_match_length) {
					max_match_length = match.length();
					matched_symbol = pattern.second;
					matched_lexeme = match.str();
					matched = true;
				}
			}
		}

		if (matched) {
			tokens.emplace_back(matched_symbol, matched_lexeme);
			pos += max_match_length;
			column_number += max_match_length;
		}
		else {
			// 无法识别的字符
			std::string invalid_char(1, input[pos]);
			add_error("Unrecognized character: '" + invalid_char + "'");
			pos++;
			column_number++;
		}
	}

	// 添加结束标记
	tokens.emplace_back(end_marker, "$");

	return tokens;
}
