export module CXXExtension.Core;

import std;

namespace cxx {
    export struct Error {
        std::string message;
    };

    export template<class T>
    using Result = std::expected<T, Error>;

    export using VoidResult = std::expected<void, Error>;


    export template<class F>
    class ScopeExit {
    public:
        template<class Fn>
            requires std::constructible_from<F, Fn>
        explicit ScopeExit(Fn &&fn)
            noexcept(std::is_nothrow_constructible_v<F, Fn>)
            : fn_(std::forward<Fn>(fn)) {
        }

        ScopeExit(ScopeExit &&other)
            noexcept(std::is_nothrow_move_constructible_v<F>)
            : fn_(std::move(other.fn_)),
              active_(std::exchange(other.active_, false)) {
        }

        ScopeExit(const ScopeExit &) = delete;

        auto operator=(const ScopeExit &) -> ScopeExit & = delete;

        auto operator=(ScopeExit &&) -> ScopeExit & = delete;

        ~ScopeExit() noexcept {
            if (active_)
                fn_();
        }

        auto Release() noexcept -> void {
            active_ = false;
        }

    private:
        F fn_;
        bool active_ = true;
    };

    template<class F>
    ScopeExit(F) -> ScopeExit<F>;


    // Overloaded for std::visit
    export template<class... Ts>
    struct Overloaded : Ts... {
        using Ts::operator()...;
    };

    template<class... Ts>
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
    export template<class E>
    inline constexpr bool EnableBitmaskOperators = false;

    export template<class E>
    concept BitmaskEnum =
            std::is_enum_v<E> && EnableBitmaskOperators<E>;

    export template<BitmaskEnum E>
    constexpr auto operator|(E lhs, E rhs) noexcept -> E {
        using U = std::underlying_type_t<E>;
        return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
    }

    export template<BitmaskEnum E>
    constexpr auto operator&(E lhs, E rhs) noexcept -> E {
        using U = std::underlying_type_t<E>;
        return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
    }

    export template<BitmaskEnum E>
    constexpr auto HasFlag(E value, E flag) noexcept -> bool {
        using U = std::underlying_type_t<E>;
        return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
    }


    export template<std::integral T>
    constexpr auto ToBigEndian(T value) noexcept -> T {
        if constexpr (std::endian::native == std::endian::big) {
            return value;
        } else {
            return std::byteswap(value);
        }
    }

    export template<std::integral T>
    constexpr auto FromBigEndian(T value) noexcept -> T {
        return ToBigEndian(value);
    }
}
