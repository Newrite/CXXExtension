/// Umbrella module for the IXXExtension utility library.
///
/// Import this module when a translation unit needs the complete public API:
/// core result/error helpers, parsing, string utilities, collection primitives,
/// actor concurrency, and range/container helpers.
///
/// ## Example
///
/// ```cpp
/// import IXXExtension;
///
/// auto value = ixx::ParseInt<>("42");
/// ```
export module IXXExtension;

export import IXXExtension.Core;
export import IXXExtension.Parse;
export import IXXExtension.String;
export import IXXExtension.Collections;
export import IXXExtension.Concurrency;
export import IXXExtension.ContainerExtension;
