/// Actor-oriented container primitives and a lightweight actor runtime.
///
/// This module exports owner-local FIFO mailboxes, actor-local inboxes with
/// stashing, and a lightweight actor abstraction. Actors accept messages from
/// external threads through `Post`. They can either be pumped manually with
/// `Update` or run autonomously after `Start`.
export module CXXExtension.Container;

import CXXExtension.Core;
import CXXExtension.ContainerExtension;

import std;

namespace cxx
{

  /// Thread-safe producer buffer drained by a single owner.
  ///
  /// Many producers may call `Push` concurrently. The actor or owner calls
  /// `Drain` to take the current batch of pending messages and clear the
  /// buffer.
  ///
  /// This is not a full blocking queue and it does not own or start a worker
  /// thread. It can be closed so later pushes are rejected. `Empty` and `Size`
  /// are synchronized snapshots intended mostly for diagnostics.
  ///
  /// @tparam Message Message type stored by value in the buffer.
  template <typename Message>
  struct ThreadSafePushBuffer
  {
    /// Creates an empty push buffer.
    explicit ThreadSafePushBuffer() = default;

    /// Creates an empty push buffer with reserved storage.
    ///
    /// Reserving capacity can reduce allocations while producers are posting.
    ///
    /// @param initialReserve Initial vector capacity.
    explicit ThreadSafePushBuffer(std::size_t initialReserve)
    {
      messages.reserve(initialReserve);
    }

    /// Appends a message to the pending producer buffer.
    ///
    /// The message is constructed in place under the buffer mutex.
    /// If the buffer has been closed, no message is stored.
    ///
    /// ## Thread safety
    ///
    /// Safe to call concurrently from multiple producer threads.
    ///
    /// @tparam M Type used to construct `Message`.
    /// @param message Value copied or moved into the buffer.
    /// @return `true` when the message was accepted, or `false` after close.
    template <class M>
    requires std::constructible_from<Message, M&&>
    auto Push(M&& message) -> bool
    {
      std::lock_guard lock{mutex};

      if (closed)
      {
        return false;
      }

      messages.emplace_back(std::forward<M>(message));
      return true;
    }

    /// Moves all pending messages out as a batch.
    ///
    /// The returned vector contains messages in producer insertion order as
    /// observed by the mutex. The internal buffer is empty after this call.
    ///
    /// ## Thread safety
    ///
    /// Safe to call while producers call `Push`, but intended to have a single
    /// draining owner.
    ///
    /// @return Vector containing the drained messages.
    auto Drain() -> std::vector<Message>
    {
      std::vector<Message> result;

      {
        std::lock_guard lock{mutex};
        messages.swap(result);
      }

      return result;
    }

    /// Closes the buffer and rejects future pushes.
    ///
    /// Already buffered messages remain available to `Drain`.
    auto Close() -> void
    {
      std::lock_guard lock{mutex};
      closed = true;
    }

    /// Returns whether the buffer is closed.
    ///
    /// This is a synchronized snapshot.
    [[nodiscard]] auto IsClosed() const -> bool
    {
      std::lock_guard lock{mutex};
      return closed;
    }

    /// Reserves storage for future pending messages.
    ///
    /// This does not reopen a closed buffer and does not affect already stored
    /// messages.
    ///
    /// @param capacity Desired vector capacity.
    auto Reserve(std::size_t capacity) -> void
    {
      std::lock_guard lock{mutex};
      messages.reserve(capacity);
    }

    /// Returns whether the buffer was empty at the moment it was inspected.
    ///
    /// The result is a synchronized snapshot and may be stale immediately
    /// after the call returns.
    [[nodiscard]] auto Empty() const -> bool
    {
      std::lock_guard lock{mutex};
      return messages.empty();
    }

    /// Returns the number of pending messages at the moment it was inspected.
    ///
    /// The result is a synchronized snapshot and should not be used for
    /// correctness decisions that depend on future producer activity.
    [[nodiscard]] auto Size() const -> std::size_t
    {
      std::lock_guard lock{mutex};
      return messages.size();
    }

private:

    mutable std::mutex   mutex{};
    std::vector<Message> messages{};
    bool                 closed{false};
  };

