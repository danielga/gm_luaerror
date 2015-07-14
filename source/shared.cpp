#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Lua/LuaGameCallback.h>
#include <GarrysMod/Lua/AutoLuaReference.h>
#include <lua.hpp>
#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include <memory>

namespace shared
{

struct LuaDebug
{
	LuaDebug( const lua_Debug &debug ) :
		event( debug.event ),
		name( debug.name != nullptr ? debug.name : "" ),
		namewhat( debug.namewhat != nullptr ? debug.namewhat : "" ),
		what( debug.what != nullptr ? debug.what : "" ),
		source( debug.source != nullptr ? debug.source : "" ),
		currentline( debug.currentline ),
		nups( debug.nups ),
		linedefined( debug.linedefined ),
		lastlinedefined( debug.lastlinedefined ),
		short_src( debug.short_src ),
		i_ci( debug.i_ci )
	{ }

	int32_t event;
	std::string name;
	std::string namewhat;
	std::string what;
	std::string source;
	int32_t currentline;
	int32_t nups;
	int32_t linedefined;
	int32_t lastlinedefined;
	std::string short_src;
	int32_t i_ci;
};

struct LuaErrorChain
{
	LuaErrorChain( ) :
		runtime( false ),
		source_line( -1 )
	{ }

	void Clear( )
	{
		runtime = false;
		source_file.clear( );
		source_line = -1;
		error_string.clear( );
		stack_data.clear( );
	}

	bool runtime;
	std::string source_file;
	int32_t source_line;
	std::string error_string;
	std::vector<LuaDebug> stack_data;
};

static GarrysMod::Lua::AutoLuaReference reporter_ref;
static LuaErrorChain lua_error_chain;

static void ParseErrorString( const std::string &error )
{
	if( error.empty( ) )
		return;

	std::istringstream strstream( error );

	std::getline( strstream, lua_error_chain.source_file, ':' );

	strstream >> lua_error_chain.source_line;

	strstream.ignore( 2 ); // remove : and <space>

	std::getline( strstream, lua_error_chain.error_string );
}

static void BuildErrorStack( GarrysMod::Lua::ILuaInterface *lua )
{
	lua_Debug dbg = { 0 };
	for( int32_t lvl = 0; lua->GetStack( lvl, &dbg ) != 0; ++lvl, memset( &dbg, 0, sizeof( dbg ) ) )
	{
		if( lua->GetInfo( "Slnu", &dbg ) == 0 )
			return;

		lua_error_chain.stack_data.push_back( dbg );
	}
}

LUA_FUNCTION_STATIC( AdvancedLuaErrorReporter )
{
	lua_error_chain.runtime = true;

	ParseErrorString( LUA->GetString( 1 ) );
	BuildErrorStack( static_cast<GarrysMod::Lua::ILuaInterface *>( LUA ) );

	reporter_ref.Push( );
	LUA->Push( 1 );
	LUA->Call( 1, 1 );
	return 1;
}

LUA_FUNCTION_STATIC( LuaErrorHookCall )
{
	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_GLOB );

	LUA->GetField( -1, "hook" );

	LUA->GetField( -1, "Run" );

	LUA->PushString( "LuaError" );

	LUA->PushBool( lua_error_chain.runtime );
	LUA->PushString( lua_error_chain.source_file.c_str( ) );
	LUA->PushNumber( lua_error_chain.source_line );
	LUA->PushString( lua_error_chain.error_string.c_str( ) );

	LUA->CreateTable( );
	for( size_t i = 0; i < lua_error_chain.stack_data.size( ); ++i )
	{
		LUA->PushNumber( i + 1 );
		LUA->CreateTable( );

		LuaDebug &stacklevel = lua_error_chain.stack_data[i];

		LUA->PushNumber( stacklevel.event );
		LUA->SetField( -2, "event" );

		LUA->PushString( stacklevel.name.c_str( ) );
		LUA->SetField( -2, "name" );

		LUA->PushString( stacklevel.namewhat.c_str( ) );
		LUA->SetField( -2, "namewhat" );

		LUA->PushString( stacklevel.what.c_str( ) );
		LUA->SetField( -2, "what" );

		LUA->PushString( stacklevel.source.c_str( ) );
		LUA->SetField( -2, "source" );

		LUA->PushNumber( stacklevel.currentline );
		LUA->SetField( -2, "currentline" );

		LUA->PushNumber( stacklevel.nups );
		LUA->SetField( -2, "nups" );

		LUA->PushNumber( stacklevel.linedefined );
		LUA->SetField( -2, "linedefined" );

		LUA->PushNumber( stacklevel.lastlinedefined );
		LUA->SetField( -2, "lastlinedefined" );

		LUA->PushString( stacklevel.short_src.c_str( ) );
		LUA->SetField( -2, "short_src" );

		LUA->PushNumber( stacklevel.i_ci );
		LUA->SetField( -2, "i_ci" );

		LUA->SetTable( -3 );
	}

