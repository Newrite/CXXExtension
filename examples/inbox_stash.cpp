import CXXExtension.Container;
import std;

auto main() -> int
{
  cxx::Inbox<std::string> inbox;

  inbox.Push("already queued");
  inbox.Stash("wait until ready");
  inbox.UnstashAll();

  const auto first  = inbox.Receive();
  const auto second = inbox.Receive();

  if (!first || !second) return 1;

  return *first == "wait until ready" && *second == "already queued" ? 0 : 1;
}
