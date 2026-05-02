/// Parsing helpers that return `cxx::Result<T>` instead of throwing.
///
/// Parsers trim ASCII whitespace, require the entire trimmed input to be
/// consumed, and report failures as `cxx::Error` with `Errc::ParseFailed`.
export module CXXExtension.Parse;

import CXXExtension.Core;
import CXXExtension.String;

import std;

namespace cxx
{

  /// Concept for integer types accepted by numeric parsers.
  ///
  /// `bool` is intentionally excluded even though it is an integral type.
  export template <class T>
  concept ParseInteger = std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>;

  /// Parses an integer from a trimmed string using `std::from_chars`.
  ///
  /// The entire trimmed input must be consumed. `base` must be in `[2, 36]`.
  /// Prefixes such as `0x` are not interpreted here; use `ParseHex` for
  /// hexadecimal input that may contain a prefix.
  ///
  /// ## Error handling
  ///
  /// Empty input, invalid base, invalid characters, trailing characters, and
  /// overflow produce `Errc::ParseFailed`.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto value = cxx::ParseIntegral<int>("101", 2);
  /// if (value) {
  ///   // *value == 5
  /// }
  /// ```
  ///
  /// @tparam T Non-bool integral output type.
  /// @param input Text to parse.
  /// @param base Integer base from 2 through 36.
  /// @return Parsed value or parse error.
  export template <ParseInteger T>
  [[nodiscard]] auto ParseIntegral(std::string_view input, int base = 10) -> Result<T>
  {
    const auto original = input;
    input               = Internal::TrimAsciiView(input);

    if (input.empty()) return Internal::ParseFailure<T>("ParseIntegral", original, "empty input");

    if (base < 2 || base > 36) return Internal::ParseFailure<T>("ParseIntegral", original, "base must be in [2, 36]");

    T value{};

    const char* first = input.data();
    const char* last  = input.data() + input.size();

    const auto [ptr, ec] = std::from_chars(first, last, value, base);

    if (ec == std::errc::invalid_argument) return Internal::ParseFailure<T>("ParseIntegral", original, "not an integer");

    if (ec == std::errc::result_out_of_range) return Internal::ParseFailure<T>("ParseIntegral", original, "integer out of range");

    if (ec != std::errc{}) return Internal::ParseFailure<T>("ParseIntegral", original, "integer parse failed");

    if (ptr != last) return Internal::ParseFailure<T>("ParseIntegral", original, "trailing characters");

    return value;
  }

  /// Parses a signed integer using decimal by default.
  ///
  /// @tparam T Signed integral output type. Defaults to `std::int64_t`.
  /// @param input Text to parse.
  /// @param base Integer base from 2 through 36.
  export template <std::signed_integral T = std::int64_t>
  [[nodiscard]] auto ParseInt(std::string_view input, int base = 10) -> Result<T>
  {
    return ParseIntegral<T>(input, base);
  }

  /// Parses an unsigned integer using decimal by default.
  ///
  /// @tparam T Unsigned integral output type. Defaults to `std::uint64_t`.
  /// @param input Text to parse.
  /// @param base Integer base from 2 through 36.
  export template <std::unsigned_integral T = std::uint64_t>
  [[nodiscard]] auto ParseUInt(std::string_view input, int base = 10) -> Result<T>
  {
    return ParseIntegral<T>(input, base);
  }

  /// Parses a floating-point value using `std::from_chars`.
  ///
  /// The entire trimmed input must be consumed.
  ///
  /// ## Error handling
  ///
  /// Empty input, invalid characters, trailing characters, and out-of-range
  /// values produce `Errc::ParseFailed`.
  ///
  /// @tparam T Floating-point output type.
  /// @param input Text to parse.
  /// @param format `std::chars_format` accepted by `std::from_chars`.
  /// @return Parsed value or parse error.
  export template <std::floating_point T>
  [[nodiscard]] auto ParseFloating(std::string_view input, std::chars_format format = std::chars_format::general) -> Result<T>
  {
    const auto original = input;
    input               = Internal::TrimAsciiView(input);

    if (input.empty()) return Internal::ParseFailure<T>("ParseFloating", original, "empty input");

    T value{};

    const char* first = input.data();
    const char* last  = input.data() + input.size();

    const auto [ptr, ec] = std::from_chars(first, last, value, format);

    if (ec == std::errc::invalid_argument) return Internal::ParseFailure<T>("ParseFloating", original, "not a floating-point number");

    if (ec == std::errc::result_out_of_range)
      return Internal::ParseFailure<T>("ParseFloating", original, "floating-point value out of range");

    if (ec != std::errc{}) return Internal::ParseFailure<T>("ParseFloating", original, "floating-point parse failed");

    if (ptr != last) return Internal::ParseFailure<T>("ParseFloating", original, "trailing characters");

    return value;
  }

