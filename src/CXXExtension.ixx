/// Umbrella module for the CXXExtension utility library.
///
/// Import this module when a translation unit needs the complete public API:
/// core result/error helpers, parsing, string utilities, actor containers, and
/// range/container helpers.
///
/// ## Example
///
/// ```cpp
/// import CXXExtension;
///
/// auto value = cxx::ParseInt<>("42");
/// ```
export module CXXExtension;

export import CXXExtension.Core;
export import CXXExtension.Parse;
export import CXXExtension.String;
export import CXXExtension.Container;
export import CXXExtension.ContainerExtension;
