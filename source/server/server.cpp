#include "server.hpp"
#include "common/common.hpp"

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

typedef void ( *HandleClientLuaError_t )( CBasePlayer *player, const char *error );

static Detouring::Hook HandleClientLuaError_detour;

static void HandleClientLuaError_d( CBasePlayer *player, const char *error )
{
	common::ParsedErrorWithStackTrace parsed_error;
	if( !common::ParseErrorWithStackTrace( error, parsed_error ) )
		return HandleClientLuaError_detour.GetTrampoline<HandleClientLuaError_t>( )( player, error );

	const int32_t funcs = LuaHelpers::PushHookRun( lua, "ClientLuaError" );
	if( funcs == 0 )
		return HandleClientLuaError_detour.GetTrampoline<HandleClientLuaError_t>( )( player, error );

	lua->GetField( GarrysMod::Lua::INDEX_GLOBAL, "Entity" );
	if( !lua->IsType( -1, GarrysMod::Lua::Type::FUNCTION ) )
	{
		lua->Pop( funcs + 1 );
		lua->ErrorNoHalt( "[ClientLuaError] Global Entity is not a function!\n" );
		return HandleClientLuaError_detour.GetTrampoline<HandleClientLuaError_t>( )( player, error );
	}
	lua->PushNumber( player->entindex( ) );
	lua->Call( 1, 1 );

	lua->PushString( error );

	lua->PushString( parsed_error.source_file.c_str( ) );
	lua->PushNumber( parsed_error.source_line );
	lua->PushString( parsed_error.error_string.c_str( ) );

	lua->CreateTable( );
	for( const auto &stack_frame : parsed_error.stack_trace )
	{
		lua->PushNumber( stack_frame.level );
		lua->CreateTable( );

		lua->PushString( stack_frame.name.c_str( ) );
		lua->SetField( -2, "name" );

		lua->PushNumber( stack_frame.currentline );
		lua->SetField( -2, "currentline" );

		lua->PushString( stack_frame.source.c_str( ) );
		lua->SetField( -2, "source" );

		lua->SetTable( -3 );
	}

	if( parsed_error.addon_name.empty( ) )
		lua->PushNil( );
	else
		lua->PushString( parsed_error.addon_name.c_str( ) );

	if( !LuaHelpers::CallHookRun( lua, 7, 1 ) )
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

	const auto HandleClientLuaError = FunctionPointers::CBasePlayer_HandleClientLuaError( );
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