  /// Parses a `float`.
  export [[nodiscard]] inline auto ParseFloat(std::string_view input) -> Result<float>
  {
    return ParseFloating<float>(input);
  }

  /// Parses a `double`.
  export [[nodiscard]] inline auto ParseDouble(std::string_view input) -> Result<double>
  {
    return ParseFloating<double>(input);
  }

  /// Parses a `long double`.
  export [[nodiscard]] inline auto ParseLongDouble(std::string_view input) -> Result<long double>
  {
    return ParseFloating<long double>(input);
  }

  /// Parses either an integer or floating-point value based on `T`.
  ///
  /// `bool` is not accepted by this helper; use `Parse<bool>` or `ParseBool`.
  export template <class T>
  requires ParseInteger<T> || std::floating_point<T>
  [[nodiscard]] auto ParseNumber(std::string_view input) -> Result<T>
  {
    if constexpr (ParseInteger<T>)
    {
      return ParseIntegral<T>(input);
    }
    else
    {
      return ParseFloating<T>(input);
    }
  }

  /// Parses a hexadecimal integer.
  ///
  /// Leading and trailing ASCII whitespace are ignored. `0x` and `0X` prefixes
  /// are accepted. Signed integers may start with `-`; unsigned integers reject
  /// negative input. A leading `+` is accepted for unsigned input.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto id = cxx::ParseHex<std::uint32_t>("0xFF");
  /// ```
  export template <ParseInteger T>
  [[nodiscard]] auto ParseHex(std::string_view input) -> Result<T>
  {
    const auto original = input;

    input = Internal::TrimAsciiView(input);

    if constexpr (std::signed_integral<T>)
    {
      bool negative = false;

      if (!input.empty() && input.front() == '-')
      {
        negative = true;
        input.remove_prefix(1);
      }

      input = Internal::StripHexPrefix(input);

      if (input.empty()) return Internal::ParseFailure<T>("ParseHex", original, "empty hex value");

      if (negative)
      {
        std::string normalized;
        normalized.reserve(input.size() + 1);
        normalized += '-';
        normalized += input;

        return ParseIntegral<T>(normalized, 16);
      }

      return ParseIntegral<T>(input, 16);
    }
    else
    {
      if (!input.empty() && input.front() == '-')
        return Internal::ParseFailure<T>("ParseHex", original, "negative value for unsigned integer");

      if (!input.empty() && input.front() == '+') input.remove_prefix(1);

      input = Internal::StripHexPrefix(input);

      if (input.empty()) return Internal::ParseFailure<T>("ParseHex", original, "empty hex value");

      return ParseIntegral<T>(input, 16);
    }
  }

  /// Parses hexadecimal text into `std::int64_t`.
  export [[nodiscard]] inline auto HexToInt64(std::string_view input) -> Result<std::int64_t>
  {
    return ParseHex<std::int64_t>(input);
  }

  /// Parses hexadecimal text into `std::uint32_t`.
  export [[nodiscard]] inline auto HexToUInt32(std::string_view input) -> Result<std::uint32_t>
  {
    return ParseHex<std::uint32_t>(input);
  }

  /// Controls which boolean spellings `ParseBool` accepts.
  export enum class BoolParseMode
  {
    /// Accept only `true` and `false`, compared ASCII case-insensitively.
    Strict,
    /// Accept `true`, `false`, `1`, `0`, `yes`, `no`, `on`, `off`, `enabled`, and `disabled`.
    Relaxed
  };

  /// Parses a boolean value.
  ///
  /// Comparison is ASCII case-insensitive for word values.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto enabled = cxx::ParseBool("yes");
  /// auto strict = cxx::ParseBool("1", cxx::BoolParseMode::Strict); // error
  /// ```
  export [[nodiscard]] inline auto ParseBool(std::string_view input, BoolParseMode mode = BoolParseMode::Relaxed) -> Result<bool>
  {
    const auto original = input;
    input               = Internal::TrimAsciiView(input);

    if (input.empty()) return Internal::ParseFailure<bool>("ParseBool", original, "empty input");

    if (Internal::IEqualsAscii(input, "true")) return true;

    if (Internal::IEqualsAscii(input, "false")) return false;

    if (mode == BoolParseMode::Strict) return Internal::ParseFailure<bool>("ParseBool", original, "expected `true` or `false`");

    if (
      input == "1" || Internal::IEqualsAscii(input, "yes") || Internal::IEqualsAscii(input, "on") ||
      Internal::IEqualsAscii(input, "enabled"))
      return true;

    if (
      input == "0" || Internal::IEqualsAscii(input, "no") || Internal::IEqualsAscii(input, "off") ||
      Internal::IEqualsAscii(input, "disabled"))
      return false;

    return Internal::ParseFailure<bool>("ParseBool", original, "not a boolean");
  }

