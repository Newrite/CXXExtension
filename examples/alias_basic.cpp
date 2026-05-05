import IXXExtension.Core;
import std;

struct UserIdTag;
struct OffsetTag;

using UserId = ixx::Alias<std::uint64_t, UserIdTag, ixx::alias::DereferenceUnwrap>;
using Offset = ixx::Alias<int, OffsetTag, ixx::alias::DereferenceUnwrap, ixx::alias::UnaryArithmetic>;

auto main() -> int
{
  auto id     = ixx::alias::Into<UserId>(42);
  auto offset = Offset{-5};

  static_assert(ixx::alias::AliasType<UserId>);
  static_assert(!std::same_as<UserId, Offset>);

  auto rawId     = *id;
  auto rawOffset = ixx::alias::Unwrap(-offset);

  return rawId == 42 && rawOffset == 5 ? 0 : 1;
}
