#include <cbase.h>

#undef INVALID_HANDLE_VALUE

#include <server.hpp>
#include <shared.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <interfaces.hpp>
#include <symbolfinder.hpp>
#include <detours.h>
#include <memory>
#include <sstream>

IVEngineServer *engine = nullptr;

namespace server
{

#if defined _WIN32

static const char *HandleClientLuaError_sym = "\x55\x8B\xEC\x83\xEC\x08\x8B\x0D\x2A\x2A\x2A\x2A\x8B\x11\x53\x56";
static const size_t HandleClientLuaError_symlen = 16;

#elif defined __linux

static const char *HandleClientLuaError_sym = "\x55\x89\xE5\x57\x56\x8D\x75\xE4\x53\x83\xEC\x4C\x8B\x5D\x08\x8D";
static const size_t HandleClientLuaError_symlen = 16;

#elif defined __APPLE__

static const char *HandleClientLuaError_sym = "@__Z20HandleClientLuaErrorP11CBasePlayerPKc";
static const size_t HandleClientLuaError_symlen = 0;

#endif

static const std::string main_binary = helpers::GetBinaryFileName(
	"server",
	false,
	true,
	"garrysmod/bin/"
);
static SourceSDK::FactoryLoader engine_loader( "engine", false );
static GarrysMod::Lua::ILuaInterface *lua = nullptr;

typedef void( *HandleClientLuaError_t )( CBasePlayer *player, const char *error );

static std::unique_ptr< MologieDetours::Detour<HandleClientLuaError_t> > HandleClientLuaError_detour;
static HandleClientLuaError_t HandleClientLuaError = nullptr;

static shared::LuaErrorChain lua_error_chain;

LUA_FUNCTION_STATIC( ClientLuaErrorHookCall )
{
	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_GLOB );

	LUA->GetField( -1, "hook" );
	LUA->GetField( -1, "Run" );

	LUA->PushString( "ClientLuaError" );

	LUA->GetField( -4, "Entity" );
	LUA->PushNumber( lua_error_chain.player );
	LUA->Call( 1, 1 );

	LUA->PushString( lua_error_chain.source_file.c_str( ) );
	LUA->PushNumber( lua_error_chain.source_line );
	LUA->PushString( lua_error_chain.error_string.c_str( ) );

	LUA->CreateTable( );

	for( size_t i = 0; i < lua_error_chain.stack_data.size( ); ++i )
	{
		LUA->PushNumber( i + 1 );
		LUA->CreateTable( );

		const shared::LuaDebug &stacklevel = lua_error_chain.stack_data[i];

		LUA->PushString( stacklevel.name.c_str( ) );
		LUA->SetField( -2, "name" );

		LUA->PushNumber( stacklevel.currentline );
		LUA->SetField( -2, "currentline" );

		LUA->PushString( stacklevel.short_src.c_str( ) );
		LUA->SetField( -2, "short_src" );

		LUA->PushString( stacklevel.short_src.c_str( ) );
		LUA->SetField( -2, "source" );

		LUA->SetTable( -3 );
	}

	LUA->Call( 6, 1 );
	return 1;
}

static void HandleClientLuaError_d( CBasePlayer *player, const char *error )
{
	lua->PushCFunction( ClientLuaErrorHookCall );

	std::istringstream strstream( error );

	strstream.ignore( 8 ); // ignore [ERROR] and <space>

	std::getline( strstream, lua_error_chain.source_file, ':' );

	strstream >> lua_error_chain.source_line;

	strstream.ignore( 2 ); // remove : and <space>

	std::getline( strstream, lua_error_chain.error_string );

	std::string waste;
	while( strstream.good( ) )
	{
		shared::LuaDebug dbg;

		int32_t level = 0;
		strstream >> level;

		strstream.ignore( 2 ); // ignore . and <space>

		strstream >> dbg.name;

		strstream.ignore( 3 ); // ignore <space>, - and <space>

		std::getline( strstream, dbg.short_src, ':' );

		strstream >> dbg.currentline;

		if( strstream.good( ) ) // it shouldn't have reached eof by now
			lua_error_chain.stack_data.push_back( dbg );
	}

	lua_error_chain.player = player->entindex( );

	bool call_original = true;
	if( lua->PCall( 0, 1, 0 ) != 0 )
		lua->ErrorNoHalt( "[ClientLuaError hook error] %s\n", lua->GetString( -1 ) );
	else if( lua->IsType( -1, GarrysMod::Lua::Type::BOOL ) )
		call_original = !lua->GetBool( -1 );

	lua->Pop( 1 );

	lua_error_chain.Clear( );

	if( call_original )
		HandleClientLuaError_detour->GetOriginalFunction( )( player, error );
}

LUA_FUNCTION_STATIC( EnableClientDetour )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::BOOL );

	bool enable = LUA->GetBool( 1 );
	if( enable && !HandleClientLuaError_detour )
	{
		bool errored = false;
		try
		{
			HandleClientLuaError_detour = std::make_unique< MologieDetours::Detour<HandleClientLuaError_t> >(
				HandleClientLuaError, HandleClientLuaError_d
			);
		}
		catch( const std::exception &e )
		{
			errored = true;
			LUA->PushNil( );
			LUA->PushString( e.what( ) );
		}

		if( errored )
			return 2;
	}
	else if( !enable && HandleClientLuaError_detour )
		HandleClientLuaError_detour.reset( );

	LUA->PushBool( true );
	return 1;
}

void Initialize( lua_State *state )
{
	lua = static_cast<GarrysMod::Lua::ILuaInterface *>( LUA );

	engine = engine_loader.GetInterface<IVEngineServer>( INTERFACEVERSION_VENGINESERVER_VERSION_21 );
	if( engine == nullptr )
		LUA->ThrowError( "failed to retrieve server engine interface" );

	SymbolFinder symfinder;

	HandleClientLuaError = reinterpret_cast<HandleClientLuaError_t>( symfinder.ResolveOnBinary(
			main_binary.c_str( ), HandleClientLuaError_sym, HandleClientLuaError_symlen
	) );
	if( HandleClientLuaError == nullptr )
		LUA->ThrowError( "unable to sigscan function HandleClientLuaError" );

	LUA->PushCFunction( EnableClientDetour );
	LUA->SetField( -2, "EnableClientDetour" );
}

void Deinitialize( lua_State *state )
{
	if( HandleClientLuaError_detour )
		HandleClientLuaError_detour.reset( );
}

}