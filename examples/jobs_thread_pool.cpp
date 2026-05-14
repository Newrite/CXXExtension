import IXXExtension;
import std;

auto main() -> int
{
  ixx::ThreadPool pool{2};

  auto answer = pool.Submit([] {
    return 21 * 2;
  });

  if (!answer) return 1;

  auto answerValue = answer->Wait();
  if (!answerValue || *answerValue != 42) return 1;

  std::vector<int> values(128);

  auto filled = ixx::ParallelFor(pool, values.size(), [&](std::size_t index) {
    values[index] = static_cast<int>(index);
  });

  if (!filled) return 1;

  auto sum = pool.SubmitResult([&values] -> ixx::Result<int> {
    int total = 0;

    for (const int value : values)
    {
      total += value;
    }

    return total;
  });

  if (!sum) return 1;

  auto sumValue = sum->Wait();
  return sumValue && *sumValue == 8128 ? 0 : 1;
}
