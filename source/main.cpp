#include <GarrysMod/Lua/Interface.h>
#include <shared.hpp>

#if defined LUAERROR_SERVER

#include <server.hpp>

#endif

#if defined _WIN32 && _MSC_VER != 1600

#error The only supported compilation platform for this project on Windows is Visual Studio 2010 (for ABI reasons).

#elif defined __linux && (__GNUC__ != 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 4))

#error The only supported compilation platforms for this project on Linux are GCC 4.4 to 4.9 (for ABI reasons).

#elif defined __APPLE__

#include <AvailabilityMacros.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED > 1050

#error The only supported compilation platform for this project on Mac OS X is GCC with Mac OS X 10.5 SDK (for ABI reasons).

#endif

#endif

GMOD_MODULE_OPEN( )
{
	LUA->CreateTable( );

	LUA->PushString( "luaerror 1.2.3" );
	LUA->SetField( -2, "Version" );

	// version num follows LuaJIT style, xxyyzz
	LUA->PushNumber( 10203 );
	LUA->SetField( -2, "VersionNum" );

#if defined LUAERROR_SERVER

	server::Initialize( state );

#endif

	shared::Initialize( state );

	LUA->SetField( GarrysMod::Lua::INDEX_GLOBAL, "luaerror" );
	return 0;
}

GMOD_MODULE_CLOSE( )
{
	shared::Deinitialize( state );

#if defined LUAERROR_SERVER

	server::Deinitialize( state );

#endif

	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_GLOBAL, "luaerror" );
	return 0;
}
