#include <GarrysMod/Lua/Interface.h>
#include <shared.hpp>

#if defined LUAERROR_SERVER

#include <server.hpp>

#endif

GMOD_MODULE_OPEN( )
{
	LUA->CreateTable( );

	LUA->PushString( "luaerror 1.5.0" );
	LUA->SetField( -2, "Version" );

	// version num follows LuaJIT style, xxyyzz
	LUA->PushNumber( 10500 );
	LUA->SetField( -2, "VersionNum" );

#if defined LUAERROR_SERVER

	server::Initialize( LUA );

#endif

	shared::Initialize( LUA );

	LUA->SetField( GarrysMod::Lua::INDEX_GLOBAL, "luaerror" );
	return 0;
}

GMOD_MODULE_CLOSE( )
{
	shared::Deinitialize( LUA );

#if defined LUAERROR_SERVER

	server::Deinitialize( LUA );

#endif

	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_GLOBAL, "luaerror" );
	return 0;
}
