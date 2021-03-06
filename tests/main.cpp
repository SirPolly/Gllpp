
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "gllpp/Gllpp.h"

using namespace Gllpp;

TEST_CASE("Basic", "[parse]") {
	Parser grammar;

	auto function = "def"_t + Capture<'{'>() + "{"_t + "}"_t;
	auto cls = "struct"_t + Capture<'{'>() + "{"_t + "}"_t;
	auto topLevelDefinition = (function | cls) + optional(grammar);
	grammar = layout(topLevelDefinition, " \t\r\n");

	auto code = "def test {}\n"
		"struct cls {}";

	auto parseResults = grammar.parse(code);
	REQUIRE(parseResults.size() == 1);
	REQUIRE(parseResults[0].is_success());
}

TEST_CASE("Direct Left-Recursion", "[parse]") {
	Parser E;
	E = optional(E) + "n"_t;

	auto parseResults = E.parse("nnn");

	REQUIRE(parseResults.size() == 1);
	REQUIRE(parseResults[0].is_success());
}