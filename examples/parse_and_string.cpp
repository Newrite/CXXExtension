import IXXExtension;
import std;

auto main() -> int
{
  auto number = ixx::ParseInt<>(" 42 ");
  auto flag   = ixx::ParseBool("enabled");
  auto parts  = ixx::Split("red,green,blue", ',');
  auto joined = ixx::Join(parts, "|");

  if (!number || !flag) return 1;

  return *number == 42 && *flag && joined == "red|green|blue" ? 0 : 1;
}
