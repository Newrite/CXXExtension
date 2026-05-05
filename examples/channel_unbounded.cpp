import IXXExtension.Concurrency;
import std;

auto main() -> int
{
  auto [sender, receiver] = ixx::channel::Unbounded<std::string>();

  auto sendResult = sender.Send("hello");
  if (!sendResult) return 1;

  auto received = receiver.WaitReceive();
  if (!received) return 1;

  sender.Close();

  auto closed = receiver.TryReceive();

  return *received == "hello" && closed && !*closed ? 0 : 1;
}
