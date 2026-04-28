export module CXXExtension.ContainerExtension;

import std;

namespace cxx {

    export template <std::ranges::input_range R>
    [[nodiscard]]
    auto ToVector(R&& range)
    {
        using T = std::ranges::range_value_t<R>;

        return std::forward<R>(range)
            | std::ranges::to<std::vector<T>>();
    }

    export template <class T, std::ranges::input_range R>
    [[nodiscard]]
    auto ToVector(R&& range) -> std::vector<T>
    {
        return std::forward<R>(range)
            | std::ranges::to<std::vector<T>>();
    }

    export template <std::ranges::input_range R, class T>
    auto Contains(R&& range, const T& value) -> bool
    {
        return std::ranges::contains(range, value);
    }

    export template <std::ranges::input_range R, class T>
    auto Find(R&& range, const T& value)
    {
        return std::ranges::find(range, value);
    }

    export template <std::ranges::input_range R, class Pred>
    auto FindIf(R&& range, Pred pred)
    {
        return std::ranges::find_if(range, pred);
    }

    export template <std::ranges::random_access_range R, class T>
    auto IndexOf(R&& range, const T& value) -> std::optional<size_t>
    {
        const auto it = std::ranges::find(range, value);

        if (it == std::ranges::end(range))
            return std::nullopt;

        return static_cast<size_t>(std::ranges::distance(std::ranges::begin(range), it));
    }

    export template <std::ranges::random_access_range R, class Pred>
    auto IndexOfIf(R&& range, Pred pred) -> std::optional<size_t>
    {
        const auto it = std::ranges::find_if(range, pred);

        if (it == std::ranges::end(range))
            return std::nullopt;

        return static_cast<size_t>(std::ranges::distance(std::ranges::begin(range), it));
    }

    export template <std::ranges::contiguous_range R, class T>
    auto FindPtr(R& range, const T& value) -> std::ranges::range_value_t<R>*
    {
        auto it = std::ranges::find(range, value);

        if (it == std::ranges::end(range))
            return nullptr;

        return std::addressof(*it);
    }

    export template <std::ranges::contiguous_range R, class Pred>
    auto FindIfPtr(R& range, Pred pred) -> std::ranges::range_value_t<R>*
    {
        auto it = std::ranges::find_if(range, pred);

        if (it == std::ranges::end(range))
            return nullptr;

        return std::addressof(*it);
    }

    export template <std::ranges::contiguous_range R, class Pred>
    auto FindIfPtr(const R& range, Pred pred) -> const std::ranges::range_value_t<R>*
    {
        auto it = std::ranges::find_if(range, pred);

        if (it == std::ranges::end(range))
            return nullptr;

        return std::addressof(*it);
    }

    export template <class Map, class Key>
    auto FindValuePtr(Map& map, const Key& key) -> typename Map::mapped_type*
    {
        auto it = map.find(key);

        if (it == map.end())
            return nullptr;

        return std::addressof(it->second);
    }

    export template <class Map, class Key>
    auto FindValuePtr(const Map& map, const Key& key) -> const typename Map::mapped_type*
    {
        auto it = map.find(key);

        if (it == map.end())
            return nullptr;

        return std::addressof(it->second);
    }

    export template <class T, class Alloc, class U>
    auto EraseFirst(std::vector<T, Alloc>& v, const U& value) -> bool
    {
        const auto it = std::ranges::find(v, value);

        if (it == v.end())
            return false;

        v.erase(it);
        return true;
    }

    export template <class T, class Alloc, class Pred>
    auto EraseFirstIf(std::vector<T, Alloc>& v, Pred pred) -> bool
    {
        const auto it = std::ranges::find_if(v, pred);

        if (it == v.end())
            return false;

        v.erase(it);
        return true;
    }

    export template <class T, class Alloc>
    auto EraseFast(std::vector<T, Alloc>& v, size_t index) -> bool
    {
        if (index >= v.size())
            return false;

        if (index + 1 != v.size())
            v[index] = std::move(v.back());

        v.pop_back();
        return true;
    }

