# Parsing And Strings

`IXXExtension.Parse` parses scalar values into `ixx::Result<T>`. Parsers trim
ASCII whitespace and require the whole trimmed input to be consumed.

```cpp
auto value = ixx::ParseInt<>(" 123 ");
auto hex = ixx::ParseHex<std::uint32_t>("0xFF");
auto enabled = ixx::ParseBool("yes");
```

Parsing failures use `ixx::Errc::ParseFailed` and include the input text in the
error message.

`IXXExtension.String` provides allocation-returning helpers such as `Trim`,
`Split`, `Join`, case conversion, and `ReplaceAll`. Trimming helpers are
ASCII-oriented. `ToUpper` and `ToLower` use the active C locale through
`std::toupper` and `std::tolower`.
