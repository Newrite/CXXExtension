module;

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

  #include <Windows.h>
#endif

/// UTF-8 and platform text conversion utilities.
///
/// This module treats `std::string` text as UTF-8 by convention.
/// It does not implement Unicode normalization, Unicode case folding,
/// grapheme segmentation, or locale-sensitive transformations.
///
/// Responsibilities:
///
/// - validate UTF-8 byte sequences;
/// - bridge `std::string` and `std::u8string`;
/// - convert UTF-8 <-> UTF-16 on Windows through WinAPI;
/// - report failures through `ixx::Result` / `ixx::Error`.
export module IXXExtension.Text;

import IXXExtension.Core;

import std;

namespace ixx::text
{

  /// Text-specific errors.
  export enum class TextErrc : std::uint16_t
  {
    /// No error.
    None = 0,

    /// Input is too large for the target API.
    InputTooLarge,

    /// The input byte sequence is not valid UTF-8.
    InvalidUtf8,

    /// The input wide sequence is not valid UTF-16.
    InvalidUtf16,

    /// A platform conversion API failed.
    ConversionFailed,
  };

}

namespace ixx
{

  /// Error-code traits for `ixx::text::TextErrc`.
  export template <>
  struct ErrorCodeTraits<text::TextErrc>
  {
    static constexpr const char* Name = "text";

    [[nodiscard]] static constexpr auto Message(const text::TextErrc code) noexcept -> std::string_view
    {
      using enum text::TextErrc;

      switch (code)
      {
      case None:
        return "No error";
      case InputTooLarge:
        return "Input is too large";
      case InvalidUtf8:
        return "Invalid UTF-8";
      case InvalidUtf16:
        return "Invalid UTF-16";
      case ConversionFailed:
        return "Text conversion failed";
      default:
        return "Unknown text error";
      }
    }
  };

}

namespace ixx::text
{

  /// UTF-8 text stored in `std::string`.
  export using Utf8String = std::string;

  /// Non-owning view of UTF-8 text stored as `char` bytes.
  export using Utf8View = std::string_view;

  /// UTF-8 text stored in `std::u8string`.
  export using U8String = std::u8string;

  /// Non-owning view of UTF-8 text stored as `char8_t` code units.
  export using U8View = std::u8string_view;

#ifdef _WIN32
  /// Windows wide text, treated as UTF-16 on Windows.
  export using WideString = std::wstring;

  /// Non-owning view of Windows wide text.
  export using WideStringView = std::wstring_view;
#endif

  namespace Internal
  {

    template <class CharT>
    concept Utf8CodeUnit =
      std::same_as<std::remove_cv_t<CharT>, char> ||
      std::same_as<std::remove_cv_t<CharT>, char8_t>;

    template <Utf8CodeUnit CharT>
    [[nodiscard]] constexpr auto ToByte(const CharT c) noexcept -> unsigned char
    {
      return static_cast<unsigned char>(c);
    }

    [[nodiscard]] constexpr auto IsUtf8Continuation(const unsigned char byte) noexcept -> bool
    {
      return (byte & 0b1100'0000u) == 0b1000'0000u;
    }

    template <Utf8CodeUnit CharT>
    [[nodiscard]] constexpr auto ByteAt(std::basic_string_view<CharT> text, const std::size_t index) noexcept -> unsigned char
    {
      return ToByte(text[index]);
    }

    template <Utf8CodeUnit CharT>
    [[nodiscard]] constexpr auto HasBytes(std::basic_string_view<CharT> text, const std::size_t index, const std::size_t count) noexcept -> bool
    {
      return index <= text.size() && count <= text.size() - index;
    }

    [[nodiscard]] inline auto MakeTextError(
      const TextErrc             code,
      std::string                message,
      std::string                operation,
      const std::source_location where = std::source_location::current()) -> Error
    {
      return Error::Make(code, std::move(message), std::move(operation), where);
    }

    [[nodiscard]] inline auto MakeInvalidUtf8Error(
      const std::size_t          offset,
      const std::source_location where = std::source_location::current()) -> Error
    {
      std::string message;
      message += "invalid UTF-8 at byte ";
      message += std::to_string(offset);

      return MakeTextError(
        TextErrc::InvalidUtf8,
        std::move(message),
        "ixx::text::ValidateUtf8",
        where);
    }

#ifdef _WIN32

    [[nodiscard]] inline auto MakeWin32TextError(
      const TextErrc             outerCode,
      std::string                message,
      std::string                operation,
      const DWORD                win32Error,
      const std::source_location where = std::source_location::current()) -> Error
    {
      auto inner = Error::Make(
        std::error_code{
          static_cast<int>(win32Error),
          std::system_category(),
        },
        "Win32 text conversion API failed",
        operation,
        where);

      return Error::Wrap(
        outerCode,
        std::move(inner),
        std::move(message),
        std::move(operation),
        where);
    }

    [[nodiscard]] inline auto ClassifyWin32Utf8ToWideError(const DWORD win32Error) noexcept -> TextErrc
    {
      if (win32Error == ERROR_NO_UNICODE_TRANSLATION)
      {
        return TextErrc::InvalidUtf8;
      }

      return TextErrc::ConversionFailed;
    }

    [[nodiscard]] inline auto ClassifyWin32WideToUtf8Error(const DWORD win32Error) noexcept -> TextErrc
    {
      if (win32Error == ERROR_NO_UNICODE_TRANSLATION)
      {
        return TextErrc::InvalidUtf16;
      }

      return TextErrc::ConversionFailed;
    }

#endif

  }

