/// Core error, result, scope, enum, endian, and strong-alias utilities.
///
/// This module provides the common vocabulary used by the rest of
/// IXXExtension: `Result<T>`, structured `Error` values, opt-in enum bitmask
/// operators, byte-order helpers, and type-safe strong aliases.
export module IXXExtension.Core;

import std;

namespace ixx
{

  /// Customization point for converting enum values to `std::error_code`.
  ///
  /// Specialize this template for an enum and provide:
  ///
  /// ```cpp
  /// static constexpr const char* Name = "category";
  /// static constexpr auto Message(MyErrc code) noexcept -> std::string_view;
  /// ```
  ///
  /// @tparam Enum Enum type being adapted.
  export template <class Enum>
  struct ErrorCodeTraits;

  /// Concept satisfied by enums that have `ErrorCodeTraits`.
  ///
  /// `ErrorCodeEnum` enables `MakeErrorCode`, `Error::Make`, and
  /// `Error::Wrap` overloads that accept enum values directly.
  ///
  /// @tparam Enum Enum type to test.
  export template <class Enum>
  concept ErrorCodeEnum = std::is_enum_v<Enum> && requires(Enum code) {
    { ErrorCodeTraits<Enum>::Name } -> std::convertible_to<const char*>;
    { ErrorCodeTraits<Enum>::Message(code) } -> std::convertible_to<std::string_view>;
  };

  /// `std::error_category` implementation backed by `ErrorCodeTraits`.
  ///
  /// Instances are normally obtained through `ErrorCategory<Enum>()`.
  ///
  /// @tparam Enum Enum type with `ErrorCodeTraits`.
  export template <class Enum>
  requires ErrorCodeEnum<Enum>
  class EnumErrorCategory final : public std::error_category
  {
public:

    /// Returns the category name declared by `ErrorCodeTraits<Enum>`.
    [[nodiscard]] auto name() const noexcept -> const char* override
    {
      return ErrorCodeTraits<Enum>::Name;
    }

    /// Returns the message for an enum value stored as an integer.
    [[nodiscard]] auto message(const int value) const -> std::string override
    {
      const auto code = static_cast<Enum>(value);
      return std::string{ErrorCodeTraits<Enum>::Message(code)};
    }
  };

  /// Returns the singleton error category for an adapted enum.
  ///
  /// The category has static storage duration and is safe to reference for the
  /// lifetime of the program.
  ///
  /// @tparam Enum Enum type with `ErrorCodeTraits`.
  /// @return Shared `std::error_category` instance.
  export template <class Enum>
  requires ErrorCodeEnum<Enum>
  [[nodiscard]] auto ErrorCategory() noexcept -> const std::error_category&
  {
    static const EnumErrorCategory<Enum> category{};
    return category;
  }

  /// Converts an adapted enum value to `std::error_code`.
  ///
  /// @tparam Enum Enum type with `ErrorCodeTraits`.
  /// @param code Enum value to convert.
  /// @return Error code using the enum's category.
  export template <class Enum>
  requires ErrorCodeEnum<Enum>
  [[nodiscard]] auto MakeErrorCode(const Enum code) noexcept -> std::error_code
  {
    return {
        static_cast<int>(code),
        ErrorCategory<Enum>(),
    };
  }

  /// Finds a message for an enum error code in a fixed lookup table.
  ///
  /// This helper is intended for `ErrorCodeTraits<Enum>::Message`
  /// implementations.
  ///
  /// @tparam Enum Enum type.
  /// @tparam Size Number of lookup entries.
  /// @param code Code to find.
  /// @param messages Pairs of code and message.
  /// @param fallback Message returned when no entry matches.
  /// @return Matching message or `fallback`.
  export template <class Enum, std::size_t Size>
  requires std::is_enum_v<Enum>
  [[nodiscard]] constexpr auto FindErrorMessage(
    const Enum                                                 code,
    const std::array<std::pair<Enum, std::string_view>, Size>& messages,
    const std::string_view                                     fallback = "Unknown error") noexcept -> std::string_view
  {
    for (const auto& [candidate, message] : messages)
    {
      if (candidate == code)
      {
        return message;
      }
    }

    return fallback;
  }

  /// Structured error value used by `Result<T>`.
  ///
  /// `Error` combines a machine-readable `std::error_code`, a user-facing
  /// message, source location, optional operation override, and optional nested
  /// cause.
  ///
  /// ## Ownership
  ///
  /// `Error` owns its strings. Wrapped causes are owned through
  /// `std::shared_ptr<const Error>` so copied errors can share the same cause
  /// chain cheaply.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto err = ixx::Error::Make(ixx::Errc::InvalidArgument, "path is empty");
  /// if (err.Is(ixx::Errc::InvalidArgument)) {
  ///   // handle invalid argument
  /// }
  /// ```
  export struct Error final
  {
    /// Machine-readable error code.
    std::error_code code{};
    /// Human-readable detail message.
    std::string message{};
    /// Optional operation name used instead of the captured function name.
    std::string operationOverride{};
    /// Source location where this error was created.
    std::source_location where = std::source_location::current();
    /// Optional owned cause for chained errors.
    std::shared_ptr<const Error> cause{};

