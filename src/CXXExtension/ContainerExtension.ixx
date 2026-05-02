/// Range, vector, and associative-container convenience utilities.
///
/// This module wraps common `<ranges>` and container operations with small,
/// intention-revealing helpers. Functions do not add synchronization; normal
/// container iterator and reference invalidation rules still apply.
export module CXXExtension.ContainerExtension;

import std;

namespace cxx
{

  /// Collects a range into `std::vector<range_value_t<R>>`.
  ///
  /// Iteration order is preserved.
  export template <std::ranges::input_range R>
  [[nodiscard]] auto ToVector(R&& range)
  {
    using T = std::ranges::range_value_t<R>;

    return std::forward<R>(range) | std::ranges::to<std::vector<T>>();
  }

  /// Collects a range into `std::vector<T>`.
  ///
  /// Use this overload when the destination value type should differ from the
  /// range value type.
  export template <class T, std::ranges::input_range R>
  [[nodiscard]] auto ToVector(R&& range) -> std::vector<T>
  {
    return std::forward<R>(range) | std::ranges::to<std::vector<T>>();
  }

  /// Returns whether a range contains `value`.
  export template <std::ranges::input_range R, class T>
  auto Contains(R&& range, const T& value)
  {
    return std::ranges::contains(range, value);
  }

  /// Finds the first element equal to `value`.
  ///
  /// @return Iterator returned by `std::ranges::find`.
  export template <std::ranges::input_range R, class T>
  auto Find(R&& range, const T& value)
  {
    return std::ranges::find(range, value);
  }

  /// Finds the first element matching a predicate.
  ///
  /// @return Iterator returned by `std::ranges::find_if`.
  export template <std::ranges::input_range R, class Pred>
  auto FindIf(R&& range, Pred pred)
  {
    return std::ranges::find_if(range, pred);
  }

  /// Returns the zero-based index of the first matching value.
  ///
  /// @return Index, or `std::nullopt` when no element matches.
  export template <std::ranges::random_access_range R, class T>
  auto IndexOf(R&& range, const T& value) -> std::optional<size_t>
  {
    const auto it = std::ranges::find(range, value);

    if (it == std::ranges::end(range)) return std::nullopt;

    return static_cast<size_t>(std::ranges::distance(std::ranges::begin(range), it));
  }

  /// Returns the zero-based index of the first element matching a predicate.
  export template <std::ranges::random_access_range R, class Pred>
  auto IndexOfIf(R&& range, Pred pred) -> std::optional<size_t>
  {
    const auto it = std::ranges::find_if(range, pred);

    if (it == std::ranges::end(range)) return std::nullopt;

    return static_cast<size_t>(std::ranges::distance(std::ranges::begin(range), it));
  }

  /// Returns a pointer to the first matching element in a contiguous range.
  ///
  /// The returned pointer is borrowed from the range and is invalidated according
  /// to the range's normal mutation rules.
  export template <std::ranges::contiguous_range R, class T>
  auto FindPtr(R& range, const T& value) -> std::ranges::range_value_t<R>*
  {
    auto it = std::ranges::find(range, value);

    if (it == std::ranges::end(range)) return nullptr;

    return std::addressof(*it);
  }

  /// Returns a pointer to the first predicate-matching element in a contiguous range.
  export template <std::ranges::contiguous_range R, class Pred>
  auto FindIfPtr(R& range, Pred pred) -> std::ranges::range_value_t<R>*
  {
    auto it = std::ranges::find_if(range, pred);

    if (it == std::ranges::end(range)) return nullptr;

    return std::addressof(*it);
  }

  /// Returns a const pointer to the first predicate-matching element.
  export template <std::ranges::contiguous_range R, class Pred>
  auto FindIfPtr(const R& range, Pred pred) -> const std::ranges::range_value_t<R>*
  {
    auto it = std::ranges::find_if(range, pred);

    if (it == std::ranges::end(range)) return nullptr;

    return std::addressof(*it);
  }

