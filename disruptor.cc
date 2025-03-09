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
