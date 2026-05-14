/// Thread pool and job execution utilities.
///
/// This module provides a fixed-size thread pool, task submission helpers,
/// and small job-level utilities built on top of `ixx::Result` and
/// `ixx::oneshot`.
///
/// ## Error handling
///
/// Jobs should report domain failures through `Result<T>` and be submitted
/// with `SubmitResult`.
///
/// `Submit` is intended for tasks that produce plain values and do not use
/// the IXXExtension error channel.
///
/// ## Thread safety
///
/// `Post`, `Submit`, `SubmitResult`, `Stop`, `IsStopped`, `PendingCount`,
/// `WorkerCount`, and `ActiveWorkerCount` may be called concurrently.
///
/// Destruction must not race with other member function calls.
///
/// User-provided task bodies are responsible for their own synchronization.
export module IXXExtension.Jobs;

import IXXExtension.Core;
import IXXExtension.Concurrency;

import std;

namespace ixx
{

  namespace jobs
  {

    /// Job/thread-pool-specific errors.
    export enum class Errc : std::uint16_t
    {
      /// No error.
      None = 0,

      /// The thread pool is stopped and no longer accepts new work.
      Stopped,

      /// A null/empty task was submitted.
      InvalidTask,

      /// `Stop` was called from one of this pool's own worker threads.
      CalledFromWorker,

      /// Grain size for a chunked operation was zero.
      InvalidGrainSize,
    };

  }

  export template <>
  struct ErrorCodeTraits<jobs::Errc>
  {
    static constexpr const char* Name = "cxx.jobs";

    [[nodiscard]] static constexpr auto Message(jobs::Errc code) noexcept -> std::string_view
    {
      using enum jobs::Errc;

      switch (code)
      {
        case None:
          return "No error";
        case Stopped:
          return "Thread pool is stopped";
        case InvalidTask:
          return "Invalid task";
        case CalledFromWorker:
          return "Thread pool stop was called from one of its own workers";
        case InvalidGrainSize:
          return "Invalid grain size";
        default:
          return "Unknown jobs error";
      }
    }
  };

  /// Returns a practical default worker count.
  ///
  /// `std::thread::hardware_concurrency()` is only a hint and may return `0`,
  /// so this function provides a fallback. For `N > 2`, it keeps one logical
  /// thread free for the caller/main thread.
  export [[nodiscard]] auto DefaultWorkerCount() -> std::size_t
  {
    const auto hw = std::thread::hardware_concurrency();

    if (hw == 0)
    {
      return 2;
    }

    if (hw <= 2)
    {
      return 1;
    }

    return static_cast<std::size_t>(hw - 1);
  }

  /// Returns a default chunk size for count-based parallel work.
  ///
  /// The result is always at least `1`. `chunksPerWorker` controls how many
  /// chunks each worker should receive on average; more chunks can improve load
  /// balancing when individual items have uneven cost.
  ///
  /// @param itemCount Number of items in the operation.
  /// @param workerCount Number of workers available to process chunks.
  /// @param chunksPerWorker Target chunks per worker.
  /// @return Suggested grain size for `ParallelFor`.
  export [[nodiscard]] auto DefaultGrainSize(std::size_t itemCount, std::size_t workerCount, std::size_t chunksPerWorker = 4) -> std::size_t
  {
    if (itemCount == 0)
    {
      return 1;
    }

    const std::size_t targetChunks = std::max<std::size_t>(1, workerCount * chunksPerWorker);

    return std::max<std::size_t>(1, (itemCount + targetChunks - 1) / targetChunks);
  }

  namespace Internal
  {

    inline thread_local const void* CurrentThreadPool = nullptr;

    template <class T>
    struct ResultTraits;

    template <class T>
    struct ResultTraits<std::expected<T, Error>>
    {
      using value_type = T;
    };

    template <class T>
    concept ResultType = requires { typename ResultTraits<std::remove_cvref_t<T>>::value_type; };

    template <class T>
    using ResultValueT = typename ResultTraits<std::remove_cvref_t<T>>::value_type;

  }