  /// Finds a mapped value by key and returns a mutable pointer.
  ///
  /// The returned pointer is borrowed from the map and may be invalidated by map
  /// mutation.
  export template <class Map, class Key>
  auto FindValuePtr(Map& map, const Key& key) -> Map::mapped_type*
  {
    auto it = map.find(key);

    if (it == map.end()) return nullptr;

    return std::addressof(it->second);
  }

  /// Finds a mapped value by key and returns a const pointer.
  export template <class Map, class Key>
  auto FindValuePtr(const Map& map, const Key& key) -> const Map::mapped_type*
  {
    auto it = map.find(key);

    if (it == map.end()) return nullptr;

    return std::addressof(it->second);
  }

  /// Erases the first vector element equal to `value`.
  ///
  /// Order is preserved. Elements after the erased position may move.
  ///
  /// @return `true` when an element was erased.
  export template <class T, class Alloc, class U>
  auto EraseFirst(std::vector<T, Alloc>& v, const U& value)
  {
    const auto it = std::ranges::find(v, value);

    if (it == v.end()) return false;

    v.erase(it);
    return true;
  }

  /// Erases the first vector element matching a predicate.
  ///
  /// Order is preserved. Elements after the erased position may move.
  export template <class T, class Alloc, class Pred>
  auto EraseFirstIf(std::vector<T, Alloc>& v, Pred pred)
  {
    const auto it = std::ranges::find_if(v, pred);

    if (it == v.end()) return false;

    v.erase(it);
    return true;
  }

  /// Erases a vector element by swapping in the last element.
  ///
  /// This is O(1), but it does not preserve order.
  ///
  /// @return `false` when `index` is out of range.
  export template <class T, class Alloc>
  auto EraseFast(std::vector<T, Alloc>& v, size_t index)
  {
    if (index >= v.size()) return false;

    if (index + 1 != v.size()) v[index] = std::move(v.back());

    v.pop_back();
    return true;
  }

  /// Erases the first matching value with unstable O(1) removal.
  ///
  /// Vector order is not preserved.
  export template <class T, class Alloc, class U>
  auto EraseFastFirst(std::vector<T, Alloc>& v, const U& value)
  {
    const auto it = std::ranges::find(v, value);

    if (it == v.end()) return false;

    const auto index = static_cast<size_t>(std::distance(v.begin(), it));
    return EraseFast(v, index);
  }

  /// Erases the first predicate match with unstable O(1) removal.
  ///
  /// Vector order is not preserved.
  export template <class T, class Alloc, class Pred>
  auto EraseFastFirstIf(std::vector<T, Alloc>& v, Pred pred)
  {
    const auto it = std::ranges::find_if(v, pred);

    if (it == v.end()) return false;

    const auto index = static_cast<size_t>(std::distance(v.begin(), it));
    return EraseFast(v, index);
  }

  /// Appends a value to a vector only if it is not already present.
  ///
  /// Equality is checked with `Contains(v, value)` before insertion.
  ///
  /// @return `true` when the value was inserted.
  export template <class T, class Alloc, class U>
  auto PushUnique(std::vector<T, Alloc>& v, U&& value)
  {
    if (Contains(v, value)) return false;

    v.emplace_back(std::forward<U>(value));
    return true;
  }

  /// Moves and removes the last vector element.
  ///
  /// @return Moved value, or `std::nullopt` when the vector is empty.
  export template <class T, class Alloc>
  auto PopBack(std::vector<T, Alloc>& v) -> std::optional<T>
  {
    if (v.empty()) return std::nullopt;

    T value = std::move(v.back());
    v.pop_back();
    return value;
  }

  /// Returns a mutable pointer to a vector element by index.
  ///
  /// The pointer is borrowed from the vector and follows normal vector
  /// invalidation rules.
  export template <class T, class Alloc>
  auto AtPtr(std::vector<T, Alloc>& v, size_t index) -> T*
  {
    if (index >= v.size()) return nullptr;

    return std::addressof(v[index]);
  }

  /// Returns a const pointer to a vector element by index.
  export template <class T, class Alloc>
  auto AtPtr(const std::vector<T, Alloc>& v, size_t index) -> const T*
  {
    if (index >= v.size()) return nullptr;

    return std::addressof(v[index]);
  }

