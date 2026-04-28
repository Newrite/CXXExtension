export module CXXExtension.String;

import CXXExtension.Core;

import std;

namespace cxx {

    export namespace Internal
    {
        [[nodiscard]]
        constexpr auto IsAsciiSpace(char c) noexcept -> bool
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        [[nodiscard]]
        constexpr auto StripHexPrefix(std::string_view s) noexcept -> std::string_view
        {
            if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
                s.remove_prefix(2);

            return s;
        }

        [[nodiscard]]
        constexpr auto TrimAsciiView(std::string_view s) noexcept -> std::string_view
        {
            while (!s.empty() && IsAsciiSpace(s.front()))
                s.remove_prefix(1);

            while (!s.empty() && IsAsciiSpace(s.back()))
                s.remove_suffix(1);

            return s;
        }

        [[nodiscard]]
        constexpr auto ToLowerAscii(char c) noexcept -> char
        {
            if (c >= 'A' && c <= 'Z')
                return static_cast<char>(c - 'A' + 'a');

            return c;
        }

        [[nodiscard]]
        constexpr auto IEqualsAscii(std::string_view a, std::string_view b) noexcept -> bool
        {
            if (a.size() != b.size())
                return false;

            for (size_t i = 0; i < a.size(); ++i) {
                if (ToLowerAscii(a[i]) != ToLowerAscii(b[i]))
                    return false;
            }

            return true;
        }

        [[nodiscard]]
        inline auto MakeParseError(std::string_view function,
                                   std::string_view input,
                                   std::string_view reason) -> Error
        {
            std::string message;
            message += function;
            message += ": ";
            message += reason;
            message += " [input: `";
            message += input;
            message += "`]";

            return Error{std::move(message)};
        }

        template <class T>
        [[nodiscard]]
        auto ParseFailure(std::string_view function,
                          std::string_view input,
                          std::string_view reason) -> Result<T>
        {
            return std::unexpected(MakeParseError(function, input, reason));
        }
    }


    // String helpers
    export auto Trim(std::string_view str) -> std::string
    {
        return std::string{Internal::TrimAsciiView(str)};
    }

    export auto Split(std::string_view str, char delimiter) -> std::vector<std::string>
    {
        return str
            | std::views::split(delimiter)
            | std::ranges::to<std::vector<std::string>>();
    }

    export auto TrimLeft(std::string_view s) -> std::string
    {
        auto view = s | std::views::drop_while([](char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        });

        return std::string{view.begin(), view.end()};
    }

    export auto ToUpper(std::string str) -> std::string
    {
        std::ranges::transform(str, str.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });

        return str;
    }

    export auto ToLower(std::string str) -> std::string
    {
        std::ranges::transform(str, str.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        return str;
    }

    export template <std::ranges::input_range R>
        requires std::constructible_from<std::string_view, std::ranges::range_reference_t<R>>
    auto Join(R&& parts, std::string_view separator) -> std::string
    {
        std::string result;
        bool first = true;

        for (auto&& part : parts) {
            if (!first)
                result += separator;

            first = false;
            result += std::string_view{part};
        }

        return result;
    }

    export auto ReplaceAll(std::string_view source,
                           std::string_view from,
                           std::string_view to) -> std::string
    {
        if (from.empty()) return "";

        std::string result;
        size_t pos = 0;

        while (true) {
            const size_t next = source.find(from, pos);

            if (next == std::string_view::npos) {
                result += source.substr(pos);
                break;
            }

            result += source.substr(pos, next - pos);
            result += to;
            pos = next + from.size();
        }

        return result;
    }

}