set_xmakever("3.0.8")

local is_standalone = os.projectdir() == os.scriptdir()

if is_standalone then
    set_project("CXXExtension")
    set_version("1.0.0")
    set_defaultmode("releasedbg")
    add_rules("mode.debug", "mode.releasedbg")
end

target("CXXExtension")
    set_kind("static")
    set_default(is_standalone)

    set_languages("c++23")
    set_warnings("allextra")

    set_policy("build.c++.modules", true)
    set_policy("check.auto_ignore_flags", false)

    add_includedirs("src", { public = true })

    add_files("src/**.ixx",  { public = true, install = true })