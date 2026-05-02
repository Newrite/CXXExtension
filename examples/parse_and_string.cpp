import CXXExtension;
import std;

auto main() -> int
{
  auto number = cxx::ParseInt<>(" 42 ");
  auto flag   = cxx::ParseBool("enabled");
  auto parts  = cxx::Split("red,green,blue", ',');
  auto joined = cxx::Join(parts, "|");

  if (!number || !flag) return 1;

  return *number == 42 && *flag && joined == "red|green|blue" ? 0 : 1;
}
