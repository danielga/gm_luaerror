#include <GarrysMod/Lua/Interface.h>
#include <shared.hpp>

#if defined LUAERROR_SERVER

#include <server.hpp>

#endif

GMOD_MODULE_OPEN( )
{
	LUA->CreateTable( );

	LUA->PushString( "luaerror 1.2.1" );
	LUA->SetField( -2, "Version" );

	// version num follows LuaJIT style, xxyyzz
	LUA->PushNumber( 10201 );
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
