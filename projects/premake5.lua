newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	value = "path to garrysmod_common directory"
})

local gmcommon = _OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON")
if gmcommon == nil then
	error("you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
end

include(gmcommon)

CreateWorkspace({name = "luaerror", abi_compatible = true})
	CreateProject({serverside = true, manual_files = true})
		files({
			"../source/main.cpp",
			"../source/server.cpp",
			"../source/server.hpp",
			"../source/shared.cpp",
			"../source/shared.hpp"
		})
		IncludeSDKCommon()
		IncludeSDKTier0()
		IncludeSDKTier1()
		IncludeScanning()
		IncludeDetouring()

	CreateProject({serverside = false, manual_files = true})
		files({
			"../source/main.cpp",
			"../source/shared.cpp",
			"../source/shared.hpp"
		})
