set_xmakever("3.0.8")

local is_standalone = os.projectdir() == os.scriptdir()

if is_standalone then
    set_project("IXXExtension")
    set_version("1.0.0")
    set_defaultmode("releasedbg")
    add_rules("mode.debug", "mode.releasedbg")
end

target("IXXExtension")
    set_kind("static")
    set_default(is_standalone)

    set_languages("c++23")
    set_warnings("allextra")

    set_policy("build.c++.modules", true)
    set_policy("check.auto_ignore_flags", false)

    if is_plat("windows") then
        add_cxxflags("/utf-8", { tools = "cl" })
        add_cxflags("/utf-8", { tools = "cl" })
    end

    add_includedirs("src", { public = true })

    add_files("src/**.ixx",  { public = true, install = true })

if is_standalone then
    local examples = {
        "actor_autonomous",
        "actor_basic",
        "actor_reply",
        "alias_basic",
        "channel_unbounded",
        "container_extension",
        "inbox_stash",
        "jobs_thread_pool",
        "mailbox_basic",
        "oneshot_basic",
        "parse_and_string",
        "text_utf8",
    }

    for _, example in ipairs(examples) do
        target("example_" .. example)
            set_kind("binary")
            set_default(false)
            set_group("examples")

            set_languages("c++23")
            set_warnings("allextra")

            set_policy("build.c++.modules", true)
            set_policy("check.auto_ignore_flags", false)

            if is_plat("windows") then
                add_cxxflags("/utf-8", { tools = "cl" })
                add_cxflags("/utf-8", { tools = "cl" })
            end

            add_deps("IXXExtension")
            add_files("examples/" .. example .. ".cpp")
    end
end
