#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <lua.hpp>

struct lua_State;

namespace GarrysMod
{

namespace Lua
{

class ILuaInterface;

}

}

namespace shared
{
	struct LuaDebug
	{
		LuaDebug( ) :
			event( 0 ),
			currentline( -1 ),
			nups( 0 ),
			linedefined( -1 ),
			lastlinedefined( -1 ),
			i_ci( 0 )
		{ }

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
			source_line( -1 ),
			player( 0 )
		{ }

		void Clear( )
		{
			runtime = false;
			source_file.clear( );
			source_line = -1;
			error_string.clear( );
			stack_data.clear( );
			player = 0;
		}

		bool runtime;
		std::string source_file;
		int32_t source_line;
		std::string error_string;
		std::vector<LuaDebug> stack_data;
		int32_t player;
	};

	void Initialize( lua_State *state );
	void Deinitialize( lua_State *state );
}