  /// Small owner-local FIFO mailbox.
  ///
  /// `Mailbox` stores messages by value and receives from the front. It is
  /// useful when one owner controls all access and wants explicit FIFO message
  /// handling without synchronization overhead.
  ///
  /// ## Thread safety
  ///
  /// `Mailbox` is not thread-safe. Use it directly only when all access is
  /// externally synchronized or owner-local.
  ///
  /// ## Move semantics
  ///
  /// `Receive` moves the front message out and removes it from the mailbox.
  /// `AppendMove` moves values from a range using iterator move semantics.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// cxx::Mailbox<std::string> mailbox;
  ///
  /// mailbox.Push("hello");
  /// mailbox.Push("world");
  ///
  /// while (auto message = mailbox.Receive()) {
  ///   // use *message
  /// }
  /// ```
  ///
  /// @tparam Message Message type stored by value.
  export template <typename Message>
  struct Mailbox
  {
    /// Creates an empty mailbox.
    explicit Mailbox() = default;

    /// Pushes a message to the back of the mailbox.
    ///
    /// @param message Value copied or moved into the mailbox.
    template <class M>
    requires std::constructible_from<Message, M&&>
    auto Push(M&& message) -> void
    {
      messages.emplace_back(std::forward<M>(message));
    }

    /// Pushes a message to the front of the mailbox.
    ///
    /// This is useful for restoring messages so they are processed before
    /// currently queued messages.
    ///
    /// @param message Value copied or moved into the mailbox.
    template <class M>
    requires std::constructible_from<Message, M&&>
    auto PushFront(M&& message) -> void
    {
      messages.emplace_front(std::forward<M>(message));
    }

    /// Receives and removes the next message.
    ///
    /// @return The front message, or `std::nullopt` when the mailbox is empty.
    auto Receive() -> std::optional<Message>
    {
      if (messages.empty())
      {
        return std::nullopt;
      }

      std::optional<Message> result{std::in_place, std::move(messages.front())};

      messages.pop_front();
      return result;
    }

    /// Appends all values from a range to the back of the mailbox.
    ///
    /// Values are forwarded from the range reference type. Order is preserved.
    ///
    /// @tparam R Input range whose references can construct `Message`.
    /// @param range Range of messages or message-like values.
    template <std::ranges::input_range R>
    requires std::constructible_from<Message, std::ranges::range_reference_t<R>>
    auto Append(R&& range) -> void
    {
      for (auto&& message : range)
      {
        messages.emplace_back(std::forward<decltype(message)>(message));
      }
    }

    /// Moves all values from a range to the back of the mailbox.
    ///
    /// The function uses `std::ranges::iter_move`, so it is appropriate for
    /// draining temporary buffers while preserving iteration order.
    ///
    /// @tparam R Input range whose rvalue references can construct `Message`.
    /// @param range Range to move from.
    template <std::ranges::input_range R>
    requires std::constructible_from<Message, std::ranges::range_rvalue_reference_t<R>>
    auto AppendMove(R&& range) -> void
    {
      auto it   = std::ranges::begin(range);
      auto last = std::ranges::end(range);

      for (; it != last; ++it)
      {
        messages.emplace_back(std::ranges::iter_move(it));
      }
    }

    /// Returns whether the mailbox currently has at least one message.
    ///
    /// This is an owner-local snapshot.
    [[nodiscard]] auto HasMessage() const -> bool
    {
      return !messages.empty();
    }

    /// Returns the current number of queued messages.
    ///
    /// This is an owner-local snapshot.
    [[nodiscard]] auto MessageCount() const -> std::size_t
    {
      return messages.size();
    }

private:

    std::deque<Message> messages{};
  };

  /// Actor-local inbox with FIFO messages and a stash buffer.
  ///
  /// `Inbox` combines a `Mailbox<Message>` with a stash used to defer messages
  /// until actor state changes. It is designed to be owned by an actor update
  /// loop.
  ///
  /// ## Thread safety
  ///
  /// `Inbox` is not thread-safe. It should be accessed only by its actor or
  /// owner.
  ///
  /// ## Stash order
  ///
  /// `UnstashAll` restores stashed messages to the front of the mailbox while
  /// preserving stash order:
  ///
  /// ```text
  /// mailbox: X Y
  /// stash:   A B C
  ///
  /// UnstashAll()
  ///
  /// mailbox: A B C X Y
  /// stash:   empty
  /// ```
  ///
  /// ## Example
  ///
  /// ```cpp
  /// cxx::Inbox<std::string> inbox;
  ///
  /// inbox.Push("already queued");
  /// inbox.Stash("wait until ready");
  ///
  /// inbox.UnstashAll();
  ///
  /// auto first = inbox.Receive(); // "wait until ready"
  /// ```
  ///
  /// @tparam Message Message type stored by value.
  export template <typename Message>
  struct Inbox
  {
    /// Creates an empty inbox.
    explicit Inbox() = default;

    /// Pushes a message to the back of the inbox mailbox.
    template <class M>
    requires std::constructible_from<Message, M&&>
    auto Push(M&& message) -> void
    {
      mailbox.Push(std::forward<M>(message));
    }

