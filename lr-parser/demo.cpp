#include <iostream>
#include "lr_parser.h"

int main()
{
	std::unique_ptr<gram::grammar> g = grammar_parser("gram_exp01.txt");

	std::cout << "Grammar:\n" << g << std::endl;
	g->build();

	
	return 0;
}
