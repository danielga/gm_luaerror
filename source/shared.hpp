#pragma once

#include <cstdint>
#include <sstream>

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

int32_t PushHookRun( GarrysMod::Lua::ILuaInterface *lua, const char *hook, bool cleanup = true );
int32_t PushErrorProperties( GarrysMod::Lua::ILuaInterface *lua, std::istringstream &error );
bool RunHook( GarrysMod::Lua::ILuaInterface *lua, const char *hook, int32_t args, int32_t funcs );

void Initialize( lua_State *state );
void Deinitialize( lua_State *state );

}