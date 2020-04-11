// LX::FileName unit tests

#include <regex>

#include "lx/xutils.h"

#include "ddt/sharedLog.h"

#include "ddt/FileSystem.h"

#define CATCH_BREAK_INTO_DEBUGGER() 	LX::xtrap();
#include "catch.hpp"

#if (defined(__clang__) && __clang__)
	// clang
	#pragma GCC diagnostic ignored "-Wunused-function"
#else
	// ASSUME gcc
	#pragma GCC diagnostic ignored "-Wunused-function"
	// else check __GNUC__ / __GNU_MINOR__
#endif

using namespace std;
using namespace LX;
using namespace DDT;
using namespace LX::FileName;

//---- std::regex -------------------------------------------------------------

static
void	test_regex(void)
{
	string	s("this subject has a submarine as a subsequence");
	
	const regex	re { "\\b(sub)([^ ]*)" };
	
	uLog(UNIT, "REGEX Target sequence: %S", s);
	
	smatch	matches;
	
	while (regex_search(s, matches/*&*/, re))
	{
		const string	unmatched_s = matches.prefix().str();
		uLog(UNIT, "  unmatched %S", unmatched_s);
		
		for (const auto x : matches)
			uLog(UNIT, "    %S", x.str());
		
		uLog(UNIT, "");
		
		// string AFTER search
		s = matches.suffix().str();
	}
}

//==== UNIT TEST ==============================================================

TEST_CASE("check FileName ops", "[filename]")
{
	uLog(UNIT, "TEST_CASE_xxx begin");
	
	SECTION("SplitUnixPath rel dir", "")
	{
		const SplitPath	split = SplitUnixPath("sub_dir/ddt_demo.lua");
		
		REQUIRE(split);
		REQUIRE(split.Path() == "sub_dir/");
		REQUIRE(split.Name() == "ddt_demo");
		REQUIRE(split.Ext() == "lua");
		REQUIRE(!split.IsAbsolute());
	}
	
	SECTION("SplitUnixPath no dir", "")
	{
		const SplitPath	split = SplitUnixPath("ddt_demo.lua");
		
		REQUIRE(split);
		REQUIRE(split.Path() == "");
		REQUIRE(split.Name() == "ddt_demo");
		REQUIRE(split.Ext() == "lua");
		REQUIRE(!split.IsAbsolute());
	}
	
	SECTION("SplitUnixPath full path", "")
	{
		const SplitPath	split = SplitUnixPath("/home/plaufenb/development/inhance_depot/DDT3/lua/ddt_demo.lua");
		
		REQUIRE(split);
		REQUIRE(split.Path() == "/home/plaufenb/development/inhance_depot/DDT3/lua/");
		REQUIRE(split.Name() == "ddt_demo");
		REQUIRE(split.Ext() == "lua");
		REQUIRE(split.IsAbsolute());
	}
	
	SECTION("MakeFullUnixPath", "")
	{
		const string	s = MakeFullUnixPath("/home/plaufenb/development/inhance_depot/DDT3/lua/", "ddt_demo.lua");
		
		REQUIRE(s == "/home/plaufenb/development/inhance_depot/DDT3/lua/ddt_demo.lua");
	}
	
	SECTION("MakeFullUnixPath duplicate separators", "")
	{
		const string	s = MakeFullUnixPath("/home/plaufenb//development////inhance_depot/DDT3/lua/", "/ddt_demo.lua");
		
		REQUIRE(s == "/home/plaufenb/development/inhance_depot/DDT3/lua/ddt_demo.lua");
	}
	
	SECTION("Path TILDE resolution", "")
	{
		const string	s = Normalize("~");
		
		REQUIRE(s == "/home/plaufenb");
	}
	
	SECTION("Path HOME resolution", "")
	{
		const string	s = Normalize("$HOME/development");
		
		REQUIRE(s == "/home/plaufenb/development");
	}
	
	SECTION("Path RND resolution", "")
	{
		const string	s = Normalize("$RND");
		
		REQUIRE(s.length() == 16);
	}
	
	uLog(UNIT, "TEST_CASE_xxx end");
}

// nada mas
