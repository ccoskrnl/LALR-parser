#include <iostream>
#include "lr_parser.h"

int main()
{
	gram::grammar g = grammar_parser("gram_exp01.txt");

	std::cout << "Grammar:\n" << g << std::endl;
	auto lr0_states = g.build_lr0_states();
	auto tuple = g.detemine_lookaheads(lr0_states[0], g.end_marker);
	
	return 0;
}