	LUA->Call( 6, 1 );
	return 1;
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

	void LuaError( GarrysMod::Lua::CLuaError *error )
	{
		if( !lua_error_chain.runtime )
		{
			ParseErrorString( error->text );
			BuildErrorStack( lua );
		}

		lua->PushCFunction( LuaErrorHookCall );

		bool call_original = true;
		if( lua->PCall( 0, 1, 0 ) != 0 )
			lua->ErrorNoHalt( "[LuaError hook error] %s\n", lua->GetString( -1 ) );
		else if( lua->IsType( -1, GarrysMod::Lua::Type::BOOL ) )
			call_original = !lua->GetBool( -1 );

		lua->Pop( 1 );

		lua_error_chain.Clear( );

		if( call_original )
			callback->LuaError( error );
	}

	void InterfaceCreated( GarrysMod::Lua::ILuaInterface *iface )
	{
		callback->InterfaceCreated( iface );
	}

	void SetLua( GarrysMod::Lua::ILuaInterface *iface )
	{
		lua = iface;
		callback = *reinterpret_cast<GarrysMod::Lua::ILuaGameCallback **>(
			reinterpret_cast<uintptr_t>( lua ) + CLuaGameCallback_offset
		);
	}

	void Detour( )
	{
		*reinterpret_cast<GarrysMod::Lua::ILuaGameCallback **>(
			reinterpret_cast<uintptr_t>( lua ) + CLuaGameCallback_offset
		) = this;
	}

	void Reset( )
	{
		*reinterpret_cast<GarrysMod::Lua::ILuaGameCallback **>(
			reinterpret_cast<uintptr_t>( lua ) + CLuaGameCallback_offset
		) = callback;
	}

private:
	GarrysMod::Lua::ILuaInterface *lua;
	GarrysMod::Lua::ILuaGameCallback *callback;

#if defined _WIN32

	static const uintptr_t CLuaGameCallback_offset = 232;

#elif defined __linux

	static const uintptr_t CLuaGameCallback_offset = 232;

#elif defined __APPLE__

	static uintptr_t CLuaGameCallback_offset = 260;

#endif

};

static CLuaGameCallback callback;

inline void DetourRuntime( lua_State *state, bool enable )
{
	if( enable )
	{
		LUA->PushSpecial( GarrysMod::Lua::SPECIAL_REG );
		LUA->PushNumber( 1 );
		LUA->PushCFunction( AdvancedLuaErrorReporter );
		LUA->SetTable( -3 );
	}
	else
	{
		LUA->PushSpecial( GarrysMod::Lua::SPECIAL_REG );
		LUA->PushNumber( 1 );
		reporter_ref.Push( );
		LUA->SetTable( -3 );
	}
}

LUA_FUNCTION_STATIC( EnableRuntimeDetour )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::BOOL );
	DetourRuntime( state, LUA->GetBool( 1 ) );
	LUA->PushBool( true );
	return 1;
}

LUA_FUNCTION_STATIC( EnableCompiletimeDetour )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::BOOL );

	if( LUA->GetBool( 1 ) )
		callback.Detour( );
	else
		callback.Reset( );

	LUA->PushBool( true );
	return 1;
}

void Initialize( lua_State *state )
{
	callback.SetLua( static_cast<GarrysMod::Lua::ILuaInterface *>( LUA ) );

	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_REG );

	LUA->PushNumber( 1 );
	LUA->GetTable( -2 );
	if( LUA->IsType( -1, GarrysMod::Lua::Type::FUNCTION ) )
		reporter_ref.Create( LUA );
	else
		LUA->ThrowError( "failed to locate AdvancedLuaErrorReporter" );

	LUA->Pop( 1 );

	LUA->PushCFunction( EnableRuntimeDetour );
	LUA->SetField( -2, "EnableRuntimeDetour" );

	LUA->PushCFunction( EnableCompiletimeDetour );
	LUA->SetField( -2, "EnableCompiletimeDetour" );
}

void Deinitialize( lua_State *state )
{
	DetourRuntime( state, false );
	callback.Reset( );
	reporter_ref.Free( );
}

}