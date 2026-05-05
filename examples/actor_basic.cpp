import IXXExtension.Concurrency;
import std;

struct Connected
{};

struct SendChat
{
  std::string text;
};

using Message = std::variant<Connected, SendChat>;

struct State
{
  bool                     connected{};
  std::vector<std::string> sent{};
};

struct Handler
{
  auto operator()(ixx::actor::Context<Message, State>& ctx, State& state, Message& message) -> void
  {
    std::visit(Visitor{ctx, state}, message);
  }

  struct Visitor
  {
    ixx::actor::Context<Message, State>& ctx;
    State&                               state;

    auto operator()(Connected&) -> void
    {
      ctx.Become([](State& s) { s.connected = true; });
    }

    auto operator()(SendChat& message) -> void
    {
      if (!state.connected)
      {
        ctx.StashCurrent();
        return;
      }

      state.sent.push_back(message.text);
    }
  };
};

auto main() -> int
{
  auto actor = ixx::actor::Make<Message>(State{}, Handler{});

  if (!actor.Post(Message{SendChat{"hello before connect"}})) return 1;
  actor.Update();

  if (!actor.Post(Message{Connected{}})) return 1;
  actor.Update();

  if (!actor.Post(Message{SendChat{"hello after connect"}})) return 1;
  actor.Update();

  return actor.IncomingCount() == 0 ? 0 : 1;
}