    /// Receives and removes the next mailbox message.
    ///
    /// Stashed messages are not received until `UnstashAll` restores them.
    ///
    /// @return The front message, or `std::nullopt` when no message is queued.
    auto Receive() -> std::optional<Message>
    {
      return mailbox.Receive();
    }

    /// Appends values from a range to the back of the inbox mailbox.
    template <std::ranges::input_range R>
    requires std::constructible_from<Message, std::ranges::range_reference_t<R>>
    auto Append(R&& range) -> void
    {
      mailbox.Append(std::forward<R>(range));
    }

    /// Moves values from a range to the back of the inbox mailbox.
    template <std::ranges::input_range R>
    requires std::constructible_from<Message, std::ranges::range_rvalue_reference_t<R>>
    auto AppendMove(R&& range) -> void
    {
      mailbox.AppendMove(std::forward<R>(range));
    }

    /// Saves a message for later processing.
    ///
    /// Stashed messages are actor-local and become visible to `Receive` only
    /// after `UnstashAll` is called.
    template <class M>
    requires std::constructible_from<Message, M&&>
    auto Stash(M&& message) -> void
    {
      stash.emplace_back(std::forward<M>(message));
    }

    /// Restores all stashed messages to the front of the mailbox.
    ///
    /// Stash order is preserved, and restored messages run before messages that
    /// were already queued.
    auto UnstashAll() -> void
    {
      while (!stash.empty())
      {
        mailbox.PushFront(std::move(stash.back()));
        stash.pop_back();
      }
    }

    /// Returns whether any messages are currently stashed.
    [[nodiscard]] auto HasStash() const -> bool
    {
      return !stash.empty();
    }

    /// Returns the number of currently stashed messages.
    [[nodiscard]] auto StashCount() const -> std::size_t
    {
      return stash.size();
    }

    /// Returns whether the mailbox has at least one receivable message.
    [[nodiscard]] auto HasMessage() const -> bool
    {
      return mailbox.HasMessage();
    }

    /// Returns the number of receivable mailbox messages.
    [[nodiscard]] auto MessageCount() const -> std::size_t
    {
      return mailbox.MessageCount();
    }

private:

    Mailbox<Message>     mailbox{};
    std::vector<Message> stash{};
  };

  namespace actor
  {

    /// Actor that serializes message handling through manual or autonomous updates.
    export template <typename Message, typename ActorState, typename Handler>
    class Actor;

    /// Restricted control surface passed to an actor handler.
    ///
    /// `Context` exists only while one message is being handled. It allows the
    /// handler to mutate state, unstash deferred messages, or request that the
    /// current message be stashed without exposing the actor internals.
    ///
    /// ## Lifetime
    ///
    /// A context must not be stored, moved, copied, or used after the handler
    /// returns.
    ///
    /// ## Handler contract
    ///
    /// Actor handlers are invoked as:
    ///
    /// ```cpp
    /// handler(ctx, state, message);
    /// ```
    ///
    /// where `ctx` is `cxx::actor::Context<Message, ActorState>&`, `state` is
    /// `ActorState&`, and `message` is `Message&`.
    ///
    /// ## Thread safety
    ///
    /// Context methods are actor-thread or owner-thread only. They must be used
    /// from the active handler call.
    ///
    /// @tparam Message Actor message type.
    /// @tparam ActorState Actor state type.
    export template <typename Message, typename ActorState>
    class Context
    {
  public:

      Context(const Context&)                    = delete;
      auto operator=(const Context&) -> Context& = delete;
      Context(Context&&)                         = delete;
      auto operator=(Context&&) -> Context&      = delete;

      /// Mutates the actor state and restores all stashed messages.
      ///
      /// The callable is invoked immediately with `ActorState&`. After it
      /// returns, `UnstashAll` is called.
      ///
      /// @param mutateState Callable compatible with `void(ActorState&)`.
      template <class F>
      requires std::invocable<F&&, ActorState&>
      auto Become(F&& mutateState) -> void
      {
        std::invoke(std::forward<F>(mutateState), state);
        inbox.UnstashAll();
      }

      /// Replaces the actor state and restores all stashed messages.
      ///
      /// @param newState New state moved into the actor.
      auto Become(ActorState newState) -> void
      {
        state = std::move(newState);
        inbox.UnstashAll();
      }

      /// Restores all stashed messages without changing state.
      auto UnstashAll() -> void
      {
        inbox.UnstashAll();
      }

