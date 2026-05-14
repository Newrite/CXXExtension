/// String utilities with explicit ASCII-oriented behavior.
///
/// This module provides small allocation-returning string helpers plus internal
/// ASCII predicates used by the parsing module.
export module IXXExtension.String;

import IXXExtension.Core;

import std;

namespace ixx
{

  /// Alias for `std::basic_string_view` used by string helpers.
  export template <class CharT, class Traits = std::char_traits<CharT>>
  using BasicStringView = std::basic_string_view<CharT, Traits>;

  /// Alias for `std::basic_string` used by allocation-returning helpers.
  export template <class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
  using BasicString = std::basic_string<CharT, Traits, Allocator>;

  /// Internal helpers shared by IXXExtension parsing and string utilities.
  ///
  /// These functions are exported for module composition, but they are not the
  /// primary public API surface.
  export namespace Internal
  {

    template <class CharT>
    concept CharacterCodeUnit = std::same_as<CharT, char> || std::same_as<CharT, wchar_t> || std::same_as<CharT, char8_t> ||
                                std::same_as<CharT, char16_t> || std::same_as<CharT, char32_t>;

    /// Returns whether a character is one of the supported ASCII whitespace bytes.
    ///
    /// Recognized characters are space, tab, carriage return, and newline.
    template <CharacterCodeUnit CharT>
    [[nodiscard]] constexpr auto IsAsciiSpace(CharT c) noexcept -> bool
    {
      return c == static_cast<CharT>(' ') || c == static_cast<CharT>('\t') || c == static_cast<CharT>('\r') ||
             c == static_cast<CharT>('\n');
    }

    /// Removes a leading `0x` or `0X` prefix from a string view.
    ///
    /// The returned view borrows from `s` and never allocates.
    template <CharacterCodeUnit CharT>
    [[nodiscard]] constexpr auto StripHexPrefix(BasicStringView<CharT> s) noexcept -> BasicStringView<CharT>
    {
      if (s.size() >= 2 && s[0] == static_cast<CharT>('0') && (s[1] == static_cast<CharT>('x') || s[1] == static_cast<CharT>('X')))
      {
        s.remove_prefix(2);
      }

      return s;
    }

    /// Trims supported ASCII whitespace from both ends of a string view.
    ///
    /// The returned view borrows from `s` and never allocates.
    template <CharacterCodeUnit CharT>
    [[nodiscard]] constexpr auto TrimAsciiView(BasicStringView<CharT> s) noexcept -> BasicStringView<CharT>
    {
      while (!s.empty() && IsAsciiSpace(s.front()))
      {
        s.remove_prefix(1);
      }

      while (!s.empty() && IsAsciiSpace(s.back()))
      {
        s.remove_suffix(1);
      }

      return s;
    }

    /// Trims supported ASCII whitespace from the left side of a string view.
    ///
    /// The returned view borrows from `s` and never allocates.
    template <CharacterCodeUnit CharT>
    [[nodiscard]] constexpr auto TrimLeftAsciiView(BasicStringView<CharT> s) noexcept -> BasicStringView<CharT>
    {
      while (!s.empty() && IsAsciiSpace(s.front()))
      {
        s.remove_prefix(1);
      }

      return s;
    }

    /// Trims supported ASCII whitespace from the right side of a string view.
    ///
    /// The returned view borrows from `s` and never allocates.
    template <CharacterCodeUnit CharT>
    [[nodiscard]] constexpr auto TrimRightAsciiView(BasicStringView<CharT> s) noexcept -> BasicStringView<CharT>
    {
      while (!s.empty() && IsAsciiSpace(s.back()))
      {
        s.remove_suffix(1);
      }

      return s;
    }

    /// Converts an ASCII uppercase byte to lowercase.
    ///
    /// Non-ASCII bytes and non-uppercase ASCII bytes are returned unchanged.
    template <CharacterCodeUnit CharT>
    [[nodiscard]] constexpr auto ToLowerAscii(CharT c) noexcept -> CharT
    {
      if (c >= static_cast<CharT>('A') && c <= static_cast<CharT>('Z'))
      {
        return static_cast<CharT>(c - static_cast<CharT>('A') + static_cast<CharT>('a'));
      }

      return c;
    }

