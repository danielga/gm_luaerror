#include <shared.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Interfaces.hpp>
#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Lua/LuaGameCallback.h>
#include <GarrysMod/Lua/AutoReference.h>
#include <filesystem_stdio.h>
#include <symbolfinder.hpp>
#include <lua.hpp>
#include <string>
#include <sstream>

namespace shared
{

static bool runtime = false;
static std::string runtime_error;
static GarrysMod::Lua::AutoReference runtime_stack;

static SourceSDK::FactoryLoader filesystem_loader( "filesystem_stdio", false, false );
static CFileSystem_Stdio *filesystem = nullptr;

static const std::string lua_shared_binary =
	Helpers::GetBinaryFileName( "lua_shared", false, IS_SERVERSIDE, "garrysmod/bin/" );

typedef int32_t ( *AdvancedLuaErrorReporter_t )( lua_State *state );
static AdvancedLuaErrorReporter_t AdvancedLuaErrorReporter = nullptr;

#if defined _WIN32

static const char AdvancedLuaErrorReporter_sym[] =
	"\x55\x8B\xEC\x81\xEC\x2A\x2A\x2A\x2A\x8B\x0D\x2A\x2A\x2A\x2A\x8B";
static const size_t AdvancedLuaErrorReporter_symlen = sizeof( AdvancedLuaErrorReporter_sym ) - 1;

#elif ( defined __linux && IS_SERVERSIDE ) || defined __APPLE__

static const char AdvancedLuaErrorReporter_sym[] = "@_Z24AdvancedLuaErrorReporterP9lua_State";
static const size_t AdvancedLuaErrorReporter_symlen = 0;

#elif ( defined __linux && !IS_SERVERSIDE )

static const char AdvancedLuaErrorReporter_sym[] =
	"\x55\x89\xE5\x57\x56\x53\x83\xEC\x2c\x8B\x15\x2A\x2A\x2A\x2A\x8B";
static const size_t AdvancedLuaErrorReporter_symlen = sizeof( AdvancedLuaErrorReporter_sym ) - 1;

#endif

#if defined LUAERROR_SERVER

static const std::string dedicated_binary =
	Helpers::GetBinaryFileName( "dedicated", false, true, "bin/" );

#if defined _WIN32

static const char FileSystemFactory_sym[] = "\x55\x8B\xEC\x68\x2A\x2A\x2A\x2A\xFF\x75\x08\xE8";
static const size_t FileSystemFactory_symlen = sizeof( FileSystemFactory_sym ) - 1;

#elif defined __linux || defined __APPLE__

static const char FileSystemFactory_sym[] = "@_Z17FileSystemFactoryPKcPi";
static const size_t FileSystemFactory_symlen = 0;

#endif

#endif

int32_t PushHookRun( GarrysMod::Lua::ILuaInterface *lua, const char *hook )
{
	lua->GetField( GarrysMod::Lua::INDEX_GLOBAL, "debug" );
	if( !lua->IsType( -1, GarrysMod::Lua::Type::TABLE ) )
	{
		lua->ErrorNoHalt( "[%s] Global debug is not a table!\n", hook );
		lua->Pop( 1 );
		return 0;
	}

	lua->GetField( -1, "traceback" );
	lua->Remove( -2 );
	if( !lua->IsType( -1, GarrysMod::Lua::Type::FUNCTION ) )
	{
		lua->ErrorNoHalt( "[%s] Global debug.traceback is not a function!\n", hook );
		lua->Pop( 1 );
		return 0;
	}

	lua->GetField( GarrysMod::Lua::INDEX_GLOBAL, "hook" );
	if( !lua->IsType( -1, GarrysMod::Lua::Type::TABLE ) )
	{
		lua->ErrorNoHalt( "[%s] Global hook is not a table!\n", hook );
		lua->Pop( 2 );
		return 0;
	}

	lua->GetField( -1, "Run" );
	lua->Remove( -2 );
	if( !lua->IsType( -1, GarrysMod::Lua::Type::FUNCTION ) )
	{
		lua->ErrorNoHalt( "[%s] Global hook.Run is not a function!\n", hook );
		lua->Pop( 2 );
		return 0;
	}

	return 2;
}

struct ErrorProperties
{
	std::string source_file;
	int32_t source_line = -1;
	std::string error_string;
};

int32_t PushErrorProperties( GarrysMod::Lua::ILuaInterface *lua, std::istringstream &error, ErrorProperties &props )
{
	std::string source_file;
	std::getline( error, source_file, ':' );

	int32_t source_line = -1;
	error >> source_line;

	error.ignore( 2 ); // ignore ": "

	std::string error_string;
	std::getline( error, error_string );

	if( !error ) // our stream is still valid
	{
		lua->PushNil( );
		lua->PushNil( );
		lua->PushNil( );
		return 3;
	}

	props.source_file = source_file;
	props.source_line = source_line;
	props.error_string = error_string;

	lua->PushString( props.source_file.c_str( ) );
	lua->PushNumber( props.source_line );
	lua->PushString( props.error_string.c_str( ) );
	return 3;
}

int32_t PushErrorProperties( GarrysMod::Lua::ILuaInterface *lua, std::istringstream &error )
{
	ErrorProperties props;
	return PushErrorProperties( lua, error, props );
}

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

static int32_t PushStackTable( GarrysMod::Lua::ILuaInterface *lua )
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

