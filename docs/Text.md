# Text

`IXXExtension.Text` contains UTF-8 validation and byte-preserving bridges
between `std::string` and `std::u8string`.

The module treats `std::string` as UTF-8 by convention. It does not perform
Unicode normalization, case folding, grapheme segmentation, or locale-sensitive
transforms.

```cpp
std::string_view text = "ready";

if (auto valid = ixx::text::ValidateUtf8(text); !valid)
{
  auto message = valid.error().message;
}

auto u8 = ixx::text::ToU8(text);
auto bytes = ixx::text::FromU8(u8);
```

On Windows, the module also provides strict UTF-8/UTF-16 conversion for Win32
wide APIs.

```cpp
#ifdef _WIN32
auto wide = ixx::text::Utf8ToWide("C:/tmp/file.txt");
if (!wide) return 1;

auto utf8 = ixx::text::WideToUtf8(*wide);
#endif
```

Invalid UTF-8 or UTF-16 is returned through `ixx::Result` with
`ixx::text::TextErrc`.
