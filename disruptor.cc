/// MIT License
///
/// Copyright (c) 2025 Qiming Zheng
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to
/// deal in the Software without restriction, including without limitation the
/// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
/// sell copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
/// IN THE SOFTWARE.

#include "disruptor.h"

using Event = int;

std::atomic<int64_t> actual_sum(0);
std::atomic<int64_t> ground_truth_sum(0);

constexpr int kLoopRange = 1000000L;
constexpr size_t kBufferSize = 128;
constexpr auto kConsumerNum = 4, kProducerNum = 4;

template <size_t N>
void Produce(MPMCQueue<Event, N>& queue) {
  auto sum = 0L;
  for (int i = 0; i < kLoopRange; i++) {
    queue.Produce({i * 2, i * 2 + 1});
    sum += i * 2 + i * 2 + 1;
  }
  ground_truth_sum.fetch_add(sum);
}

template <size_t N>
void Consume(MPMCQueue<Event, N>& queue) {
  auto sum = 0L;
  int last_num = -1;
  for (int i = 0; i < kLoopRange; i++) {
    auto ret = queue.Consume(2);
    sum += ret[0] + ret[1];
  }
  actual_sum.fetch_add(sum);
}

int main() {
  MPMCQueue<Event, kBufferSize> que;

  std::vector<std::thread> consumers;
  for (int i = 0; i < kConsumerNum; i++)
    consumers.push_back(std::thread(Consume<kBufferSize>, std::ref(que)));

  struct timeval start, end;
  gettimeofday(&start, NULL);

  std::vector<std::thread> producers;
  for (int i = 0; i < kProducerNum; i++)
    producers.push_back(std::thread(Produce<kBufferSize>, std::ref(que)));

  for (auto& consumer : consumers) consumer.join();
  gettimeofday(&end, NULL);

  for (auto& producer : producers) producer.join();

  assert(ground_truth_sum == actual_sum);

  std::cout << kLoopRange * kConsumerNum * 2.0 /
                   ((end.tv_sec - start.tv_sec) * 1000000L + end.tv_usec -
                    start.tv_usec)
            << "/us\n";
  return 0;
}
