-- set minimum xmake version
set_xmakever("3.0.8")

local is_standalone = os.projectdir() == os.scriptdir()

if is_standalone then
    set_project("CXXExtension")
    set_version("1.0.0")
    set_defaultmode("releasedbg")

    add_rules("mode.debug", "mode.releasedbg")
    add_rules("plugin.vsxmake.autoupdate")
end

-- set policies


local function add_module_interface_files(dir)
    local ixx_files = os.files(path.join(dir, "**.ixx"))
    if #ixx_files > 0 then
        add_files(ixx_files, {public = true})
    end

    local cppm_files = os.files(path.join(dir, "**.cppm"))
    if #cppm_files > 0 then
        add_files(cppm_files, {public = true})
    end
end

local function add_cpp_files(dir)
    local cpp_files = os.files(path.join(dir, "**.cpp"))
    if #cpp_files > 0 then
        add_files(cpp_files)
    end

    local cc_files = os.files(path.join(dir, "**.cc"))
    if #cc_files > 0 then
        add_files(cc_files)
    end

    local cxx_files = os.files(path.join(dir, "**.cxx"))
    if #cxx_files > 0 then
        add_files(cxx_files)
    end
end

local function add_visible_headers(dir)
    add_headerfiles(path.join(dir, "**.h"))
    add_headerfiles(path.join(dir, "**.hpp"))
    add_headerfiles(path.join(dir, "**.hh"))
    add_headerfiles(path.join(dir, "**.hxx"))
end

local function add_all_sources(dir)
    add_includedirs(dir)
    add_visible_headers(dir)
    add_module_interface_files(dir)
    add_cpp_files(dir)
end

-- targets
target("CXXExtension")
    set_kind("static")
    set_default(is_standalone)

    set_arch("x64")
    set_languages("c++23")
    set_warnings("allextra")

    set_policy("build.c++.modules", true)
    set_policy("check.auto_ignore_flags", false)

    -- add src files
    add_all_sources("src")