  namespace jobs
  {

    /// Stop behavior for `ThreadPool::Stop`.
    export enum class StopMode
    {
      /// Reject new tasks, then finish all already queued tasks.
      Drain,

      /// Reject new tasks and drop queued tasks that have not started yet.
      CancelPending,
    };

    /// Fixed-size FIFO thread pool.
    ///
    /// The pool owns a fixed number of worker threads. Submitted tasks are stored
    /// in FIFO order and executed by workers as they become available.
    ///
    /// ## Task contract
    ///
    /// Task bodies should not throw. Domain errors should be returned as
    /// `Result<T>` and submitted with `SubmitResult`.
    ///
    /// ## Stop behavior
    ///
    /// `Stop(Drain)` runs already queued tasks before workers exit.
    /// `Stop(CancelPending)` destroys queued tasks that have not started yet.
    /// Running tasks are not forcibly interrupted.
    export class ThreadPool
    {
  public:

      using Task = std::move_only_function<void()>;

      explicit ThreadPool(std::size_t workerCount = DefaultWorkerCount())
      {
        Start(workerCount == 0 ? 1 : workerCount);
      }

      ThreadPool(const ThreadPool&)                    = delete;
      auto operator=(const ThreadPool&) -> ThreadPool& = delete;

      ThreadPool(ThreadPool&&)                    = delete;
      auto operator=(ThreadPool&&) -> ThreadPool& = delete;

      ~ThreadPool()
      {
        [[maybe_unused]] auto stoppedResult = Stop(StopMode::Drain);
      }

      /// Returns the configured worker count.
      [[nodiscard]] auto WorkerCount() const noexcept -> std::size_t
      {
        return configuredWorkerCount;
      }

      /// Returns the number of workers that have not exited yet.
      [[nodiscard]] auto ActiveWorkerCount() const noexcept -> std::size_t
      {
        return activeWorkers.load(std::memory_order_acquire);
      }

      /// Returns whether the pool has begun stopping.
      [[nodiscard]] auto IsStopped() const noexcept -> bool
      {
        return stopped.load(std::memory_order_acquire);
      }

      /// Returns whether the current thread is one of this pool's workers.
      [[nodiscard]] auto IsCurrentWorker() const noexcept -> bool
      {
        return Internal::CurrentThreadPool == this;
      }

      /// Returns a synchronized snapshot of queued-but-not-started tasks.
      [[nodiscard]] auto PendingCount() const -> std::size_t
      {
        std::lock_guard lock{mutex};
        return tasks.size();
      }

      /// Stops the pool.
      ///
      /// This function is thread-safe when called from external threads.
      ///
      /// Calling `Stop` from one of this pool's own workers is rejected to avoid
      /// self-join/deadlock behavior.
      [[nodiscard]] auto Stop(StopMode mode = StopMode::Drain) -> VoidResult
      {
        if (IsCurrentWorker())
        {
          return std::unexpected{Error::Make(
            Errc::CalledFromWorker,
            "ThreadPool::Stop cannot be called from one of this pool's workers",
            "ixx::jobs::ThreadPool::Stop")};
        }

        {
          std::lock_guard lock{mutex};

          const bool alreadyStopped = stopped.exchange(true, std::memory_order_acq_rel);

          if (alreadyStopped)
          {
            return {};
          }

          if (mode == StopMode::CancelPending)
          {
            tasks.clear();
          }
        }

        cv.notify_all();

        for (auto& worker : workers)
        {
          worker.request_stop();
        }

        workers.clear();

        return {};
      }

      /// Posts a fire-and-forget task.
      ///
      /// The task is accepted if the pool has not been stopped.
      [[nodiscard]] auto Post(Task task) -> VoidResult
      {
        if (!task)
        {
          return std::unexpected{Error::Make(Errc::InvalidTask, "Cannot post an empty task", "ixx::jobs::ThreadPool::Post")};
        }

        {
          std::lock_guard lock{mutex};

          if (stopped.load(std::memory_order_acquire))
          {
            return std::unexpected{Error::Make(Errc::Stopped, "Cannot post a task after ThreadPool::Stop", "ixx::jobs::ThreadPool::Post")};
          }

          tasks.emplace_back(std::move(task));
        }

        cv.notify_one();
        return {};
      }

