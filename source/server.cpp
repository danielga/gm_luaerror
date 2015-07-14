#include <cbase.h>

#undef INVALID_HANDLE_VALUE

#include <server.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <interfaces.hpp>
#include <symbolfinder.hpp>
#include <detours.h>
#include <memory>

IVEngineServer *engine = nullptr;

namespace server
{

#if defined _WIN32

static const char *HandleClientLuaError_sym = "\x55\x89\xE5\x57\x56\x8D\x7D\xE0\x53\x83\xEC\x4C\x65\xA1\x2A\x2A";
static const size_t HandleClientLuaError_symlen = 16;

#elif defined __linux

static const char *HandleClientLuaError_sym = "\x55\x8B\xEC\x83\xEC\x08\x8B\x0D\x2A\x2A\x2A\x2A\x8B\x11\x53\x56";
static const size_t HandleClientLuaError_symlen = 16;

#elif defined __APPLE__

static const char *HandleClientLuaError_sym = "@__Z20HandleClientLuaErrorP11CBasePlayerPKc";
static const size_t HandleClientLuaError_symlen = o;

#endif

static std::string main_binary = helpers::GetBinaryFileName( "server", false, true, "garrysmod/bin/" );
static SourceSDK::FactoryLoader engine_loader( "engine", false );
static GarrysMod::Lua::ILuaInterface *lua = nullptr;

typedef void( *HandleClientLuaError_t )( CBasePlayer *player, const char *error );

static std::unique_ptr< MologieDetours::Detour<HandleClientLuaError_t> > HandleClientLuaError_detour;
static HandleClientLuaError_t HandleClientLuaError = nullptr;

LUA_FUNCTION_STATIC( ClientLuaErrorHookCall )
{
	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_GLOB );

	LUA->GetField( -1, "hook" );
	LUA->GetField( -1, "Run" );

	LUA->PushString( "ClientLuaError" );

	LUA->GetField( -4, "Entity" );
	LUA->Push( 1 );
	LUA->Call( 1, 1 );

	LUA->Push( 2 );

	LUA->Call( 3, 1 );
	return 1;
}

static void HandleClientLuaError_d( CBasePlayer *player, const char *error )
{
	lua->PushCFunction( ClientLuaErrorHookCall );

	lua->PushNumber( reinterpret_cast<CBaseEntity *>( player )->entindex( ) );
	lua->PushString( error );

	bool call_original = true;
	if( lua->PCall( 2, 1, 0 ) != 0 )
		lua->ErrorNoHalt( "[ClientLuaError hook error] %s\n", lua->GetString( -1 ) );
	else if( lua->IsType( -1, GarrysMod::Lua::Type::BOOL ) )
		call_original = !lua->GetBool( -1 );

	lua->Pop( 1 );

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

	CreateInterfaceFn engine_factory = engine_loader.GetFactory( );
	if( engine_factory == nullptr )
		LUA->ThrowError( "failed to retrieve engine factory function" );

	engine = static_cast<IVEngineServer *>(
		engine_factory( INTERFACEVERSION_VENGINESERVER_VERSION_21, nullptr )
	);
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