      /// Requests that the current message be stashed after the handler returns.
      ///
      /// The current message is not moved into the stash immediately. The actor
      /// checks this request after the handler finishes and then stashes the
      /// message value that was passed to the handler.
      auto StashCurrent() -> void
      {
        stashCurrent = true;
      }

  private:

      template <typename, typename, typename>
      friend class Actor;

      explicit Context(ActorState& state, Inbox<Message>& inbox) : state{state}, inbox{inbox} {}

      /// Returns whether the handler requested current-message stashing.
      [[nodiscard]] auto ShouldStashCurrent() const -> bool
      {
        return stashCurrent;
      }

      ActorState&     state;
      Inbox<Message>& inbox;
      bool            stashCurrent{};
    };

    template <typename Message, typename ActorState, typename Handler>
    class Actor
    {
  public:

      /// Context type passed to this actor's handler.
      using ContextType = Context<Message, ActorState>;

      /// Creates an actor with initial state and a handler.
      ///
      /// The actor does not start a thread until `Start` is called. Without
      /// `Start`, it processes messages when the owner calls `Update`.
      ///
      /// @param initialState Initial actor state, moved into the actor.
      /// @param handler Callable invoked for each message.
      explicit Actor(ActorState initialState, Handler handler) : state{std::move(initialState)}, handler{std::move(handler)} {}

      /// Stops the autonomous worker, if one is running.
      ///
      /// Destruction calls `Stop`. Pending messages that have not been processed
      /// before the worker exits are discarded with the actor object.
      ~Actor()
      {
        Stop();
      }

      /// Posts a message from any producer thread.
      ///
      /// The message is appended to the synchronized incoming buffer and will be
      /// processed during a later `Update` call or by the autonomous worker.
      /// Messages posted after `Stop` are rejected by the incoming buffer.
      ///
      /// ## Thread safety
      ///
      /// `Post` may be called concurrently by multiple external threads.
      ///
      /// @param message Value copied or moved into the actor's incoming buffer.
      template <class M>
      requires std::constructible_from<Message, M&&>
      auto Post(M&& message) -> void
      {
        if (incoming.Push(std::forward<M>(message)))
        {
          wakeCv.notify_all();
        }
      }

      /// Processes all currently posted messages and any unstashed work.
      ///
      /// `Update` drains the incoming producer buffer into the actor-local inbox,
      /// then repeatedly receives messages and invokes the handler.
      ///
      /// ## Manual update model
      ///
      /// Before `Start` is called, the caller decides when processing happens by
      /// calling `Update`, commonly from a game tick, event loop, or service
      /// pump.
      ///
      /// ## Autonomous mode
      ///
      /// After `Start` succeeds, a worker thread owns update processing. Do not
      /// call `Update` concurrently with that worker.
      ///
      /// ## Handler contract
      ///
      /// The handler is invoked as:
      ///
      /// ```cpp
      /// handler(ctx, state, message);
      /// ```
      ///
      /// `ctx`, `state`, the inbox, the stash, and the handler are actor-local
      /// during this call.
      ///
      /// ## Stash behavior
      ///
      /// If the handler calls `ctx.StashCurrent()`, the actor moves the current
      /// message into the stash after the handler returns. Calling
      /// `ctx.Become(...)` mutates or replaces state and then unstashes messages.
      ///
      /// ## Thread safety
      ///
      /// `Update` is owner-thread only. Do not call it concurrently with another
      /// `Update` or after `Start` has handed processing to the worker.
      ///
      /// ## Example
      ///
      /// ```cpp
      /// struct Connected {};
      /// struct SendChat { std::string text; };
      ///
      /// using Message = std::variant<Connected, SendChat>;
      ///
      /// struct State { bool connected{}; };
      ///
      /// struct Handler {
      ///   auto operator()(
      ///     cxx::actor::Context<Message, State>& ctx,
      ///     State& state,
      ///     Message& message
      ///   ) -> void {
      ///     std::visit(Visitor{ctx, state}, message);
      ///   }
      ///
      ///   struct Visitor {
      ///     cxx::actor::Context<Message, State>& ctx;
      ///     State& state;
      ///
      ///     auto operator()(Connected&) -> void {
      ///       ctx.Become([](State& s) { s.connected = true; });
      ///     }
      ///
      ///     auto operator()(SendChat& message) -> void {
      ///       if (!state.connected) {
      ///         ctx.StashCurrent();
      ///         return;
      ///       }
      ///
      ///       // SendPacket(message.text);
      ///     }
      ///   };
      /// };
      ///
      /// auto actor = cxx::actor::Make<Message>(State{}, Handler{});
      /// actor.Post(Message{SendChat{"hello before connect"}});
      /// actor.Update();
      /// actor.Post(Message{Connected{}});
      /// actor.Update();
      /// ```
      auto Update() -> void
      {
        if (started.load() || IsStopped()) return;
        UpdateImpl();
      }

