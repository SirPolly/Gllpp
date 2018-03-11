
#include <gllpp/Gllpp.h>
#include <iostream>

int main()
{
	Lexer l;

#if 0
	{
		Lexer::Result result{ "", "class test" };
		while (!result.trail.empty())
		{
			result = l.lex(result.trail);
			std::cout << "'" << result.token << "' '" << result.trail << "'" << std::endl;
		}
	}
#endif
	{
		Parser grammar;

		auto function = "def"_t + Capture() + "{"_t + "}"_t;
		function.set_name("[function]");

		auto cls = "class"_t + Capture() + "{"_t + "}"_t;
		cls.set_name("[class]");

		auto topLevelDefinition = function | cls;
		

		grammar = topLevelDefinition + (grammar | Empty());

		auto parseResults = grammar.parse(l, "def test { } class cls { }");
		for (auto& parseResult : parseResults) {
			std::cout << parseResult.type << " '" << parseResult.trail << "'" << std::endl;
		}
	}

    return 0;
}

