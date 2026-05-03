/// Thread-safe buffers and actor-style concurrency primitives.
///
/// This module exports the synchronized producer buffer used by actors,
/// request/reply helpers, and a lightweight actor abstraction that can be pumped
/// manually or run on an owned worker thread.
export module CXXExtension.Concurrency;

import CXXExtension.Core;
import CXXExtension.Collections;
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

  namespace actor
  {

    /// Actor-specific error codes used by replies and stopped actors.
    export enum class Errc : std::uint16_t
    {
      /// No error.
      None = 0,
      /// The actor was stopped before a request could be accepted.
      Stopped,
      /// A reply handle was destroyed before it was resolved or rejected.
      ReplyAbandoned,
      /// A reply was resolved or rejected more than once.
      ReplyAlreadyCompleted,
      /// A reply future was consumed more than once.
      ReplyAlreadyTaken,
      /// A reply or future has no shared state.
      ReplyNoState,
    };

  }

  export template <>
  struct ErrorCodeTraits<actor::Errc>
  {
    /// Error category name used by `std::error_code`.
    static constexpr const char* Name = "cxx.actor";

    /// Returns the message associated with an actor error code.
    [[nodiscard]] static constexpr auto Message(actor::Errc code) noexcept -> std::string_view
    {
      using enum actor::Errc;

      switch (code)
      {
        case None:
          return "No error";
        case Stopped:
          return "Actor is stopped";
        case ReplyAbandoned:
          return "Reply was abandoned without a value";
        case ReplyAlreadyCompleted:
          return "Reply was already completed";
        case ReplyAlreadyTaken:
          return "Reply result was already taken";
        case ReplyNoState:
          return "Reply has no shared state";
        default:
          return "Unknown actor error";
      }
    }
  };

  namespace actor
  {

    /// Result type used by actor reply futures.
    ///
    /// This is an alias for `cxx::Result<T>`.
    export template <class T>
    using ReplyResult = Result<T>;

    /// Shared state for a one-shot reply channel.
    ///
    /// The reply side writes one `Result<T>` and notifies waiters. The future
    /// side may take that result once.
    template <class T>
    struct ReplyState
    {
      mutable std::mutex       mutex{};
      std::condition_variable  cv{};
      std::optional<Result<T>> result{};
      bool                     taken{false};
    };

    export template <class T>
    class Reply;

    /// Future side of a one-shot actor reply.
    ///
    /// A `ReplyFuture<T>` is returned to the caller while a matching
    /// `Reply<T>` is moved into the posted request. The future can be polled
    /// with `TryTake` or waited on with `Wait`.
    ///
    /// ## Ownership
    ///
    /// `ReplyFuture` is move-only. The contained result can be consumed once.
    ///
    /// ## Error handling
    ///
    /// `Wait` returns `Errc::ReplyNoState` for a default-constructed or moved
    /// from future, `Errc::ReplyAlreadyTaken` after the result was already
    /// consumed, and the request-provided error if the handler rejects the reply.
    ///
    /// ## Thread safety
    ///
    /// `IsReady`, `TryTake`, and `Wait` synchronize through the shared reply
    /// state. Multiple consumers are allowed by the type, but only one can take
    /// the result successfully.
    ///
    /// @tparam T Reply value type.
    export template <class T>
    class ReplyFuture
    {
  public:

      /// Creates an invalid future with no shared state.
      ReplyFuture() = default;

      ReplyFuture(const ReplyFuture&)                    = delete;
      auto operator=(const ReplyFuture&) -> ReplyFuture& = delete;

      ReplyFuture(ReplyFuture&&) noexcept                    = default;
      auto operator=(ReplyFuture&&) noexcept -> ReplyFuture& = default;

      /// Returns whether this future has shared reply state.
      [[nodiscard]] auto IsValid() const -> bool
      {
        return static_cast<bool>(state);
      }

      /// Returns whether a result is available without blocking.
      ///
      /// Invalid futures are considered ready and will return `ReplyNoState`
      /// from `TryTake` or `Wait`.
      [[nodiscard]] auto IsReady() const -> bool
      {
        if (!state)
        {
          return true;
        }

        std::lock_guard lock{state->mutex};
        return state->result.has_value();
      }

      /// Attempts to take the reply result without blocking.
      ///
      /// @return `std::nullopt` when the reply is still pending, otherwise the
      /// result or an error result. The result can be taken only once.
      auto TryTake() -> std::optional<Result<T>>
      {
        if (!state)
        {
          return cxx::Result<T>{std::unexpected{Error::Make(Errc::ReplyNoState)}};
        }

        std::lock_guard lock{state->mutex};

        if (!state->result)
        {
          return std::nullopt;
        }

        if (state->taken)
        {
          return cxx::Result<T>{std::unexpected{Error::Make(Errc::ReplyAlreadyTaken)}};
        }

        state->taken = true;
        return std::move(*state->result);
      }

      /// Waits until the reply is resolved or rejected and consumes the result.
      ///
      /// @return The reply value, or a `cxx::Error` when the request was
      /// rejected, abandoned, already taken, or the future is invalid.
      auto Wait() -> Result<T>
      {
        if (!state)
        {
          return std::unexpected{Error::Make(Errc::ReplyNoState)};
        }

        std::unique_lock lock{state->mutex};

        state->cv.wait(lock, [this] { return state->result.has_value(); });

        if (state->taken)
        {
          return std::unexpected{Error::Make(Errc::ReplyAlreadyTaken)};
        }

        state->taken = true;
        return std::move(*state->result);
      }

  private:

      template <class>
      friend class Reply;

      explicit ReplyFuture(std::shared_ptr<ReplyState<T>> state) : state{std::move(state)} {}

      std::shared_ptr<ReplyState<T>> state{};
    };

    template <class T>
    class Reply
    {
  public:

      /// Creates an invalid reply handle.
      Reply() = default;

      Reply(const Reply&)                    = delete;
      auto operator=(const Reply&) -> Reply& = delete;

      Reply(Reply&&) noexcept                    = default;
      auto operator=(Reply&&) noexcept -> Reply& = default;

      /// Rejects an unresolved reply as abandoned.
      ///
      /// Moving the reply into a request transfers this responsibility to the
      /// moved-to handle. A handler should normally call `TryResolve` or
      /// `TryReject` explicitly.
      ~Reply()
      {
        if (state)
        {
          [[maybe_unused]] const auto completed = TryReject(Error::Make(Errc::ReplyAbandoned));
        }
      }

      /// Returns whether this reply has shared reply state.
      [[nodiscard]] auto IsValid() const -> bool
      {
        return static_cast<bool>(state);
      }

      /// Resolves the reply with a value.
      ///
      /// @return `true` when this call completed the reply, or `false` when the
      /// reply was invalid or already completed.
      template <class U>
      requires std::constructible_from<T, U&&>
      auto TryResolve(U&& value) -> bool
      {
        if (!state)
        {
          return false;
        }

        {
          std::lock_guard lock{state->mutex};

          if (state->result)
          {
            return false;
          }

          state->result.emplace(std::in_place, std::forward<U>(value));
        }

        state->cv.notify_all();
        state.reset();
        return true;
      }

      /// Rejects the reply with an error.
      ///
      /// @return `true` when this call completed the reply, or `false` when the
      /// reply was invalid or already completed.
      auto TryReject(cxx::Error error) -> bool
      {
        if (!state)
        {
          return false;
        }

        {
          std::lock_guard lock{state->mutex};

          if (state->result)
          {
            return false;
          }

          state->result.emplace(std::unexpected{std::move(error)});
        }

        state->cv.notify_all();
        state.reset();
        return true;
      }

      /// Rejects the reply with an adapted enum error code.
      template <class Enum>
      requires cxx::ErrorCodeEnum<Enum>
      auto TryReject(Enum code, std::string message = {}) -> bool
      {
        return TryReject(cxx::Error::Make(code, std::move(message)));
      }

  private:

      template <class>
      friend auto MakeReply() -> std::pair<Reply, ReplyFuture<T>>;

      explicit Reply(std::shared_ptr<ReplyState<T>> state) : state{std::move(state)} {}

      std::shared_ptr<ReplyState<T>> state{};
    };

    template <class T>
    auto MakeReply() -> std::pair<Reply<T>, ReplyFuture<T>>
    {
      auto state = std::make_shared<ReplyState<T>>();

      return {Reply<T>{state}, ReplyFuture<T>{std::move(state)}};
    }

    /// Reply handle specialization for requests that complete without a value.
    export template <>
    class Reply<void>
    {
  public:

      /// Creates an invalid void reply handle.
      Reply() = default;

      Reply(const Reply&)                    = delete;
      auto operator=(const Reply&) -> Reply& = delete;

      Reply(Reply&&) noexcept                    = default;
      auto operator=(Reply&&) noexcept -> Reply& = default;

      /// Rejects an unresolved reply as abandoned.
      ~Reply()
      {
        if (state)
        {
          [[maybe_unused]] const auto completed = TryReject(Error::Make(Errc::ReplyAbandoned));
        }
      }

      /// Returns whether this reply has shared reply state.
      [[nodiscard]] auto IsValid() const -> bool
      {
        return static_cast<bool>(state);
      }

      /// Resolves the reply successfully.
      ///
      /// @return `true` when this call completed the reply.
      auto TryResolve() -> bool
      {
        if (!state)
        {
          return false;
        }

        {
          std::lock_guard lock{state->mutex};

          if (state->result)
          {
            return false;
          }

          state->result.emplace();
        }

        state->cv.notify_all();
        state.reset();
        return true;
      }

      /// Rejects the reply with an error.
      auto TryReject(Error error) -> bool
      {
        if (!state)
        {
          return false;
        }

        {
          std::lock_guard lock{state->mutex};

          if (state->result)
          {
            return false;
          }

          state->result.emplace(std::unexpected{std::move(error)});
        }

        state->cv.notify_all();
        state.reset();
        return true;
      }

      /// Rejects the reply with an adapted enum error code.
      template <class Enum>
      requires cxx::ErrorCodeEnum<Enum>
      auto TryReject(Enum code, std::string message = {}) -> bool
      {
        return TryReject(cxx::Error::Make(code, std::move(message)));
      }

  private:

      template <class>
      friend auto MakeReply() -> std::pair<Reply, ReplyFuture<void>>;

      explicit Reply(std::shared_ptr<ReplyState<void>> state) : state{std::move(state)} {}

      std::shared_ptr<ReplyState<void>> state{};
    };

    /// Concept for request types accepted by `Actor::PostAndReply`.
    ///
    /// A request type must provide `using ReplyType = T;`. The actor constructs
    /// the request with a `Reply<T>` as the first constructor or aggregate
    /// argument, followed by the arguments passed to `PostAndReply`.
    template <typename Request>
    concept ReplyRequest = requires { typename Request::ReplyType; };

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
      /// @return `true` when the message was accepted, or `false` after stop.
      template <class M>
      requires std::constructible_from<Message, M&&>
      auto Post(M&& message) -> bool
      {
        if (IsStopped())
        {
          return false;
        }

        const auto posted = incoming.Push(std::forward<M>(message));

        if (posted)
        {
          wakeCv.notify_all();
        }

        return posted;
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

      /// Posts a request and returns a future for its one-shot reply.
      ///
      /// `Request` must declare `using ReplyType = T;` and be constructible with
      /// `Reply<T>` followed by `args...`. When `Message` is a variant that can
      /// be constructed with `std::in_place_type_t<Request>`, that form is used;
      /// otherwise the actor constructs `Request{reply, args...}` and then
      /// constructs `Message` from it.
      ///
      /// ## Reply contract
      ///
      /// The handler must call `request.reply.TryResolve(...)` or
      /// `request.reply.TryReject(...)`. If the reply handle is destroyed before
      /// completion, the future receives `Errc::ReplyAbandoned`.
      ///
      /// ## Stopped actors
      ///
      /// If the actor is already stopped, no message is posted and the returned
      /// future is immediately rejected with `Errc::Stopped`.
      ///
      /// ## Example
      ///
      /// ```cpp
      /// struct GetCount {
      ///   using ReplyType = int;
      ///   cxx::actor::Reply<int> reply;
      /// };
      ///
      /// auto future = actor.PostAndReply<GetCount>();
      /// actor.Update();
      ///
      /// auto count = future.Wait();
      /// ```
      ///
      /// @tparam Request Request message type with `ReplyType`.
      /// @param args Arguments forwarded after the generated reply handle.
      /// @return Future that can be waited on or polled.
      template <class Request, class... Args>
      requires actor::ReplyRequest<Request>
      auto PostAndReply(Args&&... args) -> ReplyFuture<typename Request::ReplyType>
      {
        using Result = Request::ReplyType;

        auto [reply, future] = actor::MakeReply<Result>();

        if (IsStopped())
        {
          [[maybe_unused]] const auto rejected = reply.TryReject(Error::Make(actor::Errc::Stopped));

          return future;
        }

        auto message = MakeReplyMessage<Request>(std::move(reply), std::forward<Args>(args)...);

        [[maybe_unused]] const auto posted = Post(std::move(message));

        return future;
      }

  private:

      template <class Request, class Result, class... Args>
      static auto MakeReplyMessage(Reply<Result>&& reply, Args&&... args) -> Message
      {
        if constexpr (std::constructible_from<Message, std::in_place_type_t<Request>, Reply<Result>&&, Args&&...>)
        {
          return Message{std::in_place_type<Request>, std::move(reply), std::forward<Args>(args)...};
        }
        else
        {
          return Message{
              Request{std::move(reply), std::forward<Args>(args)...}
          };
        }
      }

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