      /// Returns the number of messages waiting in the synchronized incoming buffer.
      ///
      /// This is a synchronized snapshot. Messages already drained into the
      /// actor-local inbox or stash are not counted.
      [[nodiscard]] auto IncomingCount() const -> std::size_t
      {
        return incoming.Size();
      }

      /// Requests actor shutdown and joins the worker when needed.
      ///
      /// `Stop` is idempotent. It closes the incoming buffer so later `Post`
      /// calls are ignored, requests the autonomous worker to stop, wakes the
      /// worker, and joins it when called from another thread.
      ///
      /// ## Thread safety
      ///
      /// `Stop` may be called concurrently with `Post`. Do not call it from a
      /// handler if the handler must guarantee that remaining queued messages are
      /// processed.
      auto Stop() -> void
      {
        const auto wasStopped = stopRequested.exchange(true);

        if (!wasStopped)
        {
          incoming.Close();
          wakeCv.notify_all();
        }

        if (worker.joinable() && worker.get_id() != std::this_thread::get_id())
        {
          worker.request_stop();
          wakeCv.notify_all();
          worker.join();
        }
      }

      /// Returns whether shutdown has been requested.
      ///
      /// The result is an atomic snapshot.
      [[nodiscard]] auto IsStopped() const -> bool
      {
        return stopRequested.load();
      }

      /// Starts autonomous actor processing on a worker thread.
      ///
      /// The worker repeatedly drains pending messages, processes the actor-local
      /// inbox, and sleeps until a new post or stop request wakes it.
      ///
      /// ## Thread safety
      ///
      /// Call `Start` at most once. Concurrent `Post` is supported after a
      /// successful start.
      ///
      /// @return `true` when the worker was started, or `false` if the actor was
      /// already started or stopped.
      auto Start() -> bool
      {
        if (IsStopped()) return false;

        bool expected = false;

        if (!started.compare_exchange_strong(expected, true))
        {
          return false;
        }

        worker = std::jthread{[this](std::stop_token stopToken) { Run(stopToken); }};

        return true;
      }

  private:

      auto UpdateImpl() -> void
      {
        auto batch = incoming.Drain();
        inbox.AppendMove(batch);

        while (auto message = inbox.Receive())
        {
          auto        msg = std::move(*message);
          ContextType context{state, inbox};

          std::invoke(handler, context, state, msg);

          if (context.ShouldStashCurrent())
          {
            inbox.Stash(std::move(msg));
          }
        }
      }

      auto Run(std::stop_token stopToken) -> void
      {
        while (!stopToken.stop_requested() && !IsStopped())
        {
          UpdateImpl();

          std::unique_lock lock{wakeMutex};

          wakeCv.wait(lock, [this, &stopToken] { return stopToken.stop_requested() || IsStopped() || !incoming.Empty(); });
        }
      }

      ThreadSafePushBuffer<Message> incoming{};
      Inbox<Message>                inbox{};

      ActorState state;
      Handler    handler;

      mutable std::mutex      wakeMutex{};
      std::condition_variable wakeCv{};
      std::atomic_bool        started{false};
      std::atomic_bool        stopRequested{false};
      std::jthread            worker{};
    };

    /// Creates an actor while deducing state and handler storage types.
    ///
    /// `Message` must be specified explicitly. `ActorState` and `Handler` are
    /// decayed before being stored in the returned actor.
    ///
    /// ## Example
    ///
    /// ```cpp
    /// auto actor = cxx::actor::Make<Message>(State{}, Handler{});
    /// actor.Post(Message{Connected{}});
    /// actor.Update();
    /// ```
    ///
    /// @tparam Message Actor message type.
    /// @param state Initial state copied or moved into the actor.
    /// @param handler Handler copied or moved into the actor.
    /// @return An actor that can be manually updated or started autonomously.
    export template <typename Message, typename ActorState, typename Handler>
    auto Make(ActorState&& state, Handler&& handler)
    {
      using State       = std::decay_t<ActorState>;
      using HandlerType = std::decay_t<Handler>;

      return Actor<Message, State, HandlerType>{std::forward<ActorState>(state), std::forward<Handler>(handler)};
    }

  }  // namespace actor

  /// Convenience alias for `cxx::actor::Actor`.
  ///
  /// Use this when the actor namespace is too verbose at a declaration site.
  export template <typename Message, typename ActorState, typename Handler>
  using Actor = actor::Actor<Message, ActorState, Handler>;

}  // namespace cxx