    /// Creates an error from a `std::error_code`.
    ///
    /// @param code Machine-readable error code.
    /// @param message Human-readable detail message.
    /// @param operationOverride Optional operation name.
    /// @param where Source location captured by default at the call site.
    /// @return New error value.
    [[nodiscard]] static auto Make(
      std::error_code            code,
      std::string                message           = {},
      std::string                operationOverride = {},
      const std::source_location where             = std::source_location::current()) -> Error
    {
      return Error{
          .code              = std::move(code),
          .message           = std::move(message),
          .operationOverride = std::move(operationOverride),
          .where             = where,
          .cause             = nullptr,
      };
    }

    /// Creates an error from an adapted enum code.
    template <class Enum>
    requires ErrorCodeEnum<Enum>
    [[nodiscard]] static auto Make(
      const Enum                 code,
      std::string                message           = {},
      std::string                operationOverride = {},
      const std::source_location where             = std::source_location::current()) -> Error
    {
      return Make(ixx::MakeErrorCode(code), std::move(message), std::move(operationOverride), where);
    }

    /// Creates an error that owns another error as its cause.
    ///
    /// @param code Outer error code.
    /// @param inner Inner error moved into the cause chain.
    /// @param message Human-readable outer message.
    /// @param operationOverride Optional outer operation name.
    /// @param where Source location captured by default at the call site.
    /// @return New wrapped error.
    [[nodiscard]] static auto Wrap(
      std::error_code            code,
      Error                      inner,
      std::string                message           = {},
      std::string                operationOverride = {},
      const std::source_location where             = std::source_location::current()) -> Error
    {
      return Error{
          .code              = std::move(code),
          .message           = std::move(message),
          .operationOverride = std::move(operationOverride),
          .where             = where,
          .cause             = std::make_shared<Error>(std::move(inner)),
      };
    }

    /// Creates an adapted-enum error that owns another error as its cause.
    template <class Enum>
    requires ErrorCodeEnum<Enum>
    [[nodiscard]] static auto Wrap(
      const Enum                 code,
      Error                      inner,
      std::string                message           = {},
      std::string                operationOverride = {},
      const std::source_location where             = std::source_location::current()) -> Error
    {
      return Wrap(ixx::MakeErrorCode(code), std::move(inner), std::move(message), std::move(operationOverride), where);
    }

    /// Tests whether this error matches an adapted enum value.
    template <class Enum>
    requires ErrorCodeEnum<Enum>
    [[nodiscard]] auto Is(const Enum expected) const noexcept -> bool
    {
      return code == ixx::MakeErrorCode(expected);
    }

    /// Returns the underlying error category name.
    [[nodiscard]] auto CategoryName() const noexcept -> std::string_view
    {
      return code.category().name();
    }

    /// Returns the message supplied by the underlying `std::error_code`.
    [[nodiscard]] auto CodeMessage() const -> std::string
    {
      return code.message();
    }

    /// Returns the operation name for this error.
    ///
    /// `operationOverride` is returned when present; otherwise the captured
    /// source-location function name is returned.
    [[nodiscard]] auto Operation() const noexcept -> std::string_view
    {
      if (!operationOverride.empty())
      {
        return operationOverride;
      }

      return where.function_name();
    }

    /// Returns whether this error wraps another error.
    [[nodiscard]] auto HasCause() const noexcept -> bool
    {
      return static_cast<bool>(cause);
    }

    /// Returns the wrapped cause, or `nullptr` when there is no cause.
    ///
    /// The returned pointer is borrowed and remains valid as long as this error
    /// or another copy sharing the same cause remains alive.
    [[nodiscard]] auto Cause() const noexcept -> const Error*
    {
      return cause ? cause.get() : nullptr;
    }

    /// Creates `std::unexpected<Error>` from an adapted enum code.
    ///
    /// This is a convenience for returning `Result<T>` failures.
    template <class Enum>
    requires ErrorCodeEnum<Enum>
    [[nodiscard]] static auto MakeUnexpected(
      const Enum                 code,
      std::string                message           = {},
      std::string                operationOverride = {},
      const std::source_location where             = std::source_location::current()) -> std::unexpected<Error>
    {
      return std::unexpected{
          Make(code, std::move(message), std::move(operationOverride), where),
      };
    }

