#include <iostream>
#include "lr_parser.h"
#include "compiler_frontend.h"

int main()
{

	compiler_frontend frontend = compiler_frontend("examples/gram_exp01.txt");
	//frontend.compile("x = 5 + 3;");
	
	return 0;
}
