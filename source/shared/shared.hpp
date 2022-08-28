#pragma once

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

void Initialize( GarrysMod::Lua::ILuaBase *LUA );
void Deinitialize( GarrysMod::Lua::ILuaBase *LUA );

}