    export template <class T, class Alloc, class U>
    auto EraseFastFirst(std::vector<T, Alloc>& v, const U& value) -> bool
    {
        const auto it = std::ranges::find(v, value);

        if (it == v.end())
            return false;

        const auto index = static_cast<size_t>(std::distance(v.begin(), it));
        return EraseFast(v, index);
    }

    export template <class T, class Alloc, class Pred>
    auto EraseFastFirstIf(std::vector<T, Alloc>& v, Pred pred) -> bool
    {
        const auto it = std::ranges::find_if(v, pred);

        if (it == v.end())
            return false;

        const auto index = static_cast<size_t>(std::distance(v.begin(), it));
        return EraseFast(v, index);
    }

    export template <class T, class Alloc, class U>
    auto PushUnique(std::vector<T, Alloc>& v, U&& value) -> bool
    {
        if (Contains(v, value))
            return false;

        v.emplace_back(std::forward<U>(value));
        return true;
    }

    export template <class T, class Alloc>
    auto PopBack(std::vector<T, Alloc>& v) -> std::optional<T>
    {
        if (v.empty())
            return std::nullopt;

        T value = std::move(v.back());
        v.pop_back();
        return value;
    }

    export template <class T, class Alloc>
    auto AtPtr(std::vector<T, Alloc>& v, size_t index) -> T*
    {
        if (index >= v.size())
            return nullptr;

        return &v[index];
    }

    export template <class T, class Alloc>
    auto AtPtr(const std::vector<T, Alloc>& v, size_t index) -> const T*
    {
        if (index >= v.size())
            return nullptr;

        return &v[index];
    }

    export template <class Map>
    auto Keys(const Map& map)
    {
        std::vector<typename Map::key_type> result;
        result.reserve(map.size());

        for (const auto& [key, value] : map)
            result.push_back(key);

        return result;
    }

    export template <class Map>
    auto Values(const Map& map)
    {
        std::vector<typename Map::mapped_type> result;
        result.reserve(map.size());

        for (const auto& [key, value] : map)
            result.push_back(value);

        return result;
    }

    export template <class Map, class Key>
    auto GetOrNull(Map& map, const Key& key) -> typename Map::mapped_type*
    {
        auto it = map.find(key);

        if (it == map.end())
            return nullptr;

        return std::addressof(it->second);
    }

    export template <class Map, class Key>
    auto GetOrNull(const Map& map, const Key& key) -> const typename Map::mapped_type*
    {
        auto it = map.find(key);

        if (it == map.end())
            return nullptr;

        return std::addressof(it->second);
    }

    export template <class Map, class Key, class Default>
    auto GetOrDefault(const Map& map, const Key& key, Default&& fallback)
    {
        auto it = map.find(key);

        if (it == map.end())
            return std::forward<Default>(fallback);

        return it->second;
    }

    export template <class Map, class Key, class... Args>
    auto GetOrEmplace(Map& map, Key&& key, Args&&... args) -> typename Map::mapped_type&
    {
        auto [it, inserted] = map.try_emplace(
            std::forward<Key>(key),
            std::forward<Args>(args)...
        );

        return it->second;
    }

    export template <std::ranges::input_range R, class T, class Proj>
    auto FindBy(R&& range, const T& value, Proj proj)
    {
        return std::ranges::find(range, value, proj);
    }

    export template <std::ranges::input_range R, class T, class Proj>
    auto ContainsBy(R&& range, const T& value, Proj proj) -> bool
    {
        return std::ranges::find(range, value, proj) != std::ranges::end(range);
    }

    export template <class T, class Alloc, class Pred>
    auto TakeFirstIf(std::vector<T, Alloc>& v, Pred pred) -> std::optional<T>
    {
        auto it = std::ranges::find_if(v, pred);

        if (it == v.end())
            return std::nullopt;

        T value = std::move(*it);
        v.erase(it);
        return value;
    }

    export template <class T, class Alloc, class Pred>
    auto EraseUnstableIf(std::vector<T, Alloc>& v, Pred pred) -> size_t
    {
        size_t removed = 0;

        for (size_t i = 0; i < v.size();) {
            if (std::invoke(pred, v[i])) {
                if (i + 1 != v.size())
                    v[i] = std::move(v.back());

                v.pop_back();
                ++removed;
            } else {
                ++i;
            }
        }

        return removed;
    }

}