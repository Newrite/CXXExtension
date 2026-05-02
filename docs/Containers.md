# Containers

`CXXExtension.Container` exports `cxx::Mailbox<Message>` and
`cxx::Inbox<Message>`.

## Mailbox

`Mailbox` is an owner-local FIFO queue. It is not thread-safe.

```cpp
cxx::Mailbox<std::string> mailbox;
mailbox.Push("hello");

auto message = mailbox.Receive();
```

`Receive` moves the front message out and removes it. `PushFront` restores a
message before already queued messages.

## Inbox

`Inbox` combines a mailbox with a stash buffer. It is actor-local and not
thread-safe.

```cpp
cxx::Inbox<std::string> inbox;
inbox.Push("already queued");
inbox.Stash("wait until ready");
inbox.UnstashAll();
```

`UnstashAll` preserves stash order and restores stashed messages before already
queued messages.

## Actors

`cxx::actor::Actor` uses a synchronized incoming buffer for external `Post`
calls and an actor-local `Inbox` for processing. It can be manually pumped with
`Update()` or run on an owned worker thread with `Start()`.

```cpp
auto actor = cxx::actor::Make<Message>(State{}, Handler{});
const bool posted = actor.Post(Message{});
actor.Update();
```

`Stop()` closes the incoming buffer. `Post` returns `false` after stop.

Actors also provide one-shot request/reply helpers with
`cxx::actor::Reply<T>`, `cxx::actor::ReplyFuture<T>`, and
`Actor::PostAndReply<Request>(...)`.

## ContainerExtension

`CXXExtension.ContainerExtension` contains range, vector, and map helpers. The
unstable erase helpers are intentionally order-changing; use the `EraseFirst`
family when order matters.