  /// Copies all keys from a map-like container into a vector.
  ///
  /// Iteration order follows the map's iteration order.
  export template <class Map>
  auto Keys(const Map& map)
  {
    std::vector<typename Map::key_type> result;
    result.reserve(map.size());

    for (const auto& [key, value] : map)
      result.push_back(key);

    return result;
  }

  /// Copies all mapped values from a map-like container into a vector.
  ///
  /// Iteration order follows the map's iteration order.
  export template <class Map>
  auto Values(const Map& map)
  {
    std::vector<typename Map::mapped_type> result;
    result.reserve(map.size());

    for (const auto& [key, value] : map)
      result.push_back(value);

    return result;
  }

  /// Finds a mapped value by key and returns a mutable pointer.
  ///
  /// This is an alias-like helper for nullable map lookup.
  export template <class Map, class Key>
  auto GetOrNull(Map& map, const Key& key) -> Map::mapped_type*
  {
    auto it = map.find(key);

    if (it == map.end()) return nullptr;

    return std::addressof(it->second);
  }

  /// Finds a mapped value by key and returns a const pointer.
  export template <class Map, class Key>
  auto GetOrNull(const Map& map, const Key& key) -> const Map::mapped_type*
  {
    auto it = map.find(key);

    if (it == map.end()) return nullptr;

    return std::addressof(it->second);
  }

  /// Returns a mapped value or a fallback value.
  ///
  /// The return type is deduced from either `fallback` or the mapped value. A
  /// found mapped value is returned by copy.
  export template <class Map, class Key, class Default>
  auto GetOrDefault(const Map& map, const Key& key, Default&& fallback)
  {
    auto it = map.find(key);

    if (it == map.end()) return std::forward<Default>(fallback);

    return it->second;
  }

  /// Returns an existing mapped value or emplaces a new one.
  ///
  /// @return Reference to the mapped value stored in the map.
  export template <class Map, class Key, class... Args>
  auto GetOrEmplace(Map& map, Key&& key, Args&&... args) -> Map::mapped_type&
  {
    auto [it, inserted] = map.try_emplace(std::forward<Key>(key), std::forward<Args>(args)...);

    return it->second;
  }

  /// Finds the first element whose projection equals `value`.
  ///
  /// @return Iterator returned by `std::ranges::find`.
  export template <std::ranges::input_range R, class T, class Proj>
  auto FindBy(R&& range, const T& value, Proj proj)
  {
    return std::ranges::find(range, value, proj);
  }

  /// Returns whether any projected element equals `value`.
  export template <std::ranges::input_range R, class T, class Proj>
  auto ContainsBy(R&& range, const T& value, Proj proj)
  {
    return std::ranges::find(range, value, proj) != std::ranges::end(range);
  }

  /// Moves out and erases the first vector element matching a predicate.
  ///
  /// Order is preserved. Elements after the erased position may move.
  ///
  /// @return Moved value, or `std::nullopt` when no element matches.
  export template <class T, class Alloc, class Pred>
  auto TakeFirstIf(std::vector<T, Alloc>& v, Pred pred) -> std::optional<T>
  {
    auto it = std::ranges::find_if(v, pred);

    if (it == v.end()) return std::nullopt;

    T value = std::move(*it);
    v.erase(it);
    return value;
  }

  /// Erases all vector elements matching a predicate using unstable removal.
  ///
  /// Vector order is not preserved. The predicate may observe elements in an
  /// order affected by swaps from the back.
  ///
  /// @return Number of erased elements.
  export template <class T, class Alloc, class Pred>
  auto EraseUnstableIf(std::vector<T, Alloc>& v, Pred pred)
  {
    size_t removed = 0;

    for (size_t i = 0; i < v.size();)
    {
      if (std::invoke(pred, v[i]))
      {
        if (i + 1 != v.size()) v[i] = std::move(v.back());

        v.pop_back();
        ++removed;
      }
      else
      {
        ++i;
      }
    }

    return removed;
  }

}
