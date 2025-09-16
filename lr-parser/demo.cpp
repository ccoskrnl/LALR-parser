#include <iostream>
#include "lr_parser.h"

int main()
{
	std::unique_ptr<parse::grammar> g = grammar_parser("gram_exp01.txt");

	std::cout << "Grammar:\n" << *g << std::endl;
	g->build();

	std::cout << g->action_table_to_string() << std::endl;
	std::cout << g->action_table_to_string_detailed() << std::endl;
	std::cout << g->goto_table_to_string() << std::endl;

	//std::unique_ptr<parse::lr_parser> parser = std::make_unique<parse::lr_parser>(parse::lr_parser();

	
	return 0;
}