      /// Submits a task returning a plain value.
      ///
      /// The outer `Result` reports whether the task was accepted by the pool.
      /// The returned receiver reports the eventual task result.
      ///
      /// ## Example
      ///
      /// ```cpp
      /// ixx::ThreadPool pool{2};
      /// auto receiver = pool.Submit([] { return 42; });
      /// auto value = receiver->Wait();
      /// ```
      template <class F>
      requires std::invocable<std::decay_t<F>&>
      [[nodiscard]] auto Submit(F&& function) -> Result<oneshot::Receiver<std::remove_cvref_t<std::invoke_result_t<std::decay_t<F>&>>>>
      {
        using Function = std::decay_t<F>;
        using RawT     = std::invoke_result_t<Function&>;
        using T        = std::remove_cvref_t<RawT>;

        auto channel  = oneshot::Make<T>();
        auto sender   = std::move(channel.first);
        auto receiver = std::move(channel.second);

        Function storedFunction{std::forward<F>(function)};

        auto posted = Post([sender = std::move(sender), fn = std::move(storedFunction)] mutable {
          if constexpr (std::same_as<T, void>)
          {
            std::invoke(fn);
            [[maybe_unused]] auto sent = sender.Send();
          }
          else
          {
            [[maybe_unused]] auto sent = sender.Send(std::invoke(fn));
          }
        });

        if (!posted)
        {
          return std::unexpected{std::move(posted.error())};
        }

        return std::move(receiver);
      }

      /// Submits a task returning `ixx::Result<T>`.
      ///
      /// The outer `Result` reports whether the task was accepted by the pool.
      /// If the task later returns an error, that error is delivered through the
      /// returned receiver.
      ///
      /// ## Example
      ///
      /// ```cpp
      /// auto receiver = pool.SubmitResult([] -> ixx::Result<int> {
      ///   return ixx::ParseInt<int>("42");
      /// });
      /// ```
      template <class F>
      requires std::invocable<std::decay_t<F>&> && Internal::ResultType<std::invoke_result_t<std::decay_t<F>&>>
      [[nodiscard]] auto SubmitResult(F&& function)
        -> Result<oneshot::Receiver<Internal::ResultValueT<std::invoke_result_t<std::decay_t<F>&>>>>
      {
        using Function   = std::decay_t<F>;
        using TaskResult = std::invoke_result_t<Function&>;
        using T          = Internal::ResultValueT<TaskResult>;

        auto channel  = oneshot::Make<T>();
        auto sender   = std::move(channel.first);
        auto receiver = std::move(channel.second);

        Function storedFunction{std::forward<F>(function)};

        auto posted = Post([sender = std::move(sender), fn = std::move(storedFunction)] mutable {
          TaskResult result = std::invoke(fn);

          if (!result)
          {
            [[maybe_unused]] auto rejected = sender.Reject(std::move(result.error()));
            return;
          }

          if constexpr (std::same_as<T, void>)
          {
            [[maybe_unused]] auto sent = sender.Send();
          }
          else
          {
            [[maybe_unused]] auto sent = sender.Send(std::move(*result));
          }
        });

        if (!posted)
        {
          return std::unexpected{std::move(posted.error())};
        }

        return std::move(receiver);
      }

  private:

      auto Start(std::size_t workerCount) -> void
      {
        configuredWorkerCount = workerCount;
        workers.reserve(workerCount);

        for (std::size_t i = 0; i < workerCount; ++i)
        {
          workers.emplace_back([this](std::stop_token stopToken) { WorkerLoop(stopToken); });
        }
      }

