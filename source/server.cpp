#include <cbase.h>

#undef INVALID_HANDLE_VALUE

#undef min
#undef max

#include <server.hpp>
#include <shared.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <lua.hpp>
#include <cstdint>
#include <interfaces.hpp>
#include <symbolfinder.hpp>
#include <detours.h>
#include <memory>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cctype>

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

static inline std::string Trim( const std::string &s )
{
	std::string c = s;
	c.erase(
		std::find_if(
			c.rbegin( ),
			c.rend( ),
			std::not1( std::ptr_fun<int, int>( std::isspace ) )
		).base( ),
		c.end( )
	); // remote trailing "spaces"
	c.erase(
		c.begin( ),
		std::find_if(
			c.begin( ),
			c.end( ),
			std::not1( std::ptr_fun<int, int>( std::isspace ) )
		)
	); // remote initial "spaces"
	return c;
}

static void HandleClientLuaError_d( CBasePlayer *player, const char *error )
{
	int32_t funcs = shared::PushHookRun( lua, "ClientLuaError", false );
	if( funcs == 0 )
		return HandleClientLuaError_detour->GetOriginalFunction( )( player, error );

	int32_t args = 2;
	lua->PushString( "ClientLuaError" );

	lua->GetField( -funcs - args, "Entity" ); // get Entity function from global table
	lua->Remove( -funcs - args - 1 ); // remove global table
	if( !lua->IsType( -1, GarrysMod::Lua::Type::FUNCTION ) )
	{
		lua->Pop( funcs + args );
		lua->ErrorNoHalt( "[ClientLuaError] Global Entity is not a function!\n" );
		return HandleClientLuaError_detour->GetOriginalFunction( )( player, error );
	}
	lua->PushNumber( player->entindex( ) );
	lua->Call( 1, 1 );

	std::string cleanerror = Trim( error );
	if( cleanerror.compare( 0, 8, "[ERROR] " ) == 0 )
		cleanerror = cleanerror.erase( 0, 8 );

	args += 2;
	lua->PushString( cleanerror.c_str( ) );

	std::istringstream errstream( cleanerror );
	args += shared::PushErrorProperties( lua, errstream );

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

	if( shared::RunHook( lua, "ClientLuaError", args, funcs ) )
		return HandleClientLuaError_detour->GetOriginalFunction( )( player, error );
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