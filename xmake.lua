-- Bare Frontier: native Starfield SFSE plugin
-- CommonLibSF is obtained with scripts/Setup-Dependencies.ps1.
includes("lib/commonlibsf")

set_project("BareFrontier")
set_version("0.1.5")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

target("BareFrontier")
    add_rules("commonlibsf.plugin", {
        name = "BareFrontier",
        author = "VexenDeimos",
        description = "Reversible character-creator preview outfit toggle for Starfield"
    })
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