      auto WorkerLoop(std::stop_token stopToken) -> void
      {
        activeWorkers.fetch_add(1, std::memory_order_acq_rel);

        const void* previousPool = std::exchange(Internal::CurrentThreadPool, this);

        auto cleanup = ScopeExit{[this, previousPool] {
          Internal::CurrentThreadPool = previousPool;
          activeWorkers.fetch_sub(1, std::memory_order_acq_rel);
        }};

        while (true)
        {
          Task task;

          {
            std::unique_lock lock{mutex};

            cv.wait(lock, stopToken, [this] { return stopped.load(std::memory_order_acquire) || !tasks.empty(); });

            if (tasks.empty())
            {
              return;
            }

            task = std::move(tasks.front());
            tasks.pop_front();
          }

          task();
        }
      }

      mutable std::mutex          mutex{};
      std::condition_variable_any cv{};
      std::deque<Task>            tasks{};
      std::vector<std::jthread>   workers{};

      std::atomic_bool   stopped{false};
      std::atomic_size_t activeWorkers{0};

      std::size_t configuredWorkerCount{0};
    };

  }

  /// Waits for all void receivers.
  ///
  /// Returns the first error encountered. Already-started tasks are not
  /// cancelled when one receiver reports an error.
  export [[nodiscard]] auto WaitAll(std::vector<oneshot::Receiver<void>>& receivers) -> VoidResult
  {
    for (auto& receiver : receivers)
    {
      auto result = receiver.Wait();

      if (!result)
      {
        return std::unexpected{std::move(result.error())};
      }
    }

    return {};
  }

  /// Waits for all receivers and collects their values.
  ///
  /// Returns the first error encountered. Already-started tasks are not
  /// cancelled when one receiver reports an error.
  export template <class T>
  requires(!std::same_as<T, void>)
  [[nodiscard]] auto CollectAll(std::vector<oneshot::Receiver<T>>& receivers) -> Result<std::vector<T>>
  {
    std::vector<T> values;
    values.reserve(receivers.size());

    for (auto& receiver : receivers)
    {
      auto result = receiver.Wait();

      if (!result)
      {
        return std::unexpected{std::move(result.error())};
      }

      values.emplace_back(std::move(*result));
    }

    return values;
  }

  /// Runs `function(index)` for every index in `[0, count)`.
  ///
  /// Work is split into chunks of `grainSize`.
  ///
  /// The function object is shared as const across tasks. If it captures shared
  /// mutable state, that state must be synchronized by the caller.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// std::vector<int> values(1024);
  /// ixx::ParallelFor(pool, values.size(), 64, [&](std::size_t i) {
  ///   values[i] = static_cast<int>(i);
  /// });
  /// ```
  export template <class F>
  requires std::invocable<const std::decay_t<F>&, std::size_t> &&
           std::same_as<std::invoke_result_t<const std::decay_t<F>&, std::size_t>, void>
  [[nodiscard]] auto ParallelFor(jobs::ThreadPool& pool, std::size_t count, std::size_t grainSize, F&& function) -> VoidResult
  {

    if (pool.IsCurrentWorker())
    {
      return std::unexpected{
          Error::Make(jobs::Errc::CalledFromWorker, "ParallelFor cannot be called from a worker of the same pool", "ixx::jobs::ParallelFor")
      };
    }

    if (count == 0)
    {
      return {};
    }

    if (grainSize == 0)
    {
      return std::unexpected{
          Error::Make(jobs::Errc::InvalidGrainSize, "ParallelFor grain size must be greater than zero", "ixx::jobs::ParallelFor")
      };
    }

    using Function = std::decay_t<F>;

    auto sharedFunction = std::make_shared<Function>(std::forward<F>(function));

    const std::size_t chunkCount = 1 + ((count - 1) / grainSize);

    std::vector<oneshot::Receiver<void>> receivers;
    receivers.reserve(chunkCount);

    for (std::size_t chunk = 0; chunk < chunkCount; ++chunk)
    {
      const std::size_t begin = chunk * grainSize;
      const std::size_t end   = std::min(count, begin + grainSize);

      auto submitted = pool.Submit([sharedFunction, begin, end] {
        for (std::size_t i = begin; i < end; ++i)
        {
          std::invoke(std::as_const(*sharedFunction), i);
        }
      });

      if (!submitted)
      {
        return std::unexpected{std::move(submitted.error())};
      }

      receivers.emplace_back(std::move(*submitted));
    }

    return WaitAll(receivers);
  }

