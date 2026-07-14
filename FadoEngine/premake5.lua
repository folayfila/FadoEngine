-- FadoEngine premake5.lua

workspace "FadoEngine"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "Engine"
    debugdir "%{wks.location}/Engine/"

-- ──────────────────────────────────────────────────────────────────────
-- Engine (Application .exe)
-- ──────────────────────────────────────────────────────────────────────
project "Engine"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    targetname "FadoEngine"

    targetdir "%{wks.location}%{cfg.platform}/%{cfg.buildcfg}/"
    objdir    "%{prj.name}/%{cfg.platform}/%{cfg.buildcfg}/"

    files
    {
        "Engine/Code/**.h",
        "Engine/Code/**.cpp",
        "Engine/Tools/FadoConverter/**.cpp",
        "Engine/Tools/FadoConverter/**.h",
        "Engine/Shaders/**.hlsl",
        "Engine/FadoEngine.rc",
        "Engine/FadoEngine.ico",
        "Engine/ThirdParty/**.h",
        "Engine/ThirdParty/**.cpp",
        "Engine/ThirdParty/**.c",
    }

    includedirs
    {
        "$(SolutionDir)/Engine/",
        "$(SolutionDir)/Engine/ThirdParty/imgui/",
        "$(SolutionDir)/Game/Code/",
    }

    postbuildcommands
    {
        "{COPYDIR} %{wks.location}/Engine/Assets %{cfg.targetdir}/Assets",
        "{COPYDIR} %{wks.location}/Engine/Shaders %{cfg.targetdir}/Shaders"
    }

    disablewarnings { "6297", "28251", "6387", "6386" }

    filter "files:Engine/Tools/FadoConverter/fado_converter.cpp"
        excludefrombuild "On"

    filter "files:Engine/Shaders/**.hlsl"
        buildaction "None"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        optimize "Off"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Speed"

    filter {}

-- ──────────────────────────────────────────────────────────────────────
-- Game (DLL)
-- ──────────────────────────────────────────────────────────────────────
project "Game"
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"

    defines { "GAME_DLL" }

    targetdir "%{wks.location}%{cfg.platform}/%{cfg.buildcfg}/"
    objdir    "%{prj.name}/%{cfg.platform}/%{cfg.buildcfg}/"

    files
    {
        "Game/Code/**.h",
        "Game/Code/**.cpp"
    }

    includedirs
    {
        "$(SolutionDir)/Engine/Code/",
        "$(SolutionDir)/Game/Code/"
    }

    disablewarnings { "6297", "28251", "6387", "6386" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        optimize "Off"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Speed"

    filter {}
