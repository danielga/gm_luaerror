#include <common.hpp>

#include <cstdio>

static bool test_parsed_error( const std::string &error, const common::ParsedError &control_parsed_error )
{
	common::ParsedError parsed_error;
	if( !common::ParseError( error, parsed_error ) )
		return false;

	return parsed_error == control_parsed_error;
}

static bool test_parsed_error_with_stacktrace( const std::string &error, const common::ParsedErrorWithStackTrace &control_parsed_error )
{
	common::ParsedErrorWithStackTrace parsed_error;
	if( !common::ParseErrorWithStackTrace( error, parsed_error ) )
		return false;

	return parsed_error == control_parsed_error;
}

int main( const int, const char *[] )
{
	const std::string error1 = "lua_run:1: '=' expected near '<eof>'";
	const common::ParsedError control_parsed_error1 =
	{
		"lua_run",
		1,
		"'=' expected near '<eof>'"
	};
	if( !test_parsed_error( error1, control_parsed_error1 ) )
	{
		printf( "Failed on test case 1!\n" );
		return 5;
	}

	const std::string error2 =
		"\n"
		"[gcad] bad argument #3 to 'Add' (function expected, got nil)\n"
		"  1. Add - lua/includes/modules/hook.lua:31\n"
		"   2. unknown - addons/gcad/lua/gcad/ui/contextmenu/contextmenueventhandler.lua:150\n"
		"    3. dtor - addons/glib/lua/glib/oop/oop.lua:292\n"
		"     4. unknown - addons/gcad/lua/gcad/ui/contextmenu/contextmenueventhandler.lua:130\n"
		"      5. xpcall - [C]:-1\n"
		"       6. DispatchEvent - addons/glib/lua/glib/events/eventprovider.lua:86\n"
		"        7. UnloadSystem - addons/glib/lua/glib/stage1.lua:380\n"
		"         8. RunPackFile - addons/glib/lua/glib/loader/loader.lua:200\n"
		"          9. runNextPackFile - addons/glib/lua/glib/loader/loader.lua:494\n"
		"           10. callback - addons/glib/lua/glib/loader/loader.lua:497\n"
		"            11. RunPackFile - addons/glib/lua/glib/loader/loader.lua:296\n"
		"             12. runNextPackFile - addons/glib/lua/glib/loader/loader.lua:494\n"
		"              13. callback - addons/glib/lua/glib/loader/loader.lua:497\n"
		"               14. RunPackFile - addons/glib/lua/glib/loader/loader.lua:296\n"
		"                15. runNextPackFile - addons/glib/lua/glib/loader/loader.lua:494\n"
		"                 16. callback - addons/glib/lua/glib/loader/loader.lua:497\n"
		"\n";
	const common::ParsedErrorWithStackTrace control_parsed_error2 =
	{
		"",
		-1,
		"bad argument #3 to 'Add' (function expected, got nil)",
		"gcad",
		{
			{ 1, "Add", "lua/includes/modules/hook.lua", 31 },
			{ 2, "unknown", "addons/gcad/lua/gcad/ui/contextmenu/contextmenueventhandler.lua", 150 },
			{ 3, "dtor", "addons/glib/lua/glib/oop/oop.lua", 292 },
			{ 4, "unknown", "addons/gcad/lua/gcad/ui/contextmenu/contextmenueventhandler.lua", 130 },
			{ 5, "xpcall", "[C]", -1 },
			{ 6, "DispatchEvent", "addons/glib/lua/glib/events/eventprovider.lua", 86 },
			{ 7, "UnloadSystem", "addons/glib/lua/glib/stage1.lua", 380 },
			{ 8, "RunPackFile", "addons/glib/lua/glib/loader/loader.lua", 200 },
			{ 9, "runNextPackFile", "addons/glib/lua/glib/loader/loader.lua", 494 },
			{ 10, "callback", "addons/glib/lua/glib/loader/loader.lua", 497 },
			{ 11, "RunPackFile", "addons/glib/lua/glib/loader/loader.lua", 296 },
			{ 12, "runNextPackFile", "addons/glib/lua/glib/loader/loader.lua", 494 },
			{ 13, "callback", "addons/glib/lua/glib/loader/loader.lua", 497 },
			{ 14, "RunPackFile", "addons/glib/lua/glib/loader/loader.lua", 296 },
			{ 15, "runNextPackFile", "addons/glib/lua/glib/loader/loader.lua", 494 },
			{ 16, "callback", "addons/glib/lua/glib/loader/loader.lua", 497 },
		}
	};
	if( !test_parsed_error_with_stacktrace( error2, control_parsed_error2 ) )
	{
		printf( "Failed on test case 2!\n" );
		return 5;
	}

	const std::string error3 =
		"\n"
		"[ERROR] CompileString:1: '=' expected near '<eof>'\n"
		"  1. unknown - lua_run:1\n"
		"\n";
	const common::ParsedErrorWithStackTrace control_parsed_error3 =
	{
		"CompileString",
		1,
		"'=' expected near '<eof>'",
		"ERROR",
		{
			{ 1, "unknown", "lua_run", 1 }
		}
	};
	if( !test_parsed_error_with_stacktrace( error3, control_parsed_error3 ) )
	{
		printf( "Failed on test case 3!\n" );
		return 5;
	}

	const std::string error4 =
		"\n"
		"[ERROR] lua_run:1: yes\n"
		"  1. error - [C]:-1\n"
		"   2. err - lua_run:1\n"
		"    3. unknown - lua_run:1\n"
		"\n";
	const common::ParsedErrorWithStackTrace control_parsed_error4 =
	{
		"lua_run",
		1,
		"yes",
		"ERROR",
		{
			{ 1, "error", "[C]", -1 },
			{ 2, "err", "lua_run", 1 },
			{ 3, "unknown", "lua_run", 1 }
		}
	};
	if( !test_parsed_error_with_stacktrace( error4, control_parsed_error4 ) )
	{
		printf( "Failed on test case 4!\n" );
		return 5;
	}

	const std::string error5 =
		"\n"
		"[ERROR] lua_run:1: '=' expected near '<eof>'\n"
		"\n";
	const common::ParsedErrorWithStackTrace control_parsed_error5 =
	{
		"lua_run",
		1,
		"'=' expected near '<eof>'",
		"ERROR",
		{ }
	};
	if( !test_parsed_error_with_stacktrace( error5, control_parsed_error5 ) )
	{
		printf( "Failed on test case 5!\n" );
		return 5;
	}

	printf( "Successfully ran all test cases!\n" );
	return 0;
}
