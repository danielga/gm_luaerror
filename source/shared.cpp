#include <shared.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Lua/LuaGameCallback.h>
#include <GarrysMod/Lua/AutoReference.h>
#include <lua.hpp>
#include <string>
#include <sstream>

#include <filesystem_stdio.h>
#include <GarrysMod/Interfaces.hpp>

#if defined LUAERROR_SERVER

#include <symbolfinder.hpp>

#endif

namespace shared
{

static GarrysMod::Lua::AutoReference reporter_ref;

static bool runtime = false;
static std::string runtime_error;
static GarrysMod::Lua::AutoReference runtime_stack;

static SourceSDK::FactoryLoader filesystem_loader( "filesystem_stdio", false, false );
static CFileSystem_Stdio *filesystem = nullptr;

#if defined LUAERROR_SERVER

static std::string dedicated_binary =
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

static int32_t PushStackTable( GarrysMod::Lua::ILuaInterface *lua )
{
	lua->CreateTable( );

	int32_t lvl = 0;
	lua_Debug dbg;
	while( lua->GetStack( lvl, &dbg ) == 1 && lua->GetInfo( "Slnu", &dbg ) == 1 )
	{
		lua->PushNumber( ++lvl );
		lua->CreateTable( );

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

		lua->PushNumber( dbg.i_ci );
		lua->SetField( -2, "i_ci" );

		lua->SetTable( -3 );
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

LUA_FUNCTION_STATIC( AdvancedLuaErrorReporter )
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

	// TODO: Fix extra stack entry, call the function directly?
	reporter_ref.Push( );
	LUA->Push( 1 );
	LUA->Call( 1, 1 );
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

	void LuaError( const CLuaError *error )
	{
		int32_t funcs = PushHookRun( lua, "LuaError" );
		if( funcs == 0 )
			return callback->LuaError( error );

		const std::string &errstr = runtime ? runtime_error : error->message;

		int32_t args = 3;
		lua->PushString( "LuaError" );
		lua->PushBool( runtime );
		lua->PushString( errstr.c_str( ) );

		std::istringstream errstream( errstr );
		ErrorProperties props;
		args += PushErrorProperties( lua, errstream, props );

		if( runtime )
		{
			args += 1;
			runtime_stack.Push( );
			runtime_stack.Free( );
		}
		else
			args += PushStackTable( lua );

		runtime = false;

		args += 1;
		const std::string source_addon = FindWorkshopAddonFileOwner( props.source_file );
		if( source_addon.empty( ) )
			lua->PushNil( );
		else
			lua->PushString( source_addon.c_str( ) );

		if( RunHook( lua, "LuaError", args, funcs ) )
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
	LUA->PushCFunction( AdvancedLuaErrorReporter );
	LUA->SetTable( GarrysMod::Lua::INDEX_REGISTRY );
}

inline void ResetRuntime( GarrysMod::Lua::ILuaBase *LUA )
{
	LUA->PushNumber( 1 );
	reporter_ref.Push( );
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
	reporter_ref.Setup( LUA );
	runtime_stack.Setup( LUA );

	callback.SetLua( static_cast<GarrysMod::Lua::ILuaInterface *>( LUA ) );

#if defined LUAERROR_SERVER

	SymbolFinder symfinder;

	CreateInterfaceFn factory = reinterpret_cast<CreateInterfaceFn>( symfinder.ResolveOnBinary(
		dedicated_binary.c_str( ), FileSystemFactory_sym, FileSystemFactory_symlen
	) );
	if( factory != nullptr )
		filesystem = static_cast<CFileSystem_Stdio *>( factory( FILESYSTEM_INTERFACE_VERSION, nullptr ) );

#endif

	if( filesystem == nullptr )
		filesystem = filesystem_loader.GetInterface<CFileSystem_Stdio>( FILESYSTEM_INTERFACE_VERSION );

	if( filesystem == nullptr )
		LUA->ThrowError( "unable to initialize IFileSystem" );

	LUA->PushNumber( 1 );
	LUA->GetTable( GarrysMod::Lua::INDEX_REGISTRY );
	if( LUA->IsType( -1, GarrysMod::Lua::Type::FUNCTION ) )
		reporter_ref.Create( );
	else
		LUA->ThrowError( "failed to locate AdvancedLuaErrorReporter" );

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
	reporter_ref.Free( );
	callback.Reset( );
}

}
