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

## ContainerExtension

`CXXExtension.ContainerExtension` contains range, vector, and map helpers. The
unstable erase helpers are intentionally order-changing; use the `EraseFirst`
family when order matters.
