export module CXXExtension.Container;

import CXXExtension.Core;
import CXXExtension.ContainerExtension;

import std;

namespace cxx {
    export template<typename Message>
    class Mailbox {
    public:
        explicit Mailbox() = default;

        template<class M>
            requires std::constructible_from<Message, M&&>
        auto Push(M&& message) -> void
        {
            messages.emplace_back(std::forward<M>(message));
        }

        auto Receive() -> std::optional<Message>
        {
            if (messages.empty()) {
                return std::nullopt;
            }

            std::optional<Message> result{ std::in_place, std::move(messages.front()) };
            messages.pop_front();
            return result;
        }

        template<std::ranges::input_range R>
            requires std::constructible_from<Message, std::ranges::range_reference_t<R>>
        auto Append(R&& range) -> void
        {
            for (auto&& message : range) {
                messages.emplace_back(std::forward<decltype(message)>(message));
            }
        }

        template<std::ranges::input_range R>
            requires std::constructible_from<Message, std::ranges::range_rvalue_reference_t<R>>
        auto AppendMove(R&& range) -> void
        {
            auto it = std::ranges::begin(range);
            auto last = std::ranges::end(range);

            for (; it != last; ++it) {
                messages.emplace_back(std::ranges::iter_move(it));
            }
        }

        auto HasMessage() const -> bool
        {
            return !messages.empty();
        }

        auto MessageCount() const -> std::size_t
        {
            return messages.size();
        }

    protected:
        template<class M>
            requires std::constructible_from<Message, M&&>
        auto PushFront(M&& message) -> void
        {
            messages.emplace_front(std::forward<M>(message));
        }

    private:
        std::deque<Message> messages{};
    };

    export template<typename Message>
    class StashBox : public Mailbox<Message> {
    public:
        explicit StashBox() = default;

        template<class M>
            requires std::constructible_from<Message, M&&>
        auto Stash(M&& message) -> void
        {
            stash.emplace_back(std::forward<M>(message));
        }

        auto UnStashAll() -> void
        {
            while (!stash.empty()) {
                this->PushFront(std::move(stash.back()));
                stash.pop_back();
            }
        }

        auto HasStash() const -> bool
        {
            return !stash.empty();
        }

        auto StashSize() const -> std::size_t
        {
            return stash.size();
        }

    private:
        std::vector<Message> stash{};
    };
}
