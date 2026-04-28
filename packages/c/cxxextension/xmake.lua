package("cxxextension")
    add_urls("https://github.com/Newrite/CXXExtension.git")
    add_versions("1.0.0", "v1.0.0")

    on_install(function (package)
        import("package.tools.xmake").install(package)
    end)