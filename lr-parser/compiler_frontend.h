#pragma once
#ifndef __COMPILER_FRONTEND__
#define __COMPILER_FRONTEND__

#include "framework.h"
#include "lr_parser.h"

#include <string>
#include <fstream>
#include <iostream>

class compiler_frontend {

private:
	parse::lexer lex;

	std::unique_ptr<parse::lr_parser> parser;

public:
	compiler_frontend(const std::string& grammar_bnf) {
		parser = std::make_unique<parse::lr_parser>(grammar_parser(grammar_bnf));
	}

	bool compile(const std::string& code_file, bool is_file) {
		
		if (!is_file)
			return false;

		std::ifstream infile;
		infile.open(code_file);

		if (!infile.is_open())
		{
			std::cerr << "Failed to open code file: " << code_file << std::endl;
		}
		std::string content((std::istreambuf_iterator<char>(infile)),
			std::istreambuf_iterator<char>());

		auto tokens = lex.tokenize(content);

		auto parse_result = parser->parse(tokens);

		std::cout << parser->parse_history_to_string() << std::endl;

		if (!parse_result.success) {
			std::cerr << "Syntax errors found:\n";
			std::cerr << parse_result.error_message << "\n";
			return false;
		}

		std::cout << "Compilation successful!\n";

		return true;
	}

};


#endif
