#include <iostream>
#include "lr_parser.h"
#include "compiler_frontend.h"

int main()
{

	compiler_frontend frontend = compiler_frontend("examples/gram_exp03.txt");
	//frontend.compile("10 / 2 + 6 * 3 - 1");
	//frontend.compile("**x = *y");
	frontend.compile("examples/gram_exp03_test.txt", true);


	//std::unique_ptr<parse::grammar> g = grammar_parser("gram_exp03.txt");

	//std::cout << "Grammar:\n" << *g << std::endl;
	//g->build();

	//std::cout << g->action_table_to_string() << std::endl;
	//std::cout << g->action_table_to_string_detailed() << std::endl;
	//std::cout << g->goto_table_to_string() << std::endl;

	//std::unique_ptr<parse::lr_parser> parser = std::make_unique<parse::lr_parser>(parse::lr_parser();

	
	return 0;
}
