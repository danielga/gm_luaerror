#pragma once

#include <cstdint>
#include <iosfwd>

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

int32_t PushErrorProperties( GarrysMod::Lua::ILuaInterface *lua, std::istringstream &error );

void Initialize( GarrysMod::Lua::ILuaBase *LUA );
void Deinitialize( GarrysMod::Lua::ILuaBase *LUA );

}
