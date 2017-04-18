#pragma once

#include <cstdint>
#include <sstream>

namespace GarrysMod
{
	namespace Lua
	{
		class ILuaBase;
		class ILuaInterface;
	}
}

namespace shared
{

int32_t PushHookRun( GarrysMod::Lua::ILuaInterface *lua, const char *hook );
int32_t PushErrorProperties( GarrysMod::Lua::ILuaInterface *lua, std::istringstream &error );
bool RunHook( GarrysMod::Lua::ILuaInterface *lua, const char *hook, int32_t args, int32_t funcs );

void Initialize( GarrysMod::Lua::ILuaBase *LUA );
void Deinitialize( GarrysMod::Lua::ILuaBase *LUA );

}
