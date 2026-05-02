/// String utilities with explicit ASCII-oriented behavior.
///
/// This module provides small allocation-returning string helpers plus internal
/// ASCII predicates used by the parsing module.
export module CXXExtension.String;

import CXXExtension.Core;

import std;

namespace cxx
{

  /// Internal helpers shared by CXXExtension parsing and string utilities.
  ///
  /// These functions are exported for module composition, but they are not the
  /// primary public API surface.
  export namespace Internal
  {

    /// Returns whether a character is one of the supported ASCII whitespace bytes.
    ///
    /// Recognized characters are space, tab, carriage return, and newline.
    [[nodiscard]] constexpr auto IsAsciiSpace(char c) noexcept -> bool
    {
      return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    /// Removes a leading `0x` or `0X` prefix from a string view.
    ///
    /// The returned view borrows from `s` and never allocates.
    [[nodiscard]] constexpr auto StripHexPrefix(std::string_view s) noexcept -> std::string_view
    {
      if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s.remove_prefix(2);

      return s;
    }

    /// Trims supported ASCII whitespace from both ends of a string view.
    ///
    /// The returned view borrows from `s` and never allocates.
    [[nodiscard]] constexpr auto TrimAsciiView(std::string_view s) noexcept -> std::string_view
    {
      while (!s.empty() && IsAsciiSpace(s.front()))
        s.remove_prefix(1);

      while (!s.empty() && IsAsciiSpace(s.back()))
        s.remove_suffix(1);

      return s;
    }

    /// Converts an ASCII uppercase byte to lowercase.
    ///
    /// Non-ASCII bytes and non-uppercase ASCII bytes are returned unchanged.
    [[nodiscard]] constexpr auto ToLowerAscii(char c) noexcept -> char
    {
      if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');

      return c;
    }

    /// Compares two strings using ASCII-only case folding.
    ///
    /// Unicode case folding is not performed.
    [[nodiscard]] constexpr auto IEqualsAscii(std::string_view a, std::string_view b) noexcept -> bool
    {
      if (a.size() != b.size()) return false;

      for (size_t i = 0; i < a.size(); ++i)
      {
        if (ToLowerAscii(a[i]) != ToLowerAscii(b[i])) return false;
      }

      return true;
    }

    /// Creates a parse error with consistent message text.
    ///
    /// The returned error has `Errc::ParseFailed` and uses `function` as the
    /// operation override.
    [[nodiscard]] inline auto MakeParseError(
      std::string_view           function,
      std::string_view           input,
      std::string_view           reason,
      const std::source_location where = std::source_location::current()) -> Error
    {
      std::string message;
      message += function;
      message += ": ";
      message += reason;
      message += " [input: `";
      message += input;
      message += "`]";

      return Error::Make(Errc::ParseFailed, std::move(message), std::string{function}, where);
    }

    /// Returns a failed `Result<T>` with a parse error.
    template <class T>
    [[nodiscard]] auto ParseFailure(
      std::string_view           function,
      std::string_view           input,
      std::string_view           reason,
      const std::source_location where = std::source_location::current()) -> Result<T>
    {
      return std::unexpected{MakeParseError(function, input, reason, where)};
    }

  }

  /// Returns a copy of a string with ASCII whitespace removed from both ends.
  ///
  /// Only space, tab, carriage return, and newline are treated as whitespace.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto value = cxx::Trim("  hello\n"); // "hello"
  /// ```
  export auto Trim(std::string_view str) -> std::string
  {
    return std::string{Internal::TrimAsciiView(str)};
  }

  /// Splits a string into owned parts separated by one delimiter character.
  ///
  /// Empty fields are preserved according to `std::views::split` behavior.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto parts = cxx::Split("a,b,c", ',');
  /// ```
  export auto Split(std::string_view str, char delimiter) -> std::vector<std::string>
  {
    return str | std::views::split(delimiter) | std::ranges::to<std::vector<std::string>>();
  }

  /// Returns a copy of a string with ASCII whitespace removed from the left.
  ///
  /// Only space, tab, carriage return, and newline are trimmed.
  export auto TrimLeft(std::string_view s) -> std::string
  {
    auto view = s | std::views::drop_while([](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; });

    return std::string{view.begin(), view.end()};
  }

  /// Returns an upper-case copy using the active C locale classification.
  ///
  /// The input string is passed by value and modified in place before being
  /// returned.
  export auto ToUpper(std::string str) -> std::string
  {
    std::ranges::transform(str, str.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    return str;
  }

  /// Returns a lower-case copy using the active C locale classification.
  ///
  /// The input string is passed by value and modified in place before being
  /// returned.
  export auto ToLower(std::string str) -> std::string
  {
    std::ranges::transform(str, str.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return str;
  }

  /// Joins a range of string-view-compatible parts with a separator.
  ///
  /// The result is newly allocated. Iteration order is preserved.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// std::array parts{"red", "green", "blue"};
  /// auto csv = cxx::Join(parts, ",");
  /// ```
  ///
  /// @tparam R Input range whose references can construct `std::string_view`.
  /// @param parts Range of string-like parts.
  /// @param separator Separator inserted between parts.
  /// @return Joined string.
  export template <std::ranges::input_range R>
  requires std::constructible_from<std::string_view, std::ranges::range_reference_t<R>>
  auto Join(R&& parts, std::string_view separator) -> std::string
  {
    std::string result;
    bool        first = true;

    for (auto&& part : parts)
    {
      if (!first) result += separator;

      first   = false;
      result += std::string_view{part};
    }

    return result;
  }

  /// Replaces every non-overlapping occurrence of one substring.
  ///
  /// `source`, `from`, and `to` are borrowed only for the duration of the call;
  /// the returned string owns the result.
  ///
  /// @warning If `from` is empty, this function currently returns an empty
  /// string.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto text = cxx::ReplaceAll("one fish, two fish", "fish", "cat");
  /// ```
  export auto ReplaceAll(std::string_view source, std::string_view from, std::string_view to) -> std::string
  {
    if (from.empty()) return "";

    std::string result;
    size_t      pos = 0;

    while (true)
    {
      const size_t next = source.find(from, pos);

      if (next == std::string_view::npos)
      {
        result += source.substr(pos);
        break;
      }

      result += source.substr(pos, next - pos);
      result += to;
      pos     = next + from.size();
    }

    return result;
  }

}
