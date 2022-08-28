#pragma once

namespace GarrysMod
{
	namespace Lua
	{
		class ILuaBase;
	}
}

namespace server
{

void Initialize( GarrysMod::Lua::ILuaBase *LUA );
void Deinitialize( GarrysMod::Lua::ILuaBase *LUA );

}
