import IXXExtension;
import std;

auto main() -> int
{
  std::string_view text = "hello";

  if (!ixx::text::IsValidUtf8(text)) return 1;

  auto valid = ixx::text::ValidateUtf8(text);
  if (!valid) return 1;

  auto u8 = ixx::text::ToU8(text);
  auto bytes = ixx::text::FromU8(u8);

  if (bytes != "hello") return 1;

#ifdef _WIN32
  auto wide = ixx::text::Utf8ToWide(bytes);
  if (!wide) return 1;

  auto roundTrip = ixx::text::WideToUtf8(*wide);
  if (!roundTrip || *roundTrip != bytes) return 1;
#endif

  return 0;
}
