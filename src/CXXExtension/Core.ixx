export module CXXExtension.Core;

import std;

namespace cxx
{

  export template <class Enum>
  struct ErrorCodeTraits;

  export template <class Enum>
  concept ErrorCodeEnum = std::is_enum_v<Enum> && requires(Enum code) {
    { ErrorCodeTraits<Enum>::Name } -> std::convertible_to<const char*>;
    { ErrorCodeTraits<Enum>::Message(code) } -> std::convertible_to<std::string_view>;
  };

  export template <class Enum>
  requires ErrorCodeEnum<Enum>
  class EnumErrorCategory final : public std::error_category
  {
public:

    [[nodiscard]] auto name() const noexcept -> const char* override
    {
      return ErrorCodeTraits<Enum>::Name;
    }

    [[nodiscard]] auto message(const int value) const -> std::string override
    {
      const auto code = static_cast<Enum>(value);
      return std::string{ErrorCodeTraits<Enum>::Message(code)};
    }
  };

  export template <class Enum>
  requires ErrorCodeEnum<Enum>
  [[nodiscard]] auto ErrorCategory() noexcept -> const std::error_category&
  {
    static const EnumErrorCategory<Enum> category{};
    return category;
  }

  export template <class Enum>
  requires ErrorCodeEnum<Enum>
  [[nodiscard]] auto MakeErrorCode(const Enum code) noexcept -> std::error_code
  {
    return {
        static_cast<int>(code),
        ErrorCategory<Enum>(),
    };
  }

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

  export struct Error final
  {
    std::error_code              code{};
    std::string                  message{};
    std::string                  operationOverride{};
    std::source_location         where = std::source_location::current();
    std::shared_ptr<const Error> cause{};

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

    template <class Enum>
    requires ErrorCodeEnum<Enum>
    [[nodiscard]] static auto Make(
      const Enum                 code,
      std::string                message           = {},
      std::string                operationOverride = {},
      const std::source_location where             = std::source_location::current()) -> Error
    {
      return Make(cxx::MakeErrorCode(code), std::move(message), std::move(operationOverride), where);
    }

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

    template <class Enum>
    requires ErrorCodeEnum<Enum>
    [[nodiscard]] static auto Wrap(
      const Enum                 code,
      Error                      inner,
      std::string                message           = {},
      std::string                operationOverride = {},
      const std::source_location where             = std::source_location::current()) -> Error
    {
      return Wrap(cxx::MakeErrorCode(code), std::move(inner), std::move(message), std::move(operationOverride), where);
    }

    template <class Enum>
    requires ErrorCodeEnum<Enum>
    [[nodiscard]] auto Is(const Enum expected) const noexcept -> bool
    {
      return code == cxx::MakeErrorCode(expected);
    }

    [[nodiscard]] auto CategoryName() const noexcept -> std::string_view
    {
      return code.category().name();
    }

    [[nodiscard]] auto CodeMessage() const -> std::string
    {
      return code.message();
    }

    [[nodiscard]] auto Operation() const noexcept -> std::string_view
    {
      if (!operationOverride.empty())
      {
        return operationOverride;
      }

      return where.function_name();
    }

    [[nodiscard]] auto HasCause() const noexcept -> bool
    {
      return static_cast<bool>(cause);
    }

    [[nodiscard]] auto Cause() const noexcept -> const Error*
    {
      return cause ? cause.get() : nullptr;
    }

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

  export template <class T>
  using Result = std::expected<T, Error>;

  export using VoidResult = std::expected<void, Error>;

  export enum class Errc : std::uint16_t
  {
    None = 0,

    InvalidArgument = 1,
    InvalidFormat,
    ParseFailed,
    OutOfRange,
    EmptyInput,
    NotFound,
    Unsupported,
  };

  export template <>
  struct ErrorCodeTraits<Errc>
  {
    static constexpr const char* Name = "cxx";

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

