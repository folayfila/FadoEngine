-- FadoEngine premake5.lua
-- Place this file next to your .sln, in FadoEngine/

workspace "FadoEngine"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "FadoEngine"
    debugdir "%{wks.location}/FadoEngine/"

-- ──────────────────────────────────────────────────────────────────────
-- Engine (Application .exe)
-- ──────────────────────────────────────────────────────────────────────
project "FadoEngine"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"

    targetdir "%{wks.location}%{cfg.platform}/%{cfg.buildcfg}/"
    objdir    "%{prj.name}/%{cfg.platform}/%{cfg.buildcfg}/"

    files
    {
        "FadoEngine/Code/**.h",
        "FadoEngine/Code/**.cpp",
        "FadoEngine/Tools/FadoConverter/**.h",
        "FadoEngine/Tools/FadoConverter/**.cpp",
        "FadoEngine/Tools/FadoConverter/**.c",
        "FadoEngine/Shaders/**.hlsl",
        "FadoEngine/FadoEngine.rc",
        "FadoEngine/FadoEngine.ico"
    }

    includedirs
    {
        "%{wks.location}/FadoEngine/",
        "%{wks.location}/FadoEngine/Code/imgui/",
        "%{wks.location}/Game/Code/",
    }

    postbuildcommands
    {
        "{COPYDIR} %{wks.location}/FadoEngine/Assets %{cfg.targetdir}/Assets",
        "{COPYDIR} %{wks.location}/FadoEngine/Shaders %{cfg.targetdir}/Shaders"
    }

    disablewarnings { "6297", "28251", "6387", "6386" }

    filter "files:FadoEngine/Tools/FadoConverter/fado_converter.cpp"
        excludefrombuild "On"

    filter "files:FadoEngine/Shaders/**.hlsl"
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

    targetdir "%{wks.location}%{cfg.platform}/%{cfg.buildcfg}/"
    objdir    "%{prj.name}/%{cfg.platform}/%{cfg.buildcfg}/"

    files
    {
        "Game/Code/**.h",
        "Game/Code/**.cpp"
    }

    includedirs
    {
        "%{wks.location}/FadoEngine/Code/"
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
