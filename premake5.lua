PROJECT_GENERATOR_VERSION = 2

newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	value = "path to garrysmod_common directory"
})

include(assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
	"you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory"))

CreateWorkspace({name = "luaerror", abi_compatible = true})
	CreateProject({serverside = true, manual_files = true})
		IncludeLuaShared()
		IncludeHelpersExtended()
		IncludeSDKCommon()
		IncludeSDKTier0()
		IncludeSDKTier1()
		IncludeScanning()
		IncludeDetouring()

		files({
			"source/shared/main.cpp",
			"source/server/server.cpp",
			"source/server/server.hpp",
			"source/shared/shared.cpp",
			"source/shared/shared.hpp",
			"source/common/common.cpp",
			"source/common/common.hpp"
		})

	CreateProject({serverside = false, manual_files = true})
		IncludeLuaShared()
		IncludeHelpersExtended()
		IncludeSDKCommon()
		IncludeSDKTier0()
		IncludeSDKTier1()
		IncludeScanning()
		IncludeDetouring()

		files({
			"source/shared/main.cpp",
			"source/shared/shared.cpp",
			"source/shared/shared.hpp",
			"source/common/common.cpp",
			"source/common/common.hpp"
		})

	project("testing")
		kind("ConsoleApp")
		includedirs("source/common")
		files({
			"source/common/common.hpp",
			"source/common/common.cpp",
			"source/testing/main.cpp"
		})
		vpaths({
			["Header files/*"] = "source/**.hpp",
			["Source files/*"] = "source/**.cpp"
		})
