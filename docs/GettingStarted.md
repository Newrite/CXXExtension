# Getting Started

The library is exported as C++ modules. A simple translation unit can import the
umbrella module:

```cpp
import IXXExtension;
import std;

auto main() -> int
{
  auto value = ixx::ParseInt<>("42");
  return value && *value == 42 ? 0 : 1;
}
```

Most fallible helpers return `ixx::Result<T>`, an alias for
`std::expected<T, ixx::Error>`.

```cpp
auto port = ixx::ParseUInt<std::uint16_t>("8080");
if (!port)
{
  auto message = port.error().message;
}
```

The `examples/` directory contains compile-oriented examples for the major
modules. They are not currently wired into the provided build files.
