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

`IXXExtension.String` provides allocation-returning helpers such as
`TrimAscii`, `TrimLeftAscii`, `TrimRightAscii`, `Split`, `Join`, ASCII case
conversion, and `ReplaceAll`.

String helpers accept common string-like inputs and preserve the source
character type for `char`, `wchar_t`, `char8_t`, `char16_t`, and `char32_t`
where the operation is ASCII-oriented.

```cpp
auto name = ixx::TrimAscii("  Alice\n");
auto upper = ixx::ToUpperAscii(name);
auto parts = ixx::Split("red::green::blue", "::");
auto text = ixx::ReplaceAll("one fish, two fish", "fish", "cat");
```

`ReplaceAll` returns a copy of the source when the search string is empty.