  /// Returns the byte offset of the first invalid UTF-8 sequence.
  ///
  /// Validates UTF-8 structurally:
  ///
  /// - rejects overlong encodings;
  /// - rejects surrogate code points;
  /// - rejects code points above U+10FFFF;
  /// - rejects truncated sequences;
  /// - accepts embedded NUL bytes.
  ///
  /// The returned offset is a byte/code-unit offset into the input.
  export template <Internal::Utf8CodeUnit CharT>
  [[nodiscard]] constexpr auto FindInvalidUtf8Offset(std::basic_string_view<CharT> text) noexcept -> std::optional<std::size_t>
  {
    std::size_t i = 0;

    while (i < text.size())
    {
      const unsigned char b0 = Internal::ByteAt(text, i);

      if (b0 <= 0x7F)
      {
        ++i;
        continue;
      }

      if (b0 >= 0xC2 && b0 <= 0xDF)
      {
        if (!Internal::HasBytes(text, i, 2) ||
            !Internal::IsUtf8Continuation(Internal::ByteAt(text, i + 1)))
        {
          return i;
        }

        i += 2;
        continue;
      }

      if (b0 == 0xE0)
      {
        if (!Internal::HasBytes(text, i, 3))
        {
          return i;
        }

        const unsigned char b1 = Internal::ByteAt(text, i + 1);
        const unsigned char b2 = Internal::ByteAt(text, i + 2);

        if (b1 < 0xA0 || b1 > 0xBF || !Internal::IsUtf8Continuation(b2))
        {
          return i;
        }

        i += 3;
        continue;
      }

      if ((b0 >= 0xE1 && b0 <= 0xEC) || (b0 >= 0xEE && b0 <= 0xEF))
      {
        if (!Internal::HasBytes(text, i, 3) ||
            !Internal::IsUtf8Continuation(Internal::ByteAt(text, i + 1)) ||
            !Internal::IsUtf8Continuation(Internal::ByteAt(text, i + 2)))
        {
          return i;
        }

        i += 3;
        continue;
      }

      if (b0 == 0xED)
      {
        if (!Internal::HasBytes(text, i, 3))
        {
          return i;
        }

        const unsigned char b1 = Internal::ByteAt(text, i + 1);
        const unsigned char b2 = Internal::ByteAt(text, i + 2);

        // U+D800..U+DFFF surrogate range is invalid in UTF-8.
        if (b1 < 0x80 || b1 > 0x9F || !Internal::IsUtf8Continuation(b2))
        {
          return i;
        }

        i += 3;
        continue;
      }

      if (b0 == 0xF0)
      {
        if (!Internal::HasBytes(text, i, 4))
        {
          return i;
        }

        const unsigned char b1 = Internal::ByteAt(text, i + 1);
        const unsigned char b2 = Internal::ByteAt(text, i + 2);
        const unsigned char b3 = Internal::ByteAt(text, i + 3);

        if (b1 < 0x90 || b1 > 0xBF ||
            !Internal::IsUtf8Continuation(b2) ||
            !Internal::IsUtf8Continuation(b3))
        {
          return i;
        }

        i += 4;
        continue;
      }

      if (b0 >= 0xF1 && b0 <= 0xF3)
      {
        if (!Internal::HasBytes(text, i, 4) ||
            !Internal::IsUtf8Continuation(Internal::ByteAt(text, i + 1)) ||
            !Internal::IsUtf8Continuation(Internal::ByteAt(text, i + 2)) ||
            !Internal::IsUtf8Continuation(Internal::ByteAt(text, i + 3)))
        {
          return i;
        }

        i += 4;
        continue;
      }

      if (b0 == 0xF4)
      {
        if (!Internal::HasBytes(text, i, 4))
        {
          return i;
        }

        const unsigned char b1 = Internal::ByteAt(text, i + 1);
        const unsigned char b2 = Internal::ByteAt(text, i + 2);
        const unsigned char b3 = Internal::ByteAt(text, i + 3);

        if (b1 < 0x80 || b1 > 0x8F ||
            !Internal::IsUtf8Continuation(b2) ||
            !Internal::IsUtf8Continuation(b3))
        {
          return i;
        }

        i += 4;
        continue;
      }

      return i;
    }

    return std::nullopt;
  }

