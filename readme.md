# gm\_luaerror

A module for Garry's Mod that adds hooks for obtaining errors that happen on the client and server (if activated on server, it also pushes errors from clients).

## API reference

    luaerror.Version -- holds the luaerror module version in a string form
    luaerror.VersionNum -- holds the luaerror module version in a numeric form, LuaJIT style

    luaerror.EnableRuntimeDetour(boolean) -- enable/disable Lua runtime errors
    luaerror.EnableCompiletimeDetour(boolean) -- enable/disable Lua compiletime errors

    luaerror.EnableClientDetour(boolean) -- enable/disable Lua errors from clients (serverside only)
    -- returns nil followed by an error string in case of failure to detour

    Hooks:
    LuaError(isruntime, fullerror, sourcefile, sourceline, errorstr, stack)
    -- isruntime is a boolean saying whether this is a runtime error or not
    -- fullerror is a string which is the full error
    -- sourcefile is a string which is the source file of the error
    -- sourceline is a number which is the source line of the error
    -- errorstr is a string which is the error itself
    -- stack is a table containing the Lua stack at the time of the error

    ClientLuaError(player, fullerror, sourcefile, sourceline, errorstr, stack)
    -- player is a Player object which indicates who errored
    -- fullerror is a string which is the full error (trimmed and cleaned up)
    -- sourcefile is a string which is the source file of the error (may be nil)
    -- sourceline is a number which is the source line of the error (may be nil)
    -- errorstr is a string which is the error itself (may be nil)
    -- stack is a table containing the Lua stack at the time of the error
    -- sourcefile, sourceline and errorstr may be nil because of ErrorNoHalt and friends

## Compiling

The only supported compilation platforms for this project on Windows are **Visual Studio 2015**, **Visual Studio 2017** and **Visual Studio 2019** on **release** mode.  
On Linux, everything should work fine as is, on **release** mode.  
For Mac OSX, any **Xcode version (using the GCC compiler)** *MIGHT* work as long as the **Mac OSX 10.7 SDK** is used, on **release** mode.  
These restrictions are not random; they exist because of ABI compatibility reasons.  
If stuff starts erroring or fails to work, be sure to check the correct line endings (\n and such) are present in the files for each OS.  

## Requirements

This project requires [garrysmod_common][1], a framework to facilitate the creation of compilations files (Visual Studio, make, XCode, etc). Simply set the environment variable '**GARRYSMOD\_COMMON**' or the premake option '**gmcommon**' to the path of your local copy of [garrysmod_common][1].  
We also use [SourceSDK2013][2], so set the environment variable '**SOURCE\_SDK**' or the premake option '**sourcesdk**' to the path of your local copy of [SourceSDK2013][2]. The previous links to [SourceSDK2013][2] point to my own fork of VALVe's repo and for good reason: Garry's Mod has lots of backwards incompatible changes to interfaces and it's much smaller, being perfect for automated build systems like Travis-CI (which is used for this project).  

  [1]: https://github.com/danielga/garrysmod_common
  [2]: https://github.com/danielga/sourcesdk-minimal
