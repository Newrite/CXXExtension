import CXXExtension.ContainerExtension;
import std;

auto main() -> int
{
  std::vector<int> values{1, 2, 3};

  const bool inserted = cxx::PushUnique(values, 4);
  const bool erased   = cxx::EraseFastFirst(values, 2);
  const auto index    = cxx::IndexOf(values, 4);

  return inserted && erased && index.has_value() ? 0 : 1;
}