  /// Returns the byte offset of the first invalid UTF-8 sequence.
  export [[nodiscard]] constexpr auto FindInvalidUtf8Offset(const std::string_view text) noexcept -> std::optional<std::size_t>
  {
    return FindInvalidUtf8Offset<char>(text);
  }

  /// Returns the byte offset of the first invalid UTF-8 sequence.
  export [[nodiscard]] constexpr auto FindInvalidUtf8Offset(const std::u8string_view text) noexcept -> std::optional<std::size_t>
  {
    return FindInvalidUtf8Offset<char8_t>(text);
  }

  /// Validates UTF-8 input.
  ///
  /// On success returns an empty `VoidResult`.
  /// On failure returns `TextErrc::InvalidUtf8` with the failing byte offset.
  export template <Internal::Utf8CodeUnit CharT>
  [[nodiscard]] auto ValidateUtf8(
    const std::basic_string_view<CharT> text,
    const std::source_location          where = std::source_location::current()) -> VoidResult
  {
    if (const auto invalidOffset = FindInvalidUtf8Offset(text))
    {
      return std::unexpected{
        Internal::MakeInvalidUtf8Error(*invalidOffset, where),
      };
    }

    return {};
  }

  /// Validates UTF-8 input stored as `char` bytes.
  export [[nodiscard]] auto ValidateUtf8(
    const std::string_view     text,
    const std::source_location where = std::source_location::current()) -> VoidResult
  {
    return ValidateUtf8<char>(text, where);
  }

  /// Validates UTF-8 input stored as `char8_t` code units.
  export [[nodiscard]] auto ValidateUtf8(
    const std::u8string_view   text,
    const std::source_location where = std::source_location::current()) -> VoidResult
  {
    return ValidateUtf8<char8_t>(text, where);
  }

  /// Returns whether input is structurally valid UTF-8.
  export template <Internal::Utf8CodeUnit CharT>
  [[nodiscard]] constexpr auto IsValidUtf8(const std::basic_string_view<CharT> text) noexcept -> bool
  {
    return !FindInvalidUtf8Offset(text).has_value();
  }

  /// Returns whether `char` input is structurally valid UTF-8.
  export [[nodiscard]] constexpr auto IsValidUtf8(const std::string_view text) noexcept -> bool
  {
    return IsValidUtf8<char>(text);
  }

  /// Returns whether `char8_t` input is structurally valid UTF-8.
  export [[nodiscard]] constexpr auto IsValidUtf8(const std::u8string_view text) noexcept -> bool
  {
    return IsValidUtf8<char8_t>(text);
  }

  /// Copies UTF-8 code units from `std::u8string_view` into `std::string`.
  ///
  /// This is a byte-preserving bridge for APIs that treat `std::string` as
  /// UTF-8, such as logging, JSON, config files, and many C APIs.
  ///
  /// No validation is performed.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto bytes = ixx::text::FromU8(u8"ready");
  /// ```
  export [[nodiscard]] inline auto FromU8(const std::u8string_view text) -> std::string
  {
    std::string result;
    result.resize(text.size());

    std::ranges::transform(text, result.begin(), [](const char8_t c) {
      return static_cast<char>(c);
    });

    return result;
  }

  /// Copies UTF-8 code units from `std::string_view` into `std::u8string`.
  ///
  /// This is a byte-preserving bridge. It assumes the input is already UTF-8.
  /// Use `ValidateUtf8` first when the input may come from an untrusted source.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto u8 = ixx::text::ToU8("ready");
  /// ```
  export [[nodiscard]] inline auto ToU8(const std::string_view text) -> std::u8string
  {
    std::u8string result;
    result.resize(text.size());

    std::ranges::transform(text, result.begin(), [](const unsigned char c) {
      return static_cast<char8_t>(c);
    });

    return result;
  }

#ifdef _WIN32

  /// Converts UTF-8 text to Windows wide text.
  ///
  /// On Windows, `std::wstring` / `wchar_t` is used here as UTF-16 text for
  /// calling Win32 `...W` APIs.
  ///
  /// This function is strict:
  ///
  /// - invalid UTF-8 fails;
  /// - no replacement characters are inserted;
  /// - failures are returned as `Result<std::wstring>`.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto wide = ixx::text::Utf8ToWide("C:/tmp/файл.txt");
  /// ```
  export [[nodiscard]] inline auto Utf8ToWide(
    const std::string_view     utf8,
    const std::source_location where = std::source_location::current()) -> Result<std::wstring>
  {
    if (utf8.empty())
    {
      return std::wstring{};
    }

    if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
      return std::unexpected{
        Internal::MakeTextError(
          TextErrc::InputTooLarge,
          "UTF-8 input is too large for MultiByteToWideChar",
          "ixx::text::Utf8ToWide",
          where),
      };
    }

