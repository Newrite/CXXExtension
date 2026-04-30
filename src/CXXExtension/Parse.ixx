export module CXXExtension.Parse;

import CXXExtension.Core;
import CXXExtension.String;

import std;

namespace cxx {
    export template<class T>
    concept ParseInteger =
            std::integral<T> &&
            !std::same_as<std::remove_cv_t<T>, bool>;

    export template<ParseInteger T>
    [[nodiscard]]
    auto ParseIntegral(std::string_view input, int base = 10) -> Result<T> {
        const auto original = input;
        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return Internal::ParseFailure<T>("ParseIntegral", original, "empty input");

        if (base < 2 || base > 36)
            return Internal::ParseFailure<T>("ParseIntegral", original, "base must be in [2, 36]");

        T value{};

        const char *first = input.data();
        const char *last = input.data() + input.size();

        const auto [ptr, ec] = std::from_chars(first, last, value, base);

        if (ec == std::errc::invalid_argument)
            return Internal::ParseFailure<T>("ParseIntegral", original, "not an integer");

        if (ec == std::errc::result_out_of_range)
            return Internal::ParseFailure<T>("ParseIntegral", original, "integer out of range");

        if (ec != std::errc{})
            return Internal::ParseFailure<T>("ParseIntegral", original, "integer parse failed");

        if (ptr != last)
            return Internal::ParseFailure<T>("ParseIntegral", original, "trailing characters");

        return value;
    }

    export template<std::signed_integral T = int64_t>
    [[nodiscard]]
    auto ParseInt(std::string_view input, int base = 10) -> Result<T> {
        return ParseIntegral<T>(input, base);
    }

    export template<std::unsigned_integral T = uint64_t>
    [[nodiscard]]
    auto ParseUInt(std::string_view input, int base = 10) -> Result<T> {
        return ParseIntegral<T>(input, base);
    }

    export template<std::floating_point T>
    [[nodiscard]]
    auto ParseFloating(std::string_view input,
                       std::chars_format format = std::chars_format::general) -> Result<T> {
        const auto original = input;
        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return Internal::ParseFailure<T>("ParseFloating", original, "empty input");

        T value{};

        const char *first = input.data();
        const char *last = input.data() + input.size();

        const auto [ptr, ec] = std::from_chars(first, last, value, format);

        if (ec == std::errc::invalid_argument)
            return Internal::ParseFailure<T>("ParseFloating", original, "not a floating-point number");

        if (ec == std::errc::result_out_of_range)
            return Internal::ParseFailure<T>("ParseFloating", original, "floating-point value out of range");

        if (ec != std::errc{})
            return Internal::ParseFailure<T>("ParseFloating", original, "floating-point parse failed");

        if (ptr != last)
            return Internal::ParseFailure<T>("ParseFloating", original, "trailing characters");

        return value;
    }

    export [[nodiscard]]
    inline auto ParseFloat(std::string_view input) -> Result<float> {
        return ParseFloating<float>(input);
    }

    export [[nodiscard]]
    inline auto ParseDouble(std::string_view input) -> Result<double> {
        return ParseFloating<double>(input);
    }

    export [[nodiscard]]
    inline auto ParseLongDouble(std::string_view input) -> Result<long double> {
        return ParseFloating<long double>(input);
    }

    export template<class T>
        requires ParseInteger<T> || std::floating_point<T>
    [[nodiscard]]
    auto ParseNumber(std::string_view input) -> Result<T> {
        if constexpr (ParseInteger<T>) {
            return ParseIntegral<T>(input);
        } else {
            return ParseFloating<T>(input);
        }
    }

    export template<ParseInteger T>
    [[nodiscard]]
    auto ParseHex(std::string_view input) -> Result<T> {
        const auto original = input;

        input = Internal::TrimAsciiView(input);

        if constexpr (std::signed_integral<T>) {
            bool negative = false;

            if (!input.empty() && input.front() == '-') {
                negative = true;
                input.remove_prefix(1);
            }

            input = Internal::StripHexPrefix(input);

            if (input.empty())
                return Internal::ParseFailure<T>("ParseHex", original, "empty hex value");

            if (negative) {
                std::string normalized;
                normalized.reserve(input.size() + 1);
                normalized += '-';
                normalized += input;

                return ParseIntegral<T>(normalized, 16);
            }

            return ParseIntegral<T>(input, 16);
        } else {
            if (!input.empty() && input.front() == '-')
                return Internal::ParseFailure<T>("ParseHex", original, "negative value for unsigned integer");

            if (!input.empty() && input.front() == '+')
                input.remove_prefix(1);

            input = Internal::StripHexPrefix(input);

            if (input.empty())
                return Internal::ParseFailure<T>("ParseHex", original, "empty hex value");

            return ParseIntegral<T>(input, 16);
        }
    }