    /// Creates `std::unexpected<Error>` that wraps an inner error.
    template <class Enum>
    requires ErrorCodeEnum<Enum>
    [[nodiscard]] static auto WrapUnexpected(
      const Enum                 code,
      Error                      inner,
      std::string                message           = {},
      std::string                operationOverride = {},
      const std::source_location where             = std::source_location::current()) -> std::unexpected<Error>
    {
      return std::unexpected{
          Wrap(code, std::move(inner), std::move(message), std::move(operationOverride), where),
      };
    }
  };

  /// Result type used by IXXExtension operations that may fail.
  ///
  /// `Result<T>` is an alias for `std::expected<T, ixx::Error>`.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// ixx::Result<int> value = ixx::ParseInt<>("42");
  /// if (!value) {
  ///   auto message = value.error().message;
  /// }
  /// ```
  export template <class T>
  using Result = std::expected<T, Error>;

  /// Result type for operations that return only success or failure.
  export using VoidResult = std::expected<void, Error>;

  /// Built-in IXXExtension error codes.
  export enum class Errc : std::uint16_t
  {
    /// No error.
    None = 0,

    /// An argument was invalid.
    InvalidArgument = 1,
    /// Input shape or syntax was invalid.
    InvalidFormat,
    /// Parsing failed.
    ParseFailed,
    /// A value was outside the supported range.
    OutOfRange,
    /// Input was empty where a value was required.
    EmptyInput,
    /// A requested value was not found.
    NotFound,
    /// The requested operation is unsupported.
    Unsupported,
  };

  /// Error-code traits for the built-in `Errc` enum.
  export template <>
  struct ErrorCodeTraits<Errc>
  {
    /// Error category name used by `std::error_code`.
    static constexpr const char* Name = "cxx";

    /// Returns the message associated with a built-in error code.
    [[nodiscard]] static constexpr auto Message(const Errc code) noexcept -> std::string_view
    {
      using enum Errc;

      switch (code)
      {
        case None:
          return "No error";
        case InvalidArgument:
          return "Invalid argument";
        case InvalidFormat:
          return "Invalid format";
        case ParseFailed:
          return "Parse failed";
        case OutOfRange:
          return "Out of range";
        case EmptyInput:
          return "Empty input";
        case NotFound:
          return "Not found";
        case Unsupported:
          return "Unsupported operation";
        default:
          return "Unknown CXXExtension error";
      }
    }
  };

  /// Calls a function when the current scope exits.
  ///
  /// `ScopeExit` is move-only. Moving transfers ownership of the pending action;
  /// `Release` disables it.
  ///
  /// ## Error handling
  ///
  /// The destructor is `noexcept`. The stored callable must not throw when the
  /// guard is active.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto guard = ixx::ScopeExit{[] { Cleanup(); }};
  /// guard.Release(); // Cleanup will not run.
  /// ```
  ///
  /// @tparam F Callable type.
  export template <class F>
  class ScopeExit
  {
public:

    /// Stores a callable to run on destruction.
    template <class Fn>
    requires std::constructible_from<F, Fn>
    explicit ScopeExit(Fn&& fn) noexcept(std::is_nothrow_constructible_v<F, Fn>) : fn_(std::forward<Fn>(fn))
    {}

    /// Transfers the active cleanup action from another guard.
    ScopeExit(ScopeExit&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : fn_(std::move(other.fn_)),
          active_(std::exchange(other.active_, false))
    {}

    ScopeExit(const ScopeExit&) = delete;

    auto operator=(const ScopeExit&) -> ScopeExit& = delete;

    auto operator=(ScopeExit&&) -> ScopeExit& = delete;

    /// Runs the stored callable if the guard is still active.
    ~ScopeExit() noexcept
    {
      if (active_) fn_();
    }

    /// Disables the cleanup action.
    auto Release() noexcept -> void
    {
      active_ = false;
    }

private:

    F    fn_;
    bool active_ = true;
  };

  template <class F>
  ScopeExit(F) -> ScopeExit<F>;

  /// Combines multiple callable overload sets for use with `std::visit`.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// std::visit(ixx::Overloaded{
  ///   [](int value) {},
  ///   [](std::string_view value) {},
  /// }, variant);
  /// ```
  export template <class... Ts>
  struct Overloaded : Ts...
  {
    using Ts::operator()...;
  };

  template <class... Ts>
  Overloaded(Ts...) -> Overloaded<Ts...>;

  /// Enables bitmask operators for a specific enum type.
  ///
  /// Specialize this variable template to `true` for enum classes whose values
  /// are intended to be combined with `|` and tested with `&` or `HasFlag`.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// enum class Flags : std::uint32_t {
  ///   None = 0,
  ///   Read = 1u << 0,
  ///   Write = 1u << 1,
  /// };
  ///
  /// template <>
  /// inline constexpr bool ixx::EnableBitmaskOperators<Flags> = true;
  /// ```
  export template <class E>
  inline constexpr bool EnableBitmaskOperators = false;