	return 1;
}

bool RunHook( GarrysMod::Lua::ILuaInterface *lua, const char *hook, int32_t args, int32_t funcs )
{
	bool call_original = true;
	if( lua->PCall( args, 1, -funcs - args ) != 0 )
		lua->ErrorNoHalt( "\n[%s] %s\n\n", hook, lua->GetString( -1 ) );
	else if( lua->IsType( -1, GarrysMod::Lua::Type::BOOL ) )
		call_original = !lua->GetBool( -1 );

	lua->Pop( funcs ); // hook.Run is popped and its result pushed
	return call_original;
}

inline std::string FindWorkshopAddonFileOwner( const std::string &source )
{
	if( source.empty( ) || source == "[C]" )
		return { };

	const auto addons = filesystem->Addons( );
	if( addons == nullptr )
		return { };

	const auto addon = addons->FindFileOwner( source );
	if( addon == nullptr )
		return { };

	return addon->title;
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
		int32_t funcs = PushHookRun( lua, "LuaError" );
		if( funcs == 0 )
			return callback->LuaError( error );

		const std::string &errstr = runtime ? runtime_error : error->message;

		lua->PushString( "LuaError" );
		lua->PushBool( runtime );
		lua->PushString( errstr.c_str( ) );

		std::istringstream errstream( errstr );
		ErrorProperties props;
		int32_t args = PushErrorProperties( lua, errstream, props );

		if( runtime )
		{
			args += 1;
			runtime_stack.Push( );
			runtime_stack.Free( );
		}
		else
			args += PushStackTable( lua );

		runtime = false;

		const std::string source_addon = FindWorkshopAddonFileOwner( props.source_file );
		if( source_addon.empty( ) )
			lua->PushNil( );
		else
			lua->PushString( source_addon.c_str( ) );

		if( RunHook( lua, "LuaError", 4 + args, funcs ) )
			return callback->LuaError( error );
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

#if defined _WIN32 || defined __linux

	static const uintptr_t CLuaGameCallback_offset = 188;

#elif defined __APPLE__

	static const uintptr_t CLuaGameCallback_offset = 192;

#endif

};

static CLuaGameCallback callback;

inline void DetourRuntime( GarrysMod::Lua::ILuaBase *LUA )
{
	LUA->PushNumber( 1 );
	LUA->PushCFunction( AdvancedLuaErrorReporter_detour );
	LUA->SetTable( GarrysMod::Lua::INDEX_REGISTRY );
}

inline void ResetRuntime( GarrysMod::Lua::ILuaBase *LUA )
{
	LUA->PushNumber( 1 );
	LUA->PushCFunction( AdvancedLuaErrorReporter );
	LUA->SetTable( GarrysMod::Lua::INDEX_REGISTRY );
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
		callback.Detour( );
	else
		callback.Reset( );

	LUA->PushBool( true );
	return 1;
}

LUA_FUNCTION_STATIC( FindWorkshopAddonFileOwnerLua )
{
	const char *path = LUA->CheckString( 1 );

	const std::string owner = FindWorkshopAddonFileOwner( path );
	if( owner.empty( ) )
		return 0;

	LUA->PushString( owner.c_str( ) );
	return 1;
}

void Initialize( GarrysMod::Lua::ILuaBase *LUA )
{
	runtime_stack.Setup( LUA );

	callback.SetLua( static_cast<GarrysMod::Lua::ILuaInterface *>( LUA ) );

	SymbolFinder symfinder;

	AdvancedLuaErrorReporter =
		reinterpret_cast<AdvancedLuaErrorReporter_t>( symfinder.ResolveOnBinary(
		lua_shared_binary.c_str( ), AdvancedLuaErrorReporter_sym, AdvancedLuaErrorReporter_symlen
	) );
	if( AdvancedLuaErrorReporter == nullptr )
		LUA->ThrowError( "unable to obtain AdvancedLuaErrorReporter" );

#if defined LUAERROR_SERVER

	CreateInterfaceFn factory = reinterpret_cast<CreateInterfaceFn>( symfinder.ResolveOnBinary(
		dedicated_binary.c_str( ), FileSystemFactory_sym, FileSystemFactory_symlen
	) );
	if( factory != nullptr )
		filesystem = static_cast<CFileSystem_Stdio *>(
			factory( FILESYSTEM_INTERFACE_VERSION, nullptr )
		);

#endif

	if( filesystem == nullptr )
		filesystem = filesystem_loader.GetInterface<CFileSystem_Stdio>(
			FILESYSTEM_INTERFACE_VERSION
		);

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
	callback.Reset( );
}

}
