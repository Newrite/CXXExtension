export module CXXExtension.Container;

import CXXExtension.Core;
import CXXExtension.ContainerExtension;

import std;

namespace cxx {

    namespace Internal {

        template<typename Message>
        struct ThreadSafePushBuffer {
            explicit ThreadSafePushBuffer() = default;

            auto Push(Message&& message) -> void
            {
                std::lock_guard lock(mutex);
                messages.emplace_back(std::forward<Message>(message));
            }

            auto Drain() -> std::vector<Message>
            {
                std::vector<Message> result;
                {
                    std::lock_guard lock(mutex);
                    std::ranges::swap(result, messages);
                }
                return result;
            }

            [[nodiscard]]
            auto Empty() const -> bool
            {
                std::lock_guard lock(mutex);
                return messages.empty();
            }

            [[nodiscard]]
            auto Size() const -> std::size_t
            {
                std::lock_guard lock(mutex);
                return messages.size();
            }

        private:
            mutable std::mutex mutex{};
            std::vector<Message> messages{};
        };

    }

    export template<typename Message>
    struct Mailbox {
        explicit Mailbox() = default;

        template<class M>
            requires std::constructible_from<Message, M&&>
        auto Push(M&& message) -> void
        {
            messages.emplace_back(std::forward<M>(message));
        }

        template<class M>
            requires std::constructible_from<Message, M&&>
        auto PushFront(M&& message) -> void
        {
            messages.emplace_front(std::forward<M>(message));
        }

        auto Receive() -> std::optional<Message>
        {
            if (messages.empty()) {
                return std::nullopt;
            }

            std::optional<Message> result{
                std::in_place,
                std::move(messages.front())
            };

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

    private:
        std::deque<Message> messages{};
    };

    export template<typename Message>
    struct Inbox {
        explicit Inbox() = default;

        template<class M>
            requires std::constructible_from<Message, M&&>
        auto Push(M&& message) -> void
        {
            mailbox.Push(std::forward<M>(message));
        }

        auto Receive() -> std::optional<Message>
        {
            return mailbox.Receive();
        }

        template<std::ranges::input_range R>
            requires std::constructible_from<Message, std::ranges::range_reference_t<R>>
        auto Append(R&& range) -> void
        {
            mailbox.Append(std::forward<R>(range));
        }

        template<std::ranges::input_range R>
            requires std::constructible_from<Message, std::ranges::range_rvalue_reference_t<R>>
        auto AppendMove(R&& range) -> void
        {
            mailbox.AppendMove(std::forward<R>(range));
        }

        template<class M>
            requires std::constructible_from<Message, M&&>
        auto Stash(M&& message) -> void
        {
            stash.emplace_back(std::forward<M>(message));
        }

        auto UnstashAll() -> void
        {
            while (!stash.empty()) {
                mailbox.PushFront(std::move(stash.back()));
                stash.pop_back();
            }
        }

        auto HasStash() const -> bool
        {
            return !stash.empty();
        }

        auto StashCount() const -> std::size_t
        {
            return stash.size();
        }

        auto HasMessage() const -> bool
        {
            return mailbox.HasMessage();
        }

        auto MessageCount() const -> std::size_t
        {
            return mailbox.MessageCount();
        }

    private:
        Mailbox<Message> mailbox{};
        std::vector<Message> stash{};
    };

    export template<typename Message, typename ActorState>
    struct Actor {
        using MessageHandle = std::function<bool(Message&)>;
        using ActorStateHandle = std::function<ActorState(const ActorState&)>;

        explicit Actor(MessageHandle&& handle, ActorState&& state)
            : handle(std::move(handle)), state(std::move(state)) {}

        template<class M>
            requires std::constructible_from<Message, M&&>
        auto Post(M&& message) -> void
        {
            queue.Push(std::forward<M>(message));
        }

        auto Update() -> void
        {
            while (auto message = inbox.Receive())
            {
                auto msg = message.value();
                if (!handle(msg);)
                {
                    inbox.Stash(std::move(msg);
                }
            }
        }

        auto Become(ActorStateHandle stateHandle) -> void
        {
            state = stateHandle(state);
        }

    private:
        Inbox<Message> inbox{};
        Internal::ThreadSafePushBuffer<Message> queue{}
        MessageHandle handle;
        ActorState state;
    };

}