    /// Converts an ASCII lowercase code unit to uppercase.
    ///
    /// Non-ASCII code units and non-lowercase ASCII code units are returned unchanged.
    template <CharacterCodeUnit CharT>
    [[nodiscard]] constexpr auto ToUpperAscii(CharT c) noexcept -> CharT
    {
      if (c >= static_cast<CharT>('a') && c <= static_cast<CharT>('z'))
      {
        return static_cast<CharT>(c - static_cast<CharT>('a') + static_cast<CharT>('A'));
      }

      return c;
    }

    /// Compares two strings using ASCII-only case folding.
    ///
    /// Unicode case folding is not performed.
    template <CharacterCodeUnit CharT>
    [[nodiscard]] constexpr auto IEqualsAscii(BasicStringView<CharT> a, BasicStringView<CharT> b) noexcept -> bool
    {
      if (a.size() != b.size())
      {
        return false;
      }

      for (std::size_t i = 0; i < a.size(); ++i)
      {
        if (ToLowerAscii(a[i]) != ToLowerAscii(b[i]))
        {
          return false;
        }
      }

      return true;
    }

    /// Compares a string view and a string literal using ASCII-only case folding.
    template <CharacterCodeUnit CharT, std::size_t Size>
    [[nodiscard]] constexpr auto IEqualsAscii(BasicStringView<CharT> a, const CharT (&b)[Size]) noexcept -> bool
    {
      return IEqualsAscii(a, BasicStringView<CharT>{b, Size - 1});
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

    template <class T>
    struct StringLikeChar;

    template <class CharT, class Traits, class Allocator>
    struct StringLikeChar<std::basic_string<CharT, Traits, Allocator>>
    {
      using type = CharT;
    };

    template <class CharT, class Traits>
    struct StringLikeChar<std::basic_string_view<CharT, Traits>>
    {
      using type = CharT;
    };

    template <class CharT>
    struct StringLikeChar<CharT*>
    {
      using type = std::remove_cv_t<CharT>;
    };

    template <class CharT, std::size_t Size>
    struct StringLikeChar<CharT[Size]>
    {
      using type = std::remove_cv_t<CharT>;
    };

    template <class T>
    using StringLikeCharT = typename StringLikeChar<std::remove_cvref_t<T>>::type;

    template <class T>
    concept StringLike = requires { typename StringLikeCharT<T>; };

    template <class T, class CharT>
    concept StringViewCompatible = std::constructible_from<std::basic_string_view<CharT>, T>;

    template <class T>
    requires StringLike<T>
    [[nodiscard]] constexpr auto AsStringView(T&& value) noexcept -> std::basic_string_view<StringLikeCharT<T>>
    {
      using CharT = StringLikeCharT<T>;

      return std::basic_string_view<CharT>{value};
    }

  }

  /// Returns a copy of a string with ASCII whitespace removed from both ends.
  ///
  /// Only space, tab, carriage return, and newline are treated as whitespace.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto value = ixx::TrimAscii("  hello\n"); // "hello"
  /// ```
  export template <Internal::StringLike Source>
  requires Internal::CharacterCodeUnit<Internal::StringLikeCharT<Source>>
  [[nodiscard]] auto TrimAscii(Source&& source) -> BasicString<Internal::StringLikeCharT<Source>>
  {
    using CharT = Internal::StringLikeCharT<Source>;

    const BasicStringView<CharT> view = Internal::AsStringView(std::forward<Source>(source));

    return BasicString<CharT>{Internal::TrimAsciiView(view)};
  }

