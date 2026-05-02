# Actors

The actor API is a lightweight, manually updated actor model.

External threads call `Actor::Post(...)`. The actor owner calls `Actor::Update()`
to process pending work. `Post` is synchronized; `Update`, the inbox, stash,
handler, and state are actor-local.

```cpp
auto actor = cxx::actor::Make<Message>(State{}, Handler{});
actor.Post(Message{Connected{}});
actor.Update();
```

Handlers are invoked as:

```cpp
handler(ctx, state, message);
```

where `ctx` is `cxx::actor::Context<Message, State>&`, `state` is `State&`, and
`message` is `Message&`.

Use `ctx.StashCurrent()` to request that the current message be stashed after
the handler returns. Use `ctx.Become(...)` to mutate or replace state and then
restore stashed messages.

There is no autonomous worker mode in the current public API: no `Start`, `Stop`,
or actor-owned worker thread is exported.
