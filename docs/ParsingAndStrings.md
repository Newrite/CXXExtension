# Parsing And Strings

`CXXExtension.Parse` parses scalar values into `cxx::Result<T>`. Parsers trim
ASCII whitespace and require the whole trimmed input to be consumed.

```cpp
auto value = cxx::ParseInt<>(" 123 ");
auto hex = cxx::ParseHex<std::uint32_t>("0xFF");
auto enabled = cxx::ParseBool("yes");
```

Parsing failures use `cxx::Errc::ParseFailed` and include the input text in the
error message.

`CXXExtension.String` provides allocation-returning helpers such as `Trim`,
`Split`, `Join`, case conversion, and `ReplaceAll`. Trimming helpers are
ASCII-oriented. `ToUpper` and `ToLower` use the active C locale through
`std::toupper` and `std::tolower`.
