workspace "RIQAudio"
    architecture "x86_64"

    configurations
    {
        "Debug",
        "Release"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "RIQAudio"
    location "RIQAudio"
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"

    cdialect "Default"
    staticruntime "On"

    targetdir ("RIQAudioUnity/Assets/RIQAudioSharp/bin")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.hpp",
        "%{prj.name}/src/**.c",
        "%{prj.name}/src/**.cpp",
    }

    includedirs
    {
        "%{prj.name}/src",
        "%{prj.name}/vendor"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS"
    }

    filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "On"