#include <iostream>
#include "lr_parser.h"
#include "compiler_frontend.h"

int main()
{

	compiler_frontend frontend = compiler_frontend("examples/gram_exp02.txt");
	//frontend.compile("x = 5 + 3;");
	frontend.compile("a 5 3.3");
	
	return 0;
}
