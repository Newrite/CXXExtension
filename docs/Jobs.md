# Jobs

`IXXExtension.Jobs` provides a fixed-size FIFO thread pool and count-based
parallel helpers.

Use `ixx::ThreadPool` for task submission. `Submit` is for plain values.
`SubmitResult` is for tasks that already report domain failures through
`ixx::Result<T>`.

```cpp
ixx::ThreadPool pool{2};

auto submitted = pool.Submit([] {
  return 21 * 2;
});

if (!submitted) return 1;

auto value = submitted->Wait();
```

`ParallelFor` splits `[0, count)` into chunks and waits for all submitted chunk
tasks. The overload without an explicit grain size uses `DefaultGrainSize`.

```cpp
std::vector<int> values(1024);

auto result = ixx::ParallelFor(pool, values.size(), [&](std::size_t i) {
  values[i] = static_cast<int>(i);
});

if (!result) return 1;
```

`ParallelForResult` is the fallible variant. It returns the first observed
`ixx::Error`; already-started chunks are not automatically cancelled.
