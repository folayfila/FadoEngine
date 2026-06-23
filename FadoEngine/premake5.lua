-- FadoEngine premake5.lua
-- Place this file next to your .sln, in FadoEngine/

workspace "FadoEngine"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "FadoEngine"

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
        "FadoEngine/Tools/FadoConverter/**.h"
    }

    removefiles
    {
        "FadoEngine/Tools/FadoConverter/fado_converter.cpp",
        "FadoEngine/Shaders/**",
    }

    includedirs
    {
        "%{wks.location}/FadoEngine/",
        "%{wks.location}/FadoEngine/Code/imgui/",
        "%{wks.location}/Game/Code/",
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
