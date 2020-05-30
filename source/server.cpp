#include "server.hpp"
#include "shared.hpp"

#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Lua/Helpers.hpp>
#include <GarrysMod/InterfacePointers.hpp>
#include <GarrysMod/FunctionPointers.hpp>

#include <detouring/hook.hpp>

#include <cstdint>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <regex>

#include <eiface.h>
#include <player.h>

#undef isspace

IVEngineServer *engine = nullptr;

namespace server
{

static GarrysMod::Lua::ILuaInterface *lua = nullptr;

static std::regex client_error_addon_matcher( "^\\[(.+)\\] ", std::regex_constants::optimize );

typedef void ( *HandleClientLuaError_t )( CBasePlayer *player, const char *error );

static Detouring::Hook HandleClientLuaError_detour;

inline std::string Trim( const std::string &s )
{
	std::string c = s;
	auto not_isspace = std::not_fn( isspace );
	// remote trailing "spaces"
	c.erase( std::find_if( c.rbegin( ), c.rend( ), not_isspace ).base( ), c.end( ) );
	// remote initial "spaces"
	c.erase( c.begin( ), std::find_if( c.begin( ), c.end( ), not_isspace ) );
	return c;
}

static void HandleClientLuaError_d( CBasePlayer *player, const char *error )
{
	const int32_t funcs = LuaHelpers::PushHookRun( lua, "ClientLuaError" );
	if( funcs == 0 )
		return HandleClientLuaError_detour.GetTrampoline<HandleClientLuaError_t>( )( player, error );

	lua->GetField( GarrysMod::Lua::INDEX_GLOBAL, "Entity" );
	if( !lua->IsType( -1, GarrysMod::Lua::Type::FUNCTION ) )
	{
		lua->Pop( funcs + 2 );
		lua->ErrorNoHalt( "[ClientLuaError] Global Entity is not a function!\n" );
		return HandleClientLuaError_detour.GetTrampoline<HandleClientLuaError_t>( )( player, error );
	}
	lua->PushNumber( player->entindex( ) );
	lua->Call( 1, 1 );

	std::string cleanerror = Trim( error );
	std::string addon;
	std::smatch matches;
	if( std::regex_search( cleanerror, matches, client_error_addon_matcher ) )
	{
		addon = matches[1];
		cleanerror.erase( 0, 1 + addon.size( ) + 1 + 1 ); // [addon]:space:
	}

	lua->PushString( cleanerror.c_str( ) );

	std::istringstream errstream( cleanerror );
	const int32_t errorPropsCount = shared::PushErrorProperties( lua, errstream );

	lua->CreateTable( );
	while( errstream )
	{
		int32_t level = 0;
		errstream >> level;

		errstream.ignore( 2 ); // ignore ". "

		std::string name;
		errstream >> name;

		errstream.ignore( 3 ); // ignore " - "

		std::string source;
		std::getline( errstream, source, ':' );

		int32_t currentline = -1;
		errstream >> currentline;

		if( !errstream ) // it shouldn't have reached eof by now
			break;

		lua->PushNumber( level );
		lua->CreateTable( );

		lua->PushString( name.c_str( ) );
		lua->SetField( -2, "name" );

		lua->PushNumber( currentline );
		lua->SetField( -2, "currentline" );

		lua->PushString( source.c_str( ) );
		lua->SetField( -2, "source" );

		lua->SetTable( -3 );
	}

	if( addon.empty( ) )
		lua->PushNil( );
	else
		lua->PushString( addon.c_str( ) );

	if( !LuaHelpers::CallHookRun( lua, 4 + errorPropsCount, 1 ) )
		return HandleClientLuaError_detour.GetTrampoline<HandleClientLuaError_t>( )( player, error );

	const bool proceed = !lua->IsType( -1, GarrysMod::Lua::Type::BOOL ) || !lua->GetBool( -1 );
	lua->Pop( 1 );
	if( proceed )
		return HandleClientLuaError_detour.GetTrampoline<HandleClientLuaError_t>( )( player, error );
}

LUA_FUNCTION_STATIC( EnableClientDetour )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::BOOL );
	LUA->PushBool( LUA->GetBool( 1 ) ?
		HandleClientLuaError_detour.Enable( ) :
		HandleClientLuaError_detour.Disable( ) );
	return 1;
}

void Initialize( GarrysMod::Lua::ILuaBase *LUA )
{
	lua = static_cast<GarrysMod::Lua::ILuaInterface *>( LUA );

	engine = InterfacePointers::VEngineServer( );
	if( engine == nullptr )
		LUA->ThrowError( "failed to retrieve server engine interface" );

	const auto HandleClientLuaError = FunctionPointers::HandleClientLuaError( );
	if( HandleClientLuaError == nullptr )
		LUA->ThrowError( "unable to sigscan function HandleClientLuaError" );

	if( !HandleClientLuaError_detour.Create(
		Detouring::Hook::Target( reinterpret_cast<void *>( HandleClientLuaError ) ),
		reinterpret_cast<void *>( &HandleClientLuaError_d )
	) )
		LUA->ThrowError( "unable to create a hook for HandleClientLuaError" );

	LUA->PushCFunction( EnableClientDetour );
	LUA->SetField( -2, "EnableClientDetour" );
}

void Deinitialize( GarrysMod::Lua::ILuaBase * )
{
	HandleClientLuaError_detour.Destroy( );
}

}
