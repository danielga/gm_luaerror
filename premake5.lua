newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	value = "path to garrysmod_common directory"
})

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
	"you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
include(path.join(gmcommon, "generator.v2.lua"))

CreateWorkspace({name = "luaerror", abi_compatible = true})
	CreateProject({serverside = true, manual_files = true})
		files({
			"source/main.cpp",
			"source/server.cpp",
			"source/server.hpp",
			"source/shared.cpp",
			"source/shared.hpp"
		})
		IncludeLuaShared()
		IncludeHelpersExtended()
		IncludeSDKCommon()
		IncludeSDKTier0()
		IncludeSDKTier1()
		IncludeScanning()
		IncludeDetouring()

	CreateProject({serverside = false, manual_files = true})
		files({
			"source/main.cpp",
			"source/shared.cpp",
			"source/shared.hpp"
		})
		IncludeLuaShared()
		IncludeHelpersExtended()
		IncludeSDKCommon()
		IncludeSDKTier0()
		IncludeSDKTier1()
		IncludeScanning()
