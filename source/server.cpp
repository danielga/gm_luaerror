#include <server.hpp>
#include <shared.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <lua.hpp>
#include <stdint.h>
#include <GarrysMod/Interfaces.hpp>
#include <symbolfinder.hpp>
#include <hook.hpp>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <eiface.h>
#include <../game/server/player.h>

IVEngineServer *engine = nullptr;

namespace server
{

#if defined _WIN32

static const char HandleClientLuaError_sym[] =
	"\x55\x8B\xEC\x83\xEC\x08\x8B\x0D\x2A\x2A\x2A\x2A\x57\x8B\x7D\x08";
static const size_t HandleClientLuaError_symlen = sizeof( HandleClientLuaError_sym ) - 1;

#elif defined __linux

#if IS_SERVERSIDE

static const char HandleClientLuaError_sym[] = "@_Z20HandleClientLuaErrorP11CBasePlayerPKc";
static const size_t HandleClientLuaError_symlen = 0;

#else

static const char HandleClientLuaError_sym[] =
	"\x55\x89\xE5\x57\x56\x53\x83\xEC\x4C\x65\xA1\x2A\x2A\x2A\x2A\x89\x45\xE4";
static const size_t HandleClientLuaError_symlen = sizeof( HandleClientLuaError_sym ) - 1;

#endif

#elif defined __APPLE__

static const char HandleClientLuaError_sym[] = "@__Z20HandleClientLuaErrorP11CBasePlayerPKc";
static const size_t HandleClientLuaError_symlen = 0;

#endif

static const std::string main_binary = Helpers::GetBinaryFileName(
	"server",
	false,
	true,
	"garrysmod/bin/"
);
static SourceSDK::FactoryLoader engine_loader( "engine", false );
static GarrysMod::Lua::ILuaInterface *lua = nullptr;

typedef void ( *HandleClientLuaError_t )( CBasePlayer *player, const char *error );

static Detouring::Hook HandleClientLuaError_detour;

inline std::string Trim( const std::string &s )
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
	int32_t funcs = shared::PushHookRun( lua, "ClientLuaError" );
	if( funcs == 0 )
		return HandleClientLuaError_detour.GetTrampoline<HandleClientLuaError_t>( )( player, error );

	int32_t args = 2;
	lua->PushString( "ClientLuaError" );

	lua->GetField( GarrysMod::Lua::INDEX_GLOBAL, "Entity" );
	if( !lua->IsType( -1, GarrysMod::Lua::Type::FUNCTION ) )
	{
		lua->Pop( funcs + args );
		lua->ErrorNoHalt( "[ClientLuaError] Global Entity is not a function!\n" );
		return HandleClientLuaError_detour.GetTrampoline<HandleClientLuaError_t>( )( player, error );
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

	engine = engine_loader.GetInterface<IVEngineServer>( INTERFACEVERSION_VENGINESERVER );
	if( engine == nullptr )
		LUA->ThrowError( "failed to retrieve server engine interface" );

	SymbolFinder symfinder;

	void *HandleClientLuaError = symfinder.ResolveOnBinary(
		main_binary.c_str( ), HandleClientLuaError_sym, HandleClientLuaError_symlen
	);
	if( HandleClientLuaError == nullptr )
		LUA->ThrowError( "unable to sigscan function HandleClientLuaError" );

	if( !HandleClientLuaError_detour.Create(
		HandleClientLuaError, reinterpret_cast<void *>( &HandleClientLuaError_d )
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
