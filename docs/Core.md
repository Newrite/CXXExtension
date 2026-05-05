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

## Aliases

`ixx::Alias<T, Tag, Skills...>` gives distinct type identity to values with the
same underlying representation. Construction is explicit, and `Value()` returns
the wrapped object with the same lvalue/rvalue category as the alias.

Optional skills add small operator surfaces. `ixx::alias::DereferenceUnwrap`
adds `operator*` as shorthand for `Value()`, and
`ixx::alias::UnaryArithmetic` adds unary `+` and `-` when the underlying value
supports them.

```cpp
struct UserIdTag;
using UserId = ixx::Alias<std::uint64_t, UserIdTag, ixx::alias::DereferenceUnwrap>;

UserId id{42};
auto raw = *id;
```

Use `ixx::alias::Into<Alias>(value)` and `ixx::alias::Unwrap(alias)` in generic
code when constructing or extracting an alias through a named function object is
clearer than direct construction.

```cpp
auto other = ixx::alias::Into<UserId>(7);
auto value = ixx::alias::Unwrap(other);
```