  /// Runs `function(index) -> VoidResult` for every index in `[0, count)`.
  ///
  /// Work is split into chunks of `grainSize`.
  ///
  /// Returns the first task error observed while waiting for chunk results.
  /// Already-started chunks are not cancelled automatically.
  ///
  /// ## Example
  ///
  /// ```cpp
  /// auto result = ixx::ParallelForResult(pool, paths.size(), 8, [&](std::size_t i) -> ixx::VoidResult {
  ///   return ValidatePath(paths[i]);
  /// });
  /// ```
  export template <class F>
  requires std::invocable<const std::decay_t<F>&, std::size_t> &&
           std::same_as<std::invoke_result_t<const std::decay_t<F>&, std::size_t>, VoidResult>
  [[nodiscard]] auto ParallelForResult(jobs::ThreadPool& pool, std::size_t count, std::size_t grainSize, F&& function) -> VoidResult
  {

    if (pool.IsCurrentWorker())
    {
      return std::unexpected{
          Error::Make(jobs::Errc::CalledFromWorker, "ParallelFor cannot be called from a worker of the same pool", "ixx::jobs::ParallelForResult")
      };
    }

    if (count == 0)
    {
      return {};
    }

    if (grainSize == 0)
    {
      return std::unexpected{
          Error::Make(jobs::Errc::InvalidGrainSize, "ParallelForResult grain size must be greater than zero", "ixx::jobs::ParallelForResult")
      };
    }

    using Function = std::decay_t<F>;

    auto sharedFunction = std::make_shared<Function>(std::forward<F>(function));

    const std::size_t chunkCount = 1 + ((count - 1) / grainSize);

    std::vector<oneshot::Receiver<void>> receivers;
    receivers.reserve(chunkCount);

    for (std::size_t chunk = 0; chunk < chunkCount; ++chunk)
    {
      const std::size_t begin = chunk * grainSize;
      const std::size_t end   = std::min(count, begin + grainSize);

      auto submitted = pool.SubmitResult([sharedFunction, begin, end] -> VoidResult {
        for (std::size_t i = begin; i < end; ++i)
        {
          auto result = std::invoke(std::as_const(*sharedFunction), i);

          if (!result)
          {
            return std::unexpected{std::move(result.error())};
          }
        }

        return {};
      });

      if (!submitted)
      {
        return std::unexpected{std::move(submitted.error())};
      }

      receivers.emplace_back(std::move(*submitted));
    }

    return WaitAll(receivers);
  }

  /// Runs `function(index)` for every index using `DefaultGrainSize`.
  export template <class F>
  requires std::invocable<const std::decay_t<F>&, std::size_t> &&
           std::same_as<std::invoke_result_t<const std::decay_t<F>&, std::size_t>, void>
  [[nodiscard]] auto ParallelFor(jobs::ThreadPool& pool, std::size_t count, F&& function) -> VoidResult
  {
    return ParallelFor(pool, count, DefaultGrainSize(count, pool.WorkerCount()), std::forward<F>(function));
  }

  /// Runs `function(index) -> VoidResult` for every index using `DefaultGrainSize`.
  export template <class F>
  requires std::invocable<const std::decay_t<F>&, std::size_t> &&
           std::same_as<std::invoke_result_t<const std::decay_t<F>&, std::size_t>, VoidResult>
  [[nodiscard]] auto ParallelForResult(jobs::ThreadPool& pool, std::size_t count, F&& function) -> VoidResult
  {
    return ParallelForResult(pool, count, DefaultGrainSize(count, pool.WorkerCount()), std::forward<F>(function));
  }

  /// Convenience alias for `ixx::jobs::ThreadPool`.
  export using ThreadPool = jobs::ThreadPool;

}
