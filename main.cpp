
#include <gllpp/Gllpp.h>
#include <iostream>
#include <algorithm>

using namespace Gllpp;

int main() {
	Parser grammar;

	auto function = "fn"_t + Capture<'{', ' '>() + "{"_t + "}"_t;
	function.set_name("[function]");

	auto strct = "struct"_t + Capture<'{', ' '>() + "{"_t + "}"_t;
	strct.set_name("[struct]");

	auto number = Capture<' '>();

	auto topLevelDefinition =
		  number + "+"_t + number
		| number + "*"_t + number;

	grammar = SetLayout(topLevelDefinition + Optional(grammar), " ");

	auto code = "1 + 2 * 3";

	{
		auto parseResults = grammar.parse(code);

		for (auto& parseResult : parseResults) {
			std::visit([](auto&& result) {
				using T = std::decay_t<decltype(result)>;
				if constexpr (std::is_same_v<T, ParserFailure>) {
					std::cout << "  " << result.description << std::endl
						<< "  " << result.trail << std::endl;
				}
			}, parseResult);
		}
	}

	system("pause");
	return 0;
}