  /// Returns a copy of a string with ASCII whitespace removed from the left.
  ///
  /// Only space, tab, carriage return, and newline are trimmed.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto value = ixx::TrimLeftAscii("\tname ");
  /// ```
  export template <Internal::StringLike Source>
  requires Internal::CharacterCodeUnit<Internal::StringLikeCharT<Source>>
  [[nodiscard]] auto TrimLeftAscii(Source&& source) -> BasicString<Internal::StringLikeCharT<Source>>
  {
    using CharT = Internal::StringLikeCharT<Source>;

    const BasicStringView<CharT> view = Internal::AsStringView(std::forward<Source>(source));

    return BasicString<CharT>{Internal::TrimLeftAsciiView(view)};
  }

  /// Returns a copy of a string with ASCII whitespace removed from the right.
  ///
  /// Only space, tab, carriage return, and newline are trimmed.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto value = ixx::TrimRightAscii("name \r\n");
  /// ```
  export template <Internal::StringLike Source>
  requires Internal::CharacterCodeUnit<Internal::StringLikeCharT<Source>>
  [[nodiscard]] auto TrimRightAscii(Source&& source) -> BasicString<Internal::StringLikeCharT<Source>>
  {
    using CharT = Internal::StringLikeCharT<Source>;

    const BasicStringView<CharT> view = Internal::AsStringView(std::forward<Source>(source));

    return BasicString<CharT>{Internal::TrimRightAsciiView(view)};
  }

  /// Returns an uppercase copy using ASCII-only case conversion.
  ///
  /// Non-ASCII code units are copied unchanged. The returned string owns the
  /// converted text and preserves the source character type.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto value = ixx::ToUpperAscii("ready"); // "READY"
  /// ```
  export template <Internal::StringLike Source>
  requires Internal::CharacterCodeUnit<Internal::StringLikeCharT<Source>>
  [[nodiscard]] auto ToUpperAscii(Source&& source) -> BasicString<Internal::StringLikeCharT<Source>>
  {
    using CharT = Internal::StringLikeCharT<Source>;

    const BasicStringView<CharT> view = Internal::AsStringView(std::forward<Source>(source));

    BasicString<CharT> result{view};

    std::ranges::transform(result, result.begin(), [](CharT c) { return Internal::ToUpperAscii(c); });

    return result;
  }

  /// Returns a lowercase copy using ASCII-only case conversion.
  ///
  /// Non-ASCII code units are copied unchanged. The returned string owns the
  /// converted text and preserves the source character type.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto value = ixx::ToLowerAscii("READY"); // "ready"
  /// ```
  export template <Internal::StringLike Source>
  requires Internal::CharacterCodeUnit<Internal::StringLikeCharT<Source>>
  [[nodiscard]] auto ToLowerAscii(Source&& source) -> BasicString<Internal::StringLikeCharT<Source>>
  {
    using CharT = Internal::StringLikeCharT<Source>;

    const BasicStringView<CharT> view = Internal::AsStringView(std::forward<Source>(source));

    BasicString<CharT> result{view};

    std::ranges::transform(result, result.begin(), [](CharT c) { return Internal::ToLowerAscii(c); });

    return result;
  }

  /// Splits a string into owned parts separated by one delimiter character.
  ///
  /// Empty fields are preserved, including leading and trailing empty fields.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto parts = ixx::Split("a,b,c", ',');
  /// ```
  export template <Internal::StringLike Source>
  [[nodiscard]] auto Split(Source&& source, Internal::StringLikeCharT<Source> delimiter)
    -> std::vector<BasicString<Internal::StringLikeCharT<Source>>>
  {
    using CharT = Internal::StringLikeCharT<Source>;

    const BasicStringView<CharT> src = Internal::AsStringView(std::forward<Source>(source));

    std::vector<BasicString<CharT>> result;

    std::size_t pos = 0;

    while (true)
    {
      const std::size_t next = src.find(delimiter, pos);

      if (next == BasicStringView<CharT>::npos)
      {
        result.emplace_back(src.substr(pos));
        break;
      }

      result.emplace_back(src.substr(pos, next - pos));
      pos = next + 1;
    }

    return result;
  }

