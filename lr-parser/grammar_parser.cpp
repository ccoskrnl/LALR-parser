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


std::unique_ptr<parse::lalr_grammar> grammar_parser(const std::string& filename) {


	std::unique_ptr<parse::lalr_grammar> grammar = std::make_unique<parse::lalr_grammar>();

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
			if (alt == "¦Å" || alt == "epsilon" || alt.empty()) {
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