    export [[nodiscard]]
    inline auto HexToInt64(std::string_view input) -> Result<int64_t> {
        return ParseHex<int64_t>(input);
    }

    export [[nodiscard]]
    inline auto HexToUInt32(std::string_view input) -> Result<uint32_t> {
        return ParseHex<uint32_t>(input);
    }

    export enum class BoolParseMode {
        Strict, // true / false
        Relaxed // true / false / 1 / 0 / yes / no / on / off
    };

    export [[nodiscard]]
    inline auto ParseBool(std::string_view input,
                          BoolParseMode mode = BoolParseMode::Relaxed) -> Result<bool> {
        const auto original = input;
        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return Internal::ParseFailure<bool>("ParseBool", original, "empty input");

        if (Internal::IEqualsAscii(input, "true"))
            return true;

        if (Internal::IEqualsAscii(input, "false"))
            return false;

        if (mode == BoolParseMode::Strict)
            return Internal::ParseFailure<bool>("ParseBool", original, "expected `true` or `false`");

        if (input == "1" ||
            Internal::IEqualsAscii(input, "yes") ||
            Internal::IEqualsAscii(input, "on") ||
            Internal::IEqualsAscii(input, "enabled"))
            return true;

        if (input == "0" ||
            Internal::IEqualsAscii(input, "no") ||
            Internal::IEqualsAscii(input, "off") ||
            Internal::IEqualsAscii(input, "disabled"))
            return false;

        return Internal::ParseFailure<bool>("ParseBool", original, "not a boolean");
    }

    export template<class T>
        requires ParseInteger<T> || std::floating_point<T> || std::same_as<T, bool>
    [[nodiscard]]
    auto Parse(std::string_view input) -> Result<T> {
        if constexpr (std::same_as<T, bool>) {
            return ParseBool(input);
        } else if constexpr (ParseInteger<T>) {
            return ParseIntegral<T>(input);
        } else {
            return ParseFloating<T>(input);
        }
    }

    export enum class CaseMode {
        Sensitive,
        InsensitiveAscii
    };

    export template<class E>
        requires std::is_enum_v<E>
    [[nodiscard]]
    auto ParseEnum(std::string_view input,
                   std::initializer_list<std::pair<std::string_view, E> > values,
                   CaseMode caseMode = CaseMode::InsensitiveAscii) -> Result<E> {
        const auto original = input;
        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return Internal::ParseFailure<E>("ParseEnum", original, "empty input");

        for (const auto &[name, value]: values) {
            const bool match =
                    caseMode == CaseMode::Sensitive
                        ? input == name
                        : Internal::IEqualsAscii(input, name);

            if (match)
                return value;
        }

        return Internal::ParseFailure<E>("ParseEnum", original, "unknown enum value");
    }

    export template<class T>
        requires ParseInteger<T> || std::floating_point<T> || std::same_as<T, bool>
    [[nodiscard]]
    auto ParseOptional(std::string_view input) -> Result<std::optional<T> > {
        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return std::optional<T>{};

        auto parsed = Parse<T>(input);

        if (!parsed)
            return std::unexpected(parsed.error());

        return std::optional<T>{std::move(*parsed)};
    }

    export template<class T>
        requires ParseInteger<T> || std::floating_point<T> || std::same_as<T, bool>
    [[nodiscard]]
    auto ParseList(std::string_view input, char delimiter = ',') -> Result<std::vector<T> > {
        std::vector<T> result;

        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return result;

        size_t pos = 0;

        while (true) {
            const size_t next = input.find(delimiter, pos);

            const std::string_view part =
                    next == std::string_view::npos
                        ? input.substr(pos)
                        : input.substr(pos, next - pos);

            auto parsed = Parse<T>(part);

            if (!parsed)
                return std::unexpected(parsed.error());

            result.emplace_back(std::move(*parsed));

            if (next == std::string_view::npos)
                break;

            pos = next + 1;
        }

        return result;
    }
}