  export template <class F>
  class ScopeExit
  {
public:

    template <class Fn>
    requires std::constructible_from<F, Fn>
    explicit ScopeExit(Fn&& fn) noexcept(std::is_nothrow_constructible_v<F, Fn>) : fn_(std::forward<Fn>(fn))
    {}

    ScopeExit(ScopeExit&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : fn_(std::move(other.fn_)),
          active_(std::exchange(other.active_, false))
    {}

    ScopeExit(const ScopeExit&) = delete;

    auto operator=(const ScopeExit&) -> ScopeExit& = delete;

    auto operator=(ScopeExit&&) -> ScopeExit& = delete;

    ~ScopeExit() noexcept
    {
      if (active_) fn_();
    }

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

  // Overloaded for std::visit
  export template <class... Ts>
  struct Overloaded : Ts...
  {
    using Ts::operator()...;
  };

  template <class... Ts>
  Overloaded(Ts...) -> Overloaded<Ts...>;

  // opt in bitmask for enum classes
  /*
    enum class ActorFlags : uint32_t
    {
        None   = 0,
        Dead   = 1 << 0,
        Hidden = 1 << 1,
        Loaded = 1 << 2,
    };

    template <>
    inline constexpr bool EnableBitmaskOperators<ActorFlags> = true;
     */
  export template <class E>
  inline constexpr bool EnableBitmaskOperators = false;

  export template <class E>
  concept BitmaskEnum = std::is_enum_v<E> && EnableBitmaskOperators<E>;

  export template <BitmaskEnum E>
  constexpr auto operator|(E lhs, E rhs) noexcept -> E
  {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
  }

  export template <BitmaskEnum E>
  constexpr auto operator&(E lhs, E rhs) noexcept -> E
  {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
  }

  export template <BitmaskEnum E>
  constexpr auto HasFlag(E value, E flag) noexcept -> bool
  {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
  }

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

  export template <std::integral T>
  constexpr auto FromBigEndian(T value) noexcept -> T
  {
    return ToBigEndian(value);
  }

  export template <class T>
  concept Hashable = requires(const T& value) {
    { std::hash<T>{}(value) } -> std::convertible_to<std::size_t>;
  };

  export template <class T, class Tag>
  class StrongAlias
  {
public:

    using underlying_type = T;
    using tag_type        = Tag;

    constexpr StrongAlias()
    requires std::default_initializable<T>
    = default;

    template <class U>
    requires(!std::same_as<std::remove_cvref_t<U>, StrongAlias>) && std::constructible_from<T, U&&>
    explicit constexpr StrongAlias(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>) : value(std::forward<U>(value))
    {}

    [[nodiscard]] constexpr auto Value() & noexcept -> T&
    {
      return value;
    }

    [[nodiscard]] constexpr auto Value() const& noexcept -> const T&
    {
      return value;
    }

    [[nodiscard]] constexpr auto Value() && noexcept -> T&&
    {
      return std::move(value);
    }

    [[nodiscard]] constexpr auto Value() const&& noexcept -> const T&&
    {
      return std::move(value);
    }

    friend constexpr auto operator==(const StrongAlias& lhs, const StrongAlias& rhs) -> bool
    requires std::equality_comparable<T>
    {
      return lhs.value == rhs.value;
    }

    friend constexpr auto operator<=>(const StrongAlias& lhs, const StrongAlias& rhs)
    requires std::three_way_comparable<T>
    {
      return lhs.value <=> rhs.value;
    }

private:

    T value{};
  };

}

export template <class T, class Tag>
requires ::cxx::Hashable<T>
struct std::hash<::cxx::StrongAlias<T, Tag>>
{
  [[nodiscard]] constexpr auto operator()(const ::cxx::StrongAlias<T, Tag>& value) const noexcept(noexcept(std::hash<T>{}(value.Value())))
    -> std::size_t
  {
    return std::hash<T>{}(value.Value());
  }
};
