#include "shared.hpp"
#include "common/common.hpp"

#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/Helpers.hpp>
#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Lua/LuaGameCallback.h>
#include <GarrysMod/Lua/AutoReference.h>
#include <GarrysMod/InterfacePointers.hpp>
#include <lua.hpp>

#include <cstdlib>
#include <string>
#include <sstream>
#include <regex>

#include <filesystem_stdio.h>

namespace shared
{

static bool runtime = false;
static std::string runtime_error;
static GarrysMod::Lua::AutoReference runtime_stack;
static CFileSystem_Stdio *filesystem = nullptr;
static bool runtime_detoured = false;
static bool compiletime_detoured = false;
static GarrysMod::Lua::CFunc AdvancedLuaErrorReporter = nullptr;

inline bool GetUpvalues( GarrysMod::Lua::ILuaInterface *lua, int32_t funcidx )
{
	if( funcidx < 0 )
		funcidx = lua->Top( ) + funcidx + 1;

	int32_t idx = 1;
	const char *name = lua->GetUpvalue( funcidx, idx );
	if( name == nullptr )
		return false;

	// Keep popping until we either reach the end or until we reach a valid upvalue
	while( name[0] == '\0' )
	{
		lua->Pop( 1 );

		if( ( name = lua->GetUpvalue( funcidx, ++idx ) ) == nullptr )
			return false;
	}

	lua->CreateTable( );

	// Push the last upvalue to the top
	lua->Push( -2 );
	// And remove it from its previous location
	lua->Remove( -3 );

	do
		if( name[0] != '\0' )
			lua->SetField( -2, name );
		else
			lua->Pop( 1 );
	while( ( name = lua->GetUpvalue( funcidx, ++idx ) ) != nullptr );

	return true;
}

inline bool GetLocals( GarrysMod::Lua::ILuaInterface *lua, lua_Debug &dbg )
{
	int32_t idx = 1;
	const char *name = lua->GetLocal( &dbg, idx );
	if( name == nullptr )
		return false;

	// Keep popping until we either reach the end or until we reach a valid local
	while( name[0] == '(' )
	{
		lua->Pop( 1 );

		if( ( name = lua->GetLocal( &dbg, ++idx ) ) == nullptr )
			return false;
	}

	lua->CreateTable( );

	// Push the last local to the top
	lua->Push( -2 );
	// And remove it from its previous location
	lua->Remove( -3 );

	do
		if( name[0] != '(' )
			lua->SetField( -2, name );
		else
			lua->Pop( 1 );
	while( ( name = lua->GetLocal( &dbg, ++idx ) ) != nullptr );

	return true;
}

static void PushStackTable( GarrysMod::Lua::ILuaInterface *lua )
{
	lua->CreateTable( );

	int32_t lvl = 0;
	lua_Debug dbg;
	while( lua->GetStack( lvl, &dbg ) == 1 && lua->GetInfo( "SfLlnu", &dbg ) == 1 )
	{
		lua->PushNumber( ++lvl );
		lua->CreateTable( );

		if( GetUpvalues( lua, -4 ) )
			lua->SetField( -2, "upvalues" );

		if( GetLocals( lua, dbg ) )
			lua->SetField( -2, "locals" );

		lua->Push( -4 );
		lua->SetField( -2, "func" );

		lua->Push( -3 );
		lua->SetField( -2, "activelines" );

		lua->PushNumber( dbg.event );
		lua->SetField( -2, "event" );

		lua->PushString( dbg.name != nullptr ? dbg.name : "" );
		lua->SetField( -2, "name" );

		lua->PushString( dbg.namewhat != nullptr ? dbg.namewhat : "" );
		lua->SetField( -2, "namewhat" );

		lua->PushString( dbg.what != nullptr ? dbg.what : "" );
		lua->SetField( -2, "what" );

		lua->PushString( dbg.source != nullptr ? dbg.source : "" );
		lua->SetField( -2, "source" );

		lua->PushNumber( dbg.currentline );
		lua->SetField( -2, "currentline" );

		lua->PushNumber( dbg.nups );
		lua->SetField( -2, "nups" );

		lua->PushNumber( dbg.linedefined );
		lua->SetField( -2, "linedefined" );

		lua->PushNumber( dbg.lastlinedefined );
		lua->SetField( -2, "lastlinedefined" );

		lua->PushString( dbg.short_src );
		lua->SetField( -2, "short_src" );

		lua->SetTable( -5 );

		// Pop activelines and func
		lua->Pop( 2 );
	}
}

inline const IAddonSystem::Information *FindWorkshopAddonFromFile( const std::string &source )
{
	if( source.empty( ) || source == "[C]" )
		return nullptr;

	const auto addons = filesystem->Addons( );
	if( addons == nullptr )
		return nullptr;

	return addons->FindFileOwner( source );
}

LUA_FUNCTION_STATIC( AdvancedLuaErrorReporter_detour )
{
	const char *errstr = LUA->GetString( 1 );

	auto lua = static_cast<GarrysMod::Lua::ILuaInterface *>( LUA );

	runtime = true;

	if( errstr != nullptr )
		runtime_error = errstr;
	else
		runtime_error.clear( );

	PushStackTable( lua );
	runtime_stack.Create( );

	return AdvancedLuaErrorReporter( LUA->GetState( ) );
}

class CLuaGameCallback : public GarrysMod::Lua::ILuaGameCallback
{
public:
	CLuaGameCallback( ) :
		lua( nullptr ),
		callback( nullptr )
	{ }

