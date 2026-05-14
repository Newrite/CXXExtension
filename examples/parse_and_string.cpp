import IXXExtension;
import std;

auto main() -> int
{
  auto number = ixx::ParseInt<>(" 42 ");
  auto flag   = ixx::ParseBool("enabled");
  auto trimmed = ixx::TrimAscii("  red::green::blue\n");
  auto parts  = ixx::Split(trimmed, "::");
  auto joined = ixx::Join(parts, "|");
  auto upper  = ixx::ToUpperAscii(joined);
  auto text   = ixx::ReplaceAll(upper, "GREEN", "YELLOW");

  if (!number || !flag) return 1;

  return *number == 42 && *flag && text == "RED|YELLOW|BLUE" ? 0 : 1;
}
