import IXXExtension.Concurrency;
import std;

auto main() -> int
{
  ixx::OneShotSender<int>   sender;
  ixx::OneShotReceiver<int> receiver;

  std::tie(sender, receiver) = ixx::oneshot::Make<int>();

  auto sendResult = sender.Send(42);
  if (!sendResult) return 1;

  auto value = receiver.Wait();
  if (!value) return 1;

  return *value == 42 ? 0 : 1;
}
