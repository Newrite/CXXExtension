# Actors

The actor API is a lightweight actor model with two processing modes.

External threads call `Actor::Post(...)`. Before autonomous mode is started, the
actor owner calls `Actor::Update()` to process pending work. `Post` is
synchronized; the inbox, stash, handler, and state are actor-local while messages
are being handled.

```cpp
auto actor = cxx::actor::Make<Message>(State{}, Handler{});
actor.Post(Message{Connected{}});
actor.Update();
```

For autonomous processing, call `Start()` once. The actor owns a worker thread
until `Stop()` or destruction.

```cpp
auto actor = cxx::actor::Make<Message>(State{}, Handler{});

if (actor.Start())
{
  actor.Post(Message{Connected{}});
  actor.Stop();
}
```

`Stop()` is idempotent. It closes the incoming buffer, wakes the worker, requests
thread stop, and joins the worker when called from another thread. Posts after
stop are ignored by the actor's incoming buffer.

Handlers are invoked as:

```cpp
handler(ctx, state, message);
```

where `ctx` is `cxx::actor::Context<Message, State>&`, `state` is `State&`, and
`message` is `Message&`.

Use `ctx.StashCurrent()` to request that the current message be stashed after
the handler returns. Use `ctx.Become(...)` to mutate or replace state and then
restore stashed messages.

Do not call `Update()` concurrently with another `Update()` or with an
autonomous worker after `Start()` succeeds.
