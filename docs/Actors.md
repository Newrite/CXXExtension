# Actors

The actor API is a lightweight actor model with two processing modes.

Import `IXXExtension.Concurrency` for actor types, one-shot channels, unbounded
channels, and reply helpers.

External threads call `Actor::Post(...)`. Before autonomous mode is started, the
actor owner calls `Actor::Update()` to process pending work. `Post` is
synchronized; the inbox, stash, handler, and state are actor-local while messages
are being handled.

```cpp
auto actor = ixx::actor::Make<Message>(State{}, Handler{});
const bool posted = actor.Post(Message{Connected{}});
actor.Update();
```

For autonomous processing, call `Start()` once. The actor owns a worker thread
until `Stop()` or destruction.

```cpp
auto actor = ixx::actor::Make<Message>(State{}, Handler{});

if (actor.Start())
{
  const bool posted = actor.Post(Message{Connected{}});
  actor.Stop();
}
```

`Stop()` is idempotent. It closes the incoming buffer, wakes the worker, requests
thread stop, and joins the worker when called from another thread. `Post` returns
`false` after stop.

Handlers are invoked as:

```cpp
handler(ctx, state, message);
```

where `ctx` is `ixx::actor::Context<Message, State>&`, `state` is `State&`, and
`message` is `Message&`.

Use `ctx.StashCurrent()` to request that the current message be stashed after
the handler returns. Use `ctx.Become(...)` to mutate or replace state and then
restore stashed messages.

## Replies

Request/reply messages use `ixx::actor::Reply<T>` and
`ixx::actor::ReplyFuture<T>`, which are aliases for the generic
`ixx::oneshot::Sender<T>` and `ixx::oneshot::Receiver<T>`. A request type opts
in by declaring `using ReplyType = T;`; `PostAndReply<Request>(args...)` creates
the channel, posts the request, and returns the receiver.

```cpp
struct GetCount
{
  using ReplyType = int;
  ixx::actor::Reply<int> reply;
};

auto future = actor.PostAndReply<GetCount>();
actor.Update();

auto count = future.Wait();
```

The handler must send or reject the reply exactly once. If the sender is
destroyed unresolved, the receiver completes with `ixx::oneshot::Errc::Abandoned`.
If the actor is already stopped, `PostAndReply` returns a future rejected with
`ixx::actor::Errc::Stopped`.

## One-Shot Channels

`ixx::oneshot::Make<T>()` creates a single-use sender/receiver pair. The sender
completes the channel with either `Send(value)` or `Reject(error)`, and the
receiver consumes the result once with `Wait()` or `TryTake()`.

The top-level aliases `ixx::OneShotSender<T>` and
`ixx::OneShotReceiver<T>` are available when a declaration does not need the
nested `oneshot` namespace.

```cpp
ixx::OneShotSender<int> sender;
ixx::OneShotReceiver<int> receiver;

std::tie(sender, receiver) = ixx::oneshot::Make<int>();

sender.Send(42);
auto value = receiver.Wait();
```

## Channels

`ixx::channel::Unbounded<T>()` creates a copyable sender and a move-only
receiver. Sends append to a synchronized FIFO queue; receives move values out.

```cpp
auto [sender, receiver] = ixx::channel::Unbounded<std::string>();

sender.Send("ready");
auto message = receiver.WaitReceive();
```

Use `TryReceive()` to poll without blocking. Closing the channel or dropping the
last sender makes an empty receiver return `ixx::channel::Errc::Closed`.

Do not call `Update()` concurrently with another `Update()` or with an
autonomous worker after `Start()` succeeds.