  /// Concept satisfied by enum classes that opted into bitmask operators.
  export template <class E>
  concept BitmaskEnum = std::is_enum_v<E> && EnableBitmaskOperators<E>;

  /// Returns the bitwise OR of two opted-in enum values.
  export template <BitmaskEnum E>
  constexpr auto operator|(E lhs, E rhs) noexcept -> E
  {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
  }

  /// Returns the bitwise AND of two opted-in enum values.
  export template <BitmaskEnum E>
  constexpr auto operator&(E lhs, E rhs) noexcept -> E
  {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
  }

  /// Tests whether all bits from `flag` are present in `value`.
  export template <BitmaskEnum E>
  constexpr auto HasFlag(E value, E flag) noexcept -> bool
  {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
  }

  /// Converts an integral value from native endian to big endian.
  ///
  /// On big-endian platforms this returns `value` unchanged. On little-endian
  /// platforms this byte-swaps the value.
  export template <std::integral T>
  constexpr auto ToBigEndian(T value) noexcept -> T
  {
    if constexpr (std::endian::native == std::endian::big)
    {
      return value;
    }
    else
    {
      return std::byteswap(value);
    }
  }

  /// Converts an integral value from big endian to native endian.
  export template <std::integral T>
  constexpr auto FromBigEndian(T value) noexcept -> T
  {
    return ToBigEndian(value);
  }

  /// Concept satisfied by types accepted by `std::hash<T>`.
  export template <class T>
  concept Hashable = requires(const T& value) {
    { std::hash<T>{}(value) } -> std::convertible_to<std::size_t>;
  };

  /// Type-safe wrapper around an underlying value.
  ///
  /// `StrongAlias<T, Tag>` prevents accidental mixing of logically distinct
  /// values that share the same representation. Construction is explicit and
  /// extraction happens through `Value()`.
  ///
  /// ## Ownership
  ///
  /// The alias owns one `T`. Reference-qualified `Value()` overloads preserve
  /// lvalue/rvalue access to the stored value.
  ///
  /// ## Example
  ///
  /// ```cpp
  ///
  /// using UserId = ixx::StrongAlias<std::uint64_t, struct UserIdTag>;
  ///
  /// UserId id{42};
  /// auto raw = id.Value();
  /// ```
  ///
  /// @tparam T Stored value type.
  /// @tparam Tag Empty tag type that distinguishes this alias from others.
  export template <class T, class Tag>
  class StrongAlias
  {
public:

    /// Underlying stored type.
    using underlying_type = T;
    /// Tag type that gives this alias its distinct identity.
    using tag_type = Tag;

    /// Default-constructs the underlying value when `T` supports it.
    constexpr StrongAlias()
    requires std::default_initializable<T>
    = default;

    /// Explicitly constructs the alias from a value accepted by `T`.
    template <class U>
    requires(!std::same_as<std::remove_cvref_t<U>, StrongAlias>) && std::constructible_from<T, U&&>
    explicit constexpr StrongAlias(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>) : value(std::forward<U>(value))
    {}

    /// Returns a mutable reference to the stored value.
    [[nodiscard]] constexpr auto Value() & noexcept -> T&
    {
      return value;
    }

    /// Returns a const reference to the stored value.
    [[nodiscard]] constexpr auto Value() const& noexcept -> const T&
    {
      return value;
    }

    /// Moves the stored value out of an rvalue alias.
    [[nodiscard]] constexpr auto Value() && noexcept -> T&&
    {
      return std::move(value);
    }

    /// Returns a const rvalue reference to the stored value.
    [[nodiscard]] constexpr auto Value() const&& noexcept -> const T&&
    {
      return std::move(value);
    }

    /// Compares two aliases by their underlying values.
    friend constexpr auto operator==(const StrongAlias& lhs, const StrongAlias& rhs) -> bool
    requires std::equality_comparable<T>
    {
      return lhs.value == rhs.value;
    }

    /// Orders two aliases by their underlying values.
    friend constexpr auto operator<=>(const StrongAlias& lhs, const StrongAlias& rhs)
    requires std::three_way_comparable<T>
    {
      return lhs.value <=> rhs.value;
    }

private:

    T value{};
  };

}

/// Hash support for hashable `ixx::StrongAlias` values.
///
/// The hash is delegated to the alias's underlying value.
export template <class T, class Tag>
requires ::ixx::Hashable<T>
struct std::hash<::ixx::StrongAlias<T, Tag>>
{
  /// Returns `std::hash<T>{}(value.Value())`.
  [[nodiscard]] constexpr auto operator()(const ::ixx::StrongAlias<T, Tag>& value) const noexcept(noexcept(std::hash<T>{}(value.Value())))
    -> std::size_t
  {
    return std::hash<T>{}(value.Value());
  }
};