    const int inputSize = static_cast<int>(utf8.size());

    ::SetLastError(ERROR_SUCCESS);

    const int required = ::MultiByteToWideChar(
      CP_UTF8,
      MB_ERR_INVALID_CHARS,
      utf8.data(),
      inputSize,
      nullptr,
      0);

    if (required == 0)
    {
      const DWORD win32Error = ::GetLastError();
      const TextErrc outerCode = Internal::ClassifyWin32Utf8ToWideError(win32Error);

      return std::unexpected{
        Internal::MakeWin32TextError(
          outerCode,
          "failed to calculate UTF-16 length from UTF-8 input",
          "ixx::text::Utf8ToWide",
          win32Error,
          where),
      };
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');

    ::SetLastError(ERROR_SUCCESS);

    const int written = ::MultiByteToWideChar(
      CP_UTF8,
      MB_ERR_INVALID_CHARS,
      utf8.data(),
      inputSize,
      result.data(),
      required);

    if (written == 0)
    {
      const DWORD win32Error = ::GetLastError();
      const TextErrc outerCode = Internal::ClassifyWin32Utf8ToWideError(win32Error);

      return std::unexpected{
        Internal::MakeWin32TextError(
          outerCode,
          "failed to convert UTF-8 input to UTF-16",
          "ixx::text::Utf8ToWide",
          win32Error,
          where),
      };
    }

    result.resize(static_cast<std::size_t>(written));
    return result;
  }

  /// Converts UTF-8 text to Windows wide text.
  ///
  /// Overload for `std::u8string_view`.
  export [[nodiscard]] inline auto Utf8ToWide(
    const std::u8string_view   utf8,
    const std::source_location where = std::source_location::current()) -> Result<std::wstring>
  {
    return Utf8ToWide(FromU8(utf8), where);
  }

  /// Converts Windows wide text to UTF-8.
  ///
  /// On Windows, this treats `std::wstring_view` as UTF-16 text.
  ///
  /// This function is strict:
  ///
  /// - invalid UTF-16 fails;
  /// - unpaired surrogates fail;
  /// - no replacement characters are inserted;
  /// - failures are returned as `Result<std::string>`.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto utf8 = ixx::text::WideToUtf8(L"C:/tmp/file.txt");
  /// ```
  export [[nodiscard]] inline auto WideToUtf8(
    const std::wstring_view    wide,
    const std::source_location where = std::source_location::current()) -> Result<std::string>
  {
    if (wide.empty())
    {
      return std::string{};
    }

    if (wide.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
      return std::unexpected{
        Internal::MakeTextError(
          TextErrc::InputTooLarge,
          "UTF-16 input is too large for WideCharToMultiByte",
          "ixx::text::WideToUtf8",
          where),
      };
    }

    const int inputSize = static_cast<int>(wide.size());

    ::SetLastError(ERROR_SUCCESS);

    const int required = ::WideCharToMultiByte(
      CP_UTF8,
      WC_ERR_INVALID_CHARS,
      wide.data(),
      inputSize,
      nullptr,
      0,
      nullptr,
      nullptr);

    if (required == 0)
    {
      const DWORD win32Error = ::GetLastError();
      const TextErrc outerCode = Internal::ClassifyWin32WideToUtf8Error(win32Error);

      return std::unexpected{
        Internal::MakeWin32TextError(
          outerCode,
          "failed to calculate UTF-8 length from UTF-16 input",
          "ixx::text::WideToUtf8",
          win32Error,
          where),
      };
    }

    std::string result(static_cast<std::size_t>(required), '\0');

    ::SetLastError(ERROR_SUCCESS);

    const int written = ::WideCharToMultiByte(
      CP_UTF8,
      WC_ERR_INVALID_CHARS,
      wide.data(),
      inputSize,
      result.data(),
      required,
      nullptr,
      nullptr);

    if (written == 0)
    {
      const DWORD win32Error = ::GetLastError();
      const TextErrc outerCode = Internal::ClassifyWin32WideToUtf8Error(win32Error);

      return std::unexpected{
        Internal::MakeWin32TextError(
          outerCode,
          "failed to convert UTF-16 input to UTF-8",
          "ixx::text::WideToUtf8",
          win32Error,
          where),
      };
    }

    result.resize(static_cast<std::size_t>(written));
    return result;
  }

#endif

}
