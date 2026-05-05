# Core

`IXXExtension.Core` provides shared utility types.

## Results and errors

`ixx::Result<T>` is `std::expected<T, ixx::Error>`. `ixx::VoidResult` is the
same pattern for operations that return no value.

`ixx::Error` stores a `std::error_code`, a detail message, source location, and
an optional cause chain.

```cpp
auto error = ixx::Error::Make(ixx::Errc::InvalidArgument, "path is empty");
```

## Enum error codes

To make your enum usable as an error code, specialize `ixx::ErrorCodeTraits`.

```cpp
enum class LoadErrc
{
  Missing,
  Corrupt,
};

template <>
struct ixx::ErrorCodeTraits<LoadErrc>
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

`ixx::StrongAlias<T, Tag>` gives distinct type identity to values with the same
underlying representation.

```cpp
struct UserIdTag;
using UserId = ixx::StrongAlias<std::uint64_t, UserIdTag>;

UserId id{42};
```
