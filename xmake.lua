-- include subprojects
includes("lib/commonlibsf")

-- project
set_project("BareFrontier")
set_version("0.1.7")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- build modes
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

target("BareFrontier")
    add_rules("commonlibsf.plugin", {
        name = "BareFrontier",
        author = "VexenDeimos",
        description = "Native character creator outfit preview control for Starfield"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")