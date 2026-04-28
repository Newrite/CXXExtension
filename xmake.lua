set_xmakever("3.0.8")

set_project("CXXExtension")
set_version("1.0.0")
set_defaultmode("releasedbg")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

target("CXXExtension")
    set_kind("static")

    set_languages("c++23")
    set_warnings("allextra")

    set_policy("build.c++.modules", true)
    set_policy("check.auto_ignore_flags", false)

    add_includedirs("src", { public = true })

    add_files("src/**.ixx",  { public = true, install = true })