# Core

`CXXExtension.Core` provides shared utility types.

## Results and errors

`cxx::Result<T>` is `std::expected<T, cxx::Error>`. `cxx::VoidResult` is the
same pattern for operations that return no value.

`cxx::Error` stores a `std::error_code`, a detail message, source location, and
an optional cause chain.

```cpp
auto error = cxx::Error::Make(cxx::Errc::InvalidArgument, "path is empty");
```

## Enum error codes

To make your enum usable as an error code, specialize `cxx::ErrorCodeTraits`.

```cpp
enum class LoadErrc
{
  Missing,
  Corrupt,
};

template <>
struct cxx::ErrorCodeTraits<LoadErrc>
{
  static constexpr const char* Name = "load";

  static constexpr auto Message(LoadErrc code) noexcept -> std::string_view
  {
    switch (code)
    {
      case LoadErrc::Missing: return "Missing file";
      case LoadErrc::Corrupt: return "Corrupt file";
      default: return "Unknown load error";
    }
  }
};
```

## Strong aliases

`cxx::StrongAlias<T, Tag>` gives distinct type identity to values with the same
underlying representation.

```cpp
struct UserIdTag;
using UserId = cxx::StrongAlias<std::uint64_t, UserIdTag>;

UserId id{42};
```
