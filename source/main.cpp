#include <GarrysMod/Lua/Interface.h>
#include <shared.hpp>

#if defined LUAERROR_SERVER

#include <server.hpp>

#endif

GMOD_MODULE_OPEN( )
{
	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_GLOB );

	LUA->CreateTable( );

	LUA->PushString( "luaerror 1.2.0" );
	LUA->SetField( -2, "Version" );

	// version num follows LuaJIT style, xxyyzz
	LUA->PushNumber( 10200 );
	LUA->SetField( -2, "VersionNum" );

#if defined LUAERROR_SERVER

	server::Initialize( state );

#endif

	shared::Initialize( state );

	LUA->SetField( -2, "luaerror" );

	LUA->Pop( 1 );
	return 0;
}

GMOD_MODULE_CLOSE( )
{
	shared::Deinitialize( state );

#if defined LUAERROR_SERVER

	server::Deinitialize( state );

#endif

	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_GLOB );

	LUA->PushNil( );
	LUA->SetField( -2, "luaerror" );

	LUA->Pop( 1 );
	return 0;
}
