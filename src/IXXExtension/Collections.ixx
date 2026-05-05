/// Owner-local collection primitives used directly and by actor runtimes.
///
/// This module exports FIFO mailboxes and inboxes with stashing. The types are
/// intentionally not synchronized; use them when access is owner-local or
/// externally controlled.
export module IXXExtension.Collections;

import IXXExtension.Core;
import IXXExtension.ContainerExtension;

import std;

namespace ixx
{

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
  /// ixx::Mailbox<std::string> mailbox;
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
  /// ixx::Inbox<std::string> inbox;
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

}  // namespace ixx
