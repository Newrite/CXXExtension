import IXXExtension.Concurrency;
import std;

struct Ping
{};

using Message = Ping;

struct State
{
  int handled{};
};

struct Handler
{
  auto operator()(ixx::actor::Context<Message, State>&, State& state, Message&) -> void
  {
    ++state.handled;
  }
};

auto main() -> int
{
  auto actor = ixx::actor::Make<Message>(State{}, Handler{});

  if (!actor.Start()) return 1;

  if (!actor.Post(Ping{})) return 1;
  actor.Stop();

  return actor.IsStopped() ? 0 : 1;
}
