export module CXXExtension.Container;

import CXXExtension.Core;
import CXXExtension.ContainerExtension;

import std;

namespace cxx {
    namespace Internal {
        template<typename Message>
        struct ThreadSafePushBuffer {
            explicit ThreadSafePushBuffer() = default;

            template<class M>
                requires std::constructible_from<Message, M &&>
            auto Push(M &&message) -> void {
                std::lock_guard lock{mutex};
                messages.emplace_back(std::forward<M>(message));
            }

            auto Drain() -> std::vector<Message> {
                std::vector<Message> result;

                {
                    std::lock_guard lock{mutex};
                    messages.swap(result);
                }

                return result;
            }

            [[nodiscard]]
            auto Empty() const -> bool {
                std::lock_guard lock{mutex};
                return messages.empty();
            }

            [[nodiscard]]
            auto Size() const -> std::size_t {
                std::lock_guard lock{mutex};
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
            requires std::constructible_from<Message, M &&>
        auto Push(M &&message) -> void {
            messages.emplace_back(std::forward<M>(message));
        }

        template<class M>
            requires std::constructible_from<Message, M &&>
        auto PushFront(M &&message) -> void {
            messages.emplace_front(std::forward<M>(message));
        }

        auto Receive() -> std::optional<Message> {
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
            requires std::constructible_from<Message, std::ranges::range_reference_t<R> >
        auto Append(R &&range) -> void {
            for (auto &&message: range) {
                messages.emplace_back(std::forward<decltype(message)>(message));
            }
        }

        template<std::ranges::input_range R>
            requires std::constructible_from<Message, std::ranges::range_rvalue_reference_t<R> >
        auto AppendMove(R &&range) -> void {
            auto it = std::ranges::begin(range);
            auto last = std::ranges::end(range);

            for (; it != last; ++it) {
                messages.emplace_back(std::ranges::iter_move(it));
            }
        }

        [[nodiscard]]
        auto HasMessage() const -> bool {
            return !messages.empty();
        }

        [[nodiscard]]
        auto MessageCount() const -> std::size_t {
            return messages.size();
        }

    private:
        std::deque<Message> messages{};
    };

    export template<typename Message>
    struct Inbox {
        explicit Inbox() = default;

        template<class M>
            requires std::constructible_from<Message, M &&>
        auto Push(M &&message) -> void {
            mailbox.Push(std::forward<M>(message));
        }

        auto Receive() -> std::optional<Message> {
            return mailbox.Receive();
        }

        template<std::ranges::input_range R>
            requires std::constructible_from<Message, std::ranges::range_reference_t<R> >
        auto Append(R &&range) -> void {
            mailbox.Append(std::forward<R>(range));
        }

        template<std::ranges::input_range R>
            requires std::constructible_from<Message, std::ranges::range_rvalue_reference_t<R> >
        auto AppendMove(R &&range) -> void {
            mailbox.AppendMove(std::forward<R>(range));
        }

        template<class M>
            requires std::constructible_from<Message, M &&>
        auto Stash(M &&message) -> void {
            stash.emplace_back(std::forward<M>(message));
        }

        auto UnstashAll() -> void {
            while (!stash.empty()) {
                mailbox.PushFront(std::move(stash.back()));
                stash.pop_back();
            }
        }

        [[nodiscard]]
        auto HasStash() const -> bool {
            return !stash.empty();
        }

        [[nodiscard]]
        auto StashCount() const -> std::size_t {
            return stash.size();
        }

        [[nodiscard]]
        auto HasMessage() const -> bool {
            return mailbox.HasMessage();
        }

        [[nodiscard]]
        auto MessageCount() const -> std::size_t {
            return mailbox.MessageCount();
        }

    private:
        Mailbox<Message> mailbox{};
        std::vector<Message> stash{};
    };

    export namespace actor {
        template<typename Message, typename ActorState, typename Handler>
        class Actor;

        template<typename Message, typename ActorState>
        class Context {
        public:
            Context(const Context &) = delete;

            auto operator=(const Context &) -> Context & = delete;

            Context(Context &&) = delete;

            auto operator=(Context &&) -> Context & = delete;

            template<class F>
                requires std::invocable<F &&, ActorState &>
            auto Become(F &&mutateState) -> void {
                std::invoke(std::forward<F>(mutateState), state);
                inbox.UnstashAll();
            }

            auto Become(ActorState newState) -> void {
                state = std::move(newState);
                inbox.UnstashAll();
            }

            auto UnstashAll() -> void {
                inbox.UnstashAll();
            }

            auto StashCurrent() -> void {
                stashCurrent = true;
            }

        private:
            template<typename, typename, typename>
            friend class Actor;

            explicit Context(ActorState &state, Inbox<Message> &inbox)
                : state{state}, inbox{inbox} {}

            [[nodiscard]]
            auto ShouldStashCurrent() const -> bool {
                return stashCurrent;
            }

            ActorState &state;
            Inbox<Message> &inbox;
            bool stashCurrent{};
        };

        template<typename Message, typename ActorState, typename Handler>
        class Actor {
        public:
            using ContextType = Context<Message, ActorState>;

            explicit Actor(ActorState initialState, Handler handler)
                : state{std::move(initialState)},
                  handler{std::move(handler)} {
            }

            Actor(const Actor &) = delete;

            auto operator=(const Actor &) -> Actor & = delete;

            Actor(Actor &&) = delete;

            auto operator=(Actor &&) -> Actor & = delete;

            template<class M>
                requires std::constructible_from<Message, M &&>
            auto Post(M &&message) -> void {
                incoming.Push(std::forward<M>(message));
            }

            auto Update() -> void {
                auto batch = incoming.Drain();
                inbox.AppendMove(batch);

                while (auto message = inbox.Receive()) {
                    auto msg = std::move(*message);
                    ContextType context{state, inbox};

                    std::invoke(
                        handler,
                        context,
                        state,
                        msg
                    );

                    if (context.ShouldStashCurrent()) {
                        inbox.Stash(std::move(msg));
                    }
                }
            }

            [[nodiscard]]
            auto IncomingCount() const -> std::size_t {
                return incoming.Size();
            }

        private:
            Internal::ThreadSafePushBuffer<Message> incoming{};
            Inbox<Message> inbox{};

            ActorState state;
            Handler handler;
        };

        template<typename Message, typename ActorState, typename Handler>
        auto Make(ActorState &&state, Handler &&handler) {
            using State = std::decay_t<ActorState>;
            using HandlerType = std::decay_t<Handler>;

            return Actor<Message, State, HandlerType>{
                std::forward<ActorState>(state),
                std::forward<Handler>(handler)
            };
        }
    }

    export template<typename Message, typename ActorState, typename Handler>
    using Actor = actor::Actor<Message, ActorState, Handler>;
}