  /// Splits a string into owned parts separated by a string separator.
  ///
  /// Empty fields are preserved, including leading and trailing empty fields.
  /// If `separator` is empty, the result contains the whole source string.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto parts = ixx::Split("red::green::blue", "::");
  /// ```
  export template <Internal::StringLike Source, class Separator>
  requires Internal::StringViewCompatible<Separator, Internal::StringLikeCharT<Source>>
  [[nodiscard]] auto Split(Source&& source, Separator&& separator) -> std::vector<BasicString<Internal::StringLikeCharT<Source>>>
  {
    using CharT = Internal::StringLikeCharT<Source>;

    const BasicStringView<CharT> src = Internal::AsStringView(std::forward<Source>(source));

    const BasicStringView<CharT> sep = BasicStringView<CharT>{std::forward<Separator>(separator)};

    std::vector<BasicString<CharT>> result;

    if (sep.empty())
    {
      result.emplace_back(src);
      return result;
    }

    std::size_t pos = 0;

    while (true)
    {
      const std::size_t next = src.find(sep, pos);

      if (next == BasicStringView<CharT>::npos)
      {
        result.emplace_back(src.substr(pos));
        break;
      }

      result.emplace_back(src.substr(pos, next - pos));
      pos = next + sep.size();
    }

    return result;
  }

  /// Joins a range of string-view-compatible parts with a separator.
  ///
  /// The result is newly allocated. Iteration order is preserved.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// std::array parts{"red", "green", "blue"};
  /// auto csv = ixx::Join(parts, ",");
  /// ```
  ///
  /// @tparam R Input range whose references can construct `std::string_view`.
  /// @param parts Range of string-like parts.
  /// @param separator Separator inserted between parts.
  /// @return Joined string.
  export template <std::ranges::input_range R, Internal::StringLike Separator>
  requires Internal::StringViewCompatible<std::ranges::range_reference_t<R>, Internal::StringLikeCharT<Separator>>
  [[nodiscard]] auto Join(R&& parts, Separator&& separator) -> BasicString<Internal::StringLikeCharT<Separator>>
  {
    using CharT = Internal::StringLikeCharT<Separator>;

    const BasicStringView<CharT> sep = Internal::AsStringView(std::forward<Separator>(separator));

    BasicString<CharT> result;
    bool               first = true;

    for (auto&& part : parts)
    {
      if (!first)
      {
        result.append(sep);
      }

      first = false;
      result.append(BasicStringView<CharT>{part});
    }

    return result;
  }

  /// Replaces every non-overlapping occurrence of one substring.
  ///
  /// `source`, `from`, and `to` are borrowed only for the duration of the call;
  /// the returned string owns the result.
  ///
  /// If `from` is empty, the function returns a copy of `source`.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto text = ixx::ReplaceAll("one fish, two fish", "fish", "cat");
  /// ```
  export template <Internal::StringLike Source, class From, class To>
  requires Internal::StringViewCompatible<From, Internal::StringLikeCharT<Source>> &&
           Internal::StringViewCompatible<To, Internal::StringLikeCharT<Source>>
  [[nodiscard]] auto ReplaceAll(Source&& source, From&& from, To&& to) -> BasicString<Internal::StringLikeCharT<Source>>
  {
    using CharT = Internal::StringLikeCharT<Source>;

    const BasicStringView<CharT> src  = Internal::AsStringView(std::forward<Source>(source));
    const BasicStringView<CharT> old  = BasicStringView<CharT>{std::forward<From>(from)};
    const BasicStringView<CharT> repl = BasicStringView<CharT>{std::forward<To>(to)};

    if (old.empty())
    {
      return BasicString<CharT>{src};
    }

    BasicString<CharT> result;
    result.reserve(src.size());

    std::size_t pos = 0;

    while (true)
    {
      const std::size_t next = src.find(old, pos);

      if (next == BasicStringView<CharT>::npos)
      {
        result.append(src.substr(pos));
        break;
      }

      result.append(src.substr(pos, next - pos));
      result.append(repl);

      pos = next + old.size();
    }

    return result;
  }

}
