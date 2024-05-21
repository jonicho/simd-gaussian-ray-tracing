ARCH = os.getenv("ARCH")
if ARCH == nil then
    ARCH = "native"
end

workspace "ba-thesis"
    toolset "clang"
    cppdialect "c++20"
    configurations { "debug", "release" }
    location "build"

    filter "configurations:debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:release"
        defines { "NDEBUG" }
        optimize "On"

    project "imgui"
        kind "StaticLib"
        language "C++"
        targetdir "build/lib/%{cfg.buildcfg}"

        includedirs { "./src/external/imgui", "./src/external/imgui/backend" }
        files { "./src/external/imgui/**.cpp" }

    project "jevents"
        kind "StaticLib"
        language "C"
        targetdir "build/lib/%{cfg.buildcfg}"

        files { "./src/jevents/*.c" }

    project "vk-renderer"
        kind "StaticLib"
        language "C++"
        targetdir "build/lib/%{cfg.buildcfg}"
        buildoptions { "-Wall", "-Wextra", "-Werror" }
        defines { "VULKAN_HPP_NO_EXCEPTIONS" }

        filter "files:src/vk-renderer/vk-mem-alloc/vk_mem_alloc.cpp"
            buildoptions { "-w" }
        filter {}
        includedirs { "./src", "./src/external/imgui/" }
        files { "./src/vk-renderer/**.cpp" }

    project "volumetric-ray-tracer"
        kind "ConsoleApp"
        language "C++"
        targetdir "build/bin/%{cfg.buildcfg}"
        buildoptions { "-Wall", "-Wextra", "-Werror", "-march="..ARCH, "-save-temps=obj", "-masm=intel", "-fverbose-asm", "-ffast-math" }
        defines { "INCLUDE_IMGUI" }

        includedirs { "./src", "./src/external/imgui/" }
        files { "./src/volumetric-ray-tracer/*.cpp" }
        links { "fmt", "glfw", "vulkan", "vk-renderer", "imgui", "rt" }

    project "cycles-test"
        kind "ConsoleApp"
        language "C++"
        targetdir "build/bin/%{cfg.buildcfg}"
        buildoptions { "-Wall", "-Wextra", "-march="..ARCH, "-save-temps=obj", "-masm=intel", "-fverbose-asm", "-ffast-math" }

        includedirs { "./src", "./src/jevents" }
        files { "./src/volumetric-ray-tracer/tests/approx_cycles.cpp", "./src/volumetric-ray-tracer/approx.cpp" }
        links { "fmt", "jevents" }

    project "accuracy-test"
        kind "ConsoleApp"
        language "C++"
        targetdir "build/bin/%{cfg.buildcfg}"
        buildoptions { "-Wall", "-Wextra", "-march="..ARCH, "-save-temps=obj", "-masm=intel", "-fverbose-asm", "-ffast-math" }

        includedirs { "./src" }
        files { "./src/volumetric-ray-tracer/tests/accuracy.cpp", "./src/volumetric-ray-tracer/approx.cpp" }
        links { "fmt" }

    project "transmittance-test"
        kind "ConsoleApp"
        language "C++"
        targetdir "build/bin/%{cfg.buildcfg}"
        buildoptions { "-Wall", "-Wextra", "-march="..ARCH, "-save-temps=obj", "-masm=intel", "-fverbose-asm", "-ffast-math" }

        includedirs { "./src" }
        files { "./src/volumetric-ray-tracer/tests/transmittance.cpp", "./src/volumetric-ray-tracer/rt.cpp" }
        links { "fmt" }

    project "timing-test"
        kind "ConsoleApp"
        language "C++"
        targetdir "build/bin/%{cfg.buildcfg}"
        buildoptions { "-Wall", "-Wextra", "-march="..ARCH, "-save-temps=obj", "-masm=intel", "-fverbose-asm", "-ffast-math" }

        includedirs { "./src" }
        files { "./src/volumetric-ray-tracer/tests/timing.cpp", "./src/volumetric-ray-tracer/rt.cpp", "./src/volumetric-ray-tracer/approx.cpp" }
        links { "fmt" }