	~CLuaGameCallback( )
	{
		Reset( );
	}

	GarrysMod::Lua::ILuaObject *CreateLuaObject( )
	{
		return callback->CreateLuaObject( );
	}

	void DestroyLuaObject( GarrysMod::Lua::ILuaObject *obj )
	{
		callback->DestroyLuaObject( obj );
	}

	void ErrorPrint( const char *error, bool print )
	{
		callback->ErrorPrint( error, print );
	}

	void Msg( const char *msg, bool useless )
	{
		callback->Msg( msg, useless );
	}

	void MsgColour( const char *msg, const Color &color )
	{
		callback->MsgColour( msg, color );
	}

	void LuaError( const CLuaError *error )
	{
		const std::string &error_str = runtime ? runtime_error : error->message;

		common::ParsedError parsed_error;
		if( entered_hook || !common::ParseError( error_str, parsed_error ) )
			return callback->LuaError( error );

		const int32_t funcs = LuaHelpers::PushHookRun( lua, "LuaError" );
		if( funcs == 0 )
			return callback->LuaError( error );

		lua->PushBool( runtime );
		lua->PushString( error_str.c_str( ) );

		lua->PushString( parsed_error.source_file.c_str( ) );
		lua->PushNumber( parsed_error.source_line );
		lua->PushString( parsed_error.error_string.c_str( ) );

		if( runtime )
		{
			runtime_stack.Push( );
			runtime_stack.Free( );
		}
		else
			PushStackTable( lua );

		runtime = false;

		const auto source_addon = FindWorkshopAddonFromFile( parsed_error.source_file );
		if( source_addon == nullptr )
		{
			lua->PushNil( );
			lua->PushNil( );
		}
		else
		{
			lua->PushString( source_addon->title.c_str( ) );
			lua->PushString( std::to_string( source_addon->wsid ).c_str( ) );
		}

		entered_hook = true;
		const bool call_success = LuaHelpers::CallHookRun( lua, 8, 1 );
		entered_hook = false;
		if( !call_success )
			return callback->LuaError( error );

		const bool proceed = !lua->IsType( -1, GarrysMod::Lua::Type::BOOL ) || !lua->GetBool( -1 );
		lua->Pop( 1 );
		if( proceed )
			return callback->LuaError( error );
	}

	void InterfaceCreated( GarrysMod::Lua::ILuaInterface *iface )
	{
		callback->InterfaceCreated( iface );
	}

	void SetLua( GarrysMod::Lua::ILuaInterface *iface )
	{
		lua = static_cast<GarrysMod::Lua::CLuaInterface *>( iface );
		callback = lua->GetLuaGameCallback( );
	}

	void Detour( )
	{
		lua->SetLuaGameCallback( this );
	}

	void Reset( )
	{
		lua->SetLuaGameCallback( callback );
	}

private:
	GarrysMod::Lua::CLuaInterface *lua;
	GarrysMod::Lua::ILuaGameCallback *callback;
	bool entered_hook = false;
};

static CLuaGameCallback callback;

inline void DetourCompiletime( )
{
	if( compiletime_detoured )
		return;

	if( !runtime_detoured )
		callback.Detour( );

	compiletime_detoured = true;
}

inline void ResetCompiletime( )
{
	if( !compiletime_detoured )
		return;

	if( !runtime_detoured )
		callback.Reset( );

	compiletime_detoured = false;
}

inline void DetourRuntime( GarrysMod::Lua::ILuaBase *LUA )
{
	if( runtime_detoured )
		return;

	if( !compiletime_detoured )
		callback.Detour( );

	LUA->PushNumber( 1 );
	LUA->PushCFunction( AdvancedLuaErrorReporter_detour );
	LUA->SetTable( GarrysMod::Lua::INDEX_REGISTRY );
	runtime_detoured = true;
}

inline void ResetRuntime( GarrysMod::Lua::ILuaBase *LUA )
{
	if( !runtime_detoured )
		return;

	if( !compiletime_detoured )
		callback.Reset( );

	LUA->PushNumber( 1 );
	LUA->PushCFunction( AdvancedLuaErrorReporter );
	LUA->SetTable( GarrysMod::Lua::INDEX_REGISTRY );
	runtime_detoured = false;
}

LUA_FUNCTION_STATIC( EnableRuntimeDetour )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::BOOL );

	if( LUA->GetBool( 1 ) )
		DetourRuntime( LUA );
	else
		ResetRuntime( LUA );

	LUA->PushBool( true );
	return 1;
}

LUA_FUNCTION_STATIC( EnableCompiletimeDetour )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::BOOL );

	if( LUA->GetBool( 1 ) )
		DetourCompiletime( );
	else
		ResetCompiletime( );

	LUA->PushBool( true );
	return 1;
}

LUA_FUNCTION_STATIC( FindWorkshopAddonFileOwnerLua )
{
	const char *path = LUA->CheckString( 1 );

	const auto owner = FindWorkshopAddonFromFile( path );
	if( owner == nullptr )
		return 0;

	LUA->PushString( owner->title.c_str( ) );
	LUA->PushString( std::to_string( owner->wsid ).c_str( ) );
	return 2;
}

void Initialize( GarrysMod::Lua::ILuaBase *LUA )
{
	runtime_stack.Setup( LUA );

	callback.SetLua( static_cast<GarrysMod::Lua::ILuaInterface *>( LUA ) );

	LUA->ReferencePush( 1 );
	if( !LUA->IsType( -1, GarrysMod::Lua::Type::FUNCTION ) )
		LUA->ThrowError( "reference to AdvancedLuaErrorReporter is invalid" );

	AdvancedLuaErrorReporter = LUA->GetCFunction( -1 );
	if( AdvancedLuaErrorReporter == nullptr )
		LUA->ThrowError( "unable to obtain AdvancedLuaErrorReporter" );

	LUA->Pop( 1 );

	filesystem = static_cast<CFileSystem_Stdio *>( InterfacePointers::FileSystem( ) );
	if( filesystem == nullptr )
		LUA->ThrowError( "unable to initialize IFileSystem" );

	LUA->PushCFunction( EnableRuntimeDetour );
	LUA->SetField( -2, "EnableRuntimeDetour" );

	LUA->PushCFunction( EnableCompiletimeDetour );
	LUA->SetField( -2, "EnableCompiletimeDetour" );

	LUA->PushCFunction( FindWorkshopAddonFileOwnerLua );
	LUA->SetField( -2, "FindWorkshopAddonFileOwner" );
}

void Deinitialize( GarrysMod::Lua::ILuaBase *LUA )
{
	ResetRuntime( LUA );
	ResetCompiletime( );
}

}
