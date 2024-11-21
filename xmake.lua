-- set xmake version
set_xmakever("2.9.4")

-- include local folders
includes("extern/commonlibsse")

-- set project
set_project("Paraglider")
set_version("1.12.0")
set_license("MIT")

-- set defaults
set_arch("x64")
set_languages("c++23")
set_optimize("faster")
set_warnings("allextra", "error")
set_defaultmode("releasedbg")

-- enable lto
set_policy("build.optimization.lto", true)

-- add rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- add config
set_config("rex_ini", true)
set_config("sfse_xbyak", true)
set_config("skyrim_ae", true)

-- setup targets
target("Paraglider")
    -- bind local dependencies
    add_deps("commonlibsse")

    -- add commonlibsse plugin
    add_rules("commonlibsse.plugin", {
        name = "Paraglider",
        author = "Loki"
    })

    -- add source files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/PCH.h")

    -- add extra files
    add_extrafiles(".clang-format")
