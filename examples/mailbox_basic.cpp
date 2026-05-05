import IXXExtension.Collections;
import std;

auto main() -> int
{
  ixx::Mailbox<std::string> mailbox;

  mailbox.Push("hello");
  mailbox.Push("world");

  std::vector<std::string> received;
  while (auto message = mailbox.Receive())
  {
    received.push_back(std::move(*message));
  }

  return received == std::vector<std::string>{"hello", "world"} ? 0 : 1;
}