  /// Parses a supported scalar type.
  ///
  /// Supported `T` values are non-bool integers, floating-point types, and
  /// `bool`.
  export template <class T>
  requires ParseInteger<T> || std::floating_point<T> || std::same_as<T, bool>
  [[nodiscard]] auto Parse(std::string_view input) -> Result<T>
  {
    if constexpr (std::same_as<T, bool>)
    {
      return ParseBool(input);
    }
    else if constexpr (ParseInteger<T>)
    {
      return ParseIntegral<T>(input);
    }
    else
    {
      return ParseFloating<T>(input);
    }
  }

  /// Controls case matching for enum parsing.
  export enum class CaseMode
  {
    /// Match names exactly.
    Sensitive,
    /// Match names with ASCII-only case folding.
    InsensitiveAscii
  };

  /// Parses an enum from an explicit name/value table.
  ///
  /// The input is trimmed before matching. The initializer-list table is borrowed
  /// only for the duration of the call.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// enum class Mode { Read, Write };
  ///
  /// auto mode = cxx::ParseEnum<Mode>("read", {
  ///   {"read", Mode::Read},
  ///   {"write", Mode::Write},
  /// });
  /// ```
  ///
  /// @tparam E Enum output type.
  /// @param input Text to parse.
  /// @param values Name/value table.
  /// @param caseMode Case matching mode.
  /// @return Parsed enum value or parse error.
  export template <class E>
  requires std::is_enum_v<E>
  [[nodiscard]] auto ParseEnum(
    std::string_view                                      input,
    std::initializer_list<std::pair<std::string_view, E>> values,
    CaseMode                                              caseMode = CaseMode::InsensitiveAscii) -> Result<E>
  {
    const auto original = input;
    input               = Internal::TrimAsciiView(input);

    if (input.empty()) return Internal::ParseFailure<E>("ParseEnum", original, "empty input");

    for (const auto& [name, value] : values)
    {
      const bool match = caseMode == CaseMode::Sensitive ? input == name : Internal::IEqualsAscii(input, name);

      if (match) return value;
    }

    return Internal::ParseFailure<E>("ParseEnum", original, "unknown enum value");
  }

  /// Parses an optional scalar where empty input means `std::nullopt`.
  ///
  /// Non-empty input is parsed with `Parse<T>`.
  export template <class T>
  requires ParseInteger<T> || std::floating_point<T> || std::same_as<T, bool>
  [[nodiscard]] auto ParseOptional(std::string_view input) -> Result<std::optional<T>>
  {
    input = Internal::TrimAsciiView(input);

    if (input.empty()) return std::optional<T>{};

    auto parsed = Parse<T>(input);

    if (!parsed) return std::unexpected(parsed.error());

    return std::optional<T>{std::move(*parsed)};
  }

  /// Parses a delimiter-separated list of supported scalar values.
  ///
  /// Empty trimmed input returns an empty vector. Each element is parsed with
  /// `Parse<T>`, so whitespace around elements is accepted.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto numbers = cxx::ParseList<int>("1, 2, 3");
  /// ```
  export template <class T>
  requires ParseInteger<T> || std::floating_point<T> || std::same_as<T, bool>
  [[nodiscard]] auto ParseList(std::string_view input, char delimiter = ',') -> Result<std::vector<T>>
  {
    std::vector<T> result;

    input = Internal::TrimAsciiView(input);

    if (input.empty()) return result;

    size_t pos = 0;

    while (true)
    {
      const size_t next = input.find(delimiter, pos);

      const std::string_view part = next == std::string_view::npos ? input.substr(pos) : input.substr(pos, next - pos);

      auto parsed = Parse<T>(part);

      if (!parsed) return std::unexpected(parsed.error());

      result.emplace_back(std::move(*parsed));

      if (next == std::string_view::npos) break;

      pos = next + 1;
    }

    return result;
  }

}
