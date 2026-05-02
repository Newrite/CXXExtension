import CXXExtension.Container;
import std;

struct Increment
{};

struct GetCount
{
  using ReplyType = int;

  cxx::actor::Reply<int> reply;
};

using Message = std::variant<Increment, GetCount>;

struct State
{
  int count{};
};

struct Handler
{
  auto operator()(cxx::actor::Context<Message, State>&, State& state, Message& message) -> void
  {
    std::visit(Visitor{state}, message);
  }

  struct Visitor
  {
    State& state;

    auto operator()(Increment&) const -> void
    {
      ++state.count;
    }

    auto operator()(GetCount& request) const -> void
    {
      [[maybe_unused]] const bool resolved = request.reply.TryResolve(state.count);
    }
  };
};

auto main() -> int
{
  auto actor = cxx::actor::Make<Message>(State{}, Handler{});

  if (!actor.Post(Message{Increment{}})) return 1;

  auto future = actor.PostAndReply<GetCount>();

  actor.Update();

  auto count = future.Wait();

  return count && *count == 1 ? 0 : 1;
}
