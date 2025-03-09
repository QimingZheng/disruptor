#include <assert.h>
#include <sys/time.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

constexpr int64_t kDefaultInitialCursorSequence = 0L;
constexpr int kCacheLineWidth = 128;

class Sequence {
 public:
  explicit Sequence(
      int64_t initial_sequence_number = kDefaultInitialCursorSequence)
      : sequence_(initial_sequence_number) {}
  Sequence(const Sequence&) = delete;
  Sequence& operator=(const Sequence&) = delete;
  Sequence(Sequence&&) = delete;
  Sequence& operator=(Sequence&&) = delete;

  int64_t Get() const { return sequence_.load(std::memory_order_acquire); }
  int64_t Increment(int64_t delta) {
    return sequence_.fetch_add(delta, std::memory_order_release) + delta;
  }

 private:
  std::atomic<int64_t> sequence_;
  int64_t _post_pad[7];
};

constexpr size_t kDefaultBufferSize = 1024;

template <typename T, size_t N = kDefaultBufferSize>
class RingBuffer {
 public:
  static_assert(N > 0);
  static_assert((N & (~N + 1)) == N);

  RingBuffer(const std::array<T, N>& events) : events_(events) {}
  RingBuffer() {}

  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;
  RingBuffer(RingBuffer&&) = delete;
  RingBuffer& operator=(RingBuffer&&) = delete;

  T& operator[](const int64_t sequence) { return events_[sequence % N]; }
  const T operator[](const int64_t sequence) const {
    return events_[sequence % N];
  }

 private:
  std::array<T, N> events_;
};

template <size_t N>
class Sequencer {
 public:
  int64_t ClaimToConsunme(int64_t batch) {
    auto new_consumer_claimed_cursor =
        consumer_claimed_cursor_.Increment(batch);
    while (new_consumer_claimed_cursor > producer_cursor_.Get()) {
      std::this_thread::yield();
    }
    return new_consumer_claimed_cursor;
  }

  int64_t ClaimToProduce(int64_t batch) {
    producer_claimed_cursor_.Get();
    auto new_producer_claimed_cursor =
        producer_claimed_cursor_.Increment(batch);
    while (consumer_cursor_.Get() + N < new_producer_claimed_cursor) {
      std::this_thread::yield();
    }
    return new_producer_claimed_cursor;
  }

  void CommitConsume(int64_t sequence, int64_t batch) {
    while (consumer_cursor_.Get() + batch < sequence) {
      std::this_thread::yield();
    }
    consumer_cursor_.Increment(batch);
  }

  void CommitProduce(int64_t sequence, int64_t batch) {
    while (producer_cursor_.Get() + batch < sequence) {
      std::this_thread::yield();
    }
    producer_cursor_.Increment(batch);
  }

 private:
  Sequence consumer_cursor_, consumer_claimed_cursor_;
  Sequence producer_cursor_, producer_claimed_cursor_;
};

template <typename T, size_t N>
class MPMCQueue {
 public:
  std::vector<T> Consume(int batch) {
    std::vector<T> ret;
    auto sequence = sequencer_.ClaimToConsunme(batch);
    for (auto i = 0; i < batch; i++)
      ret.push_back(ring_buffer_[sequence - batch + i]);
    sequencer_.CommitConsume(sequence, batch);
    return ret;
  }

  void Produce(std::vector<T> elem) {
    auto sequence = sequencer_.ClaimToProduce(elem.size());
    for (auto i = 0; i < elem.size(); i++)
      ring_buffer_[sequence - elem.size() + i] = elem[i];
    sequencer_.CommitProduce(sequence, elem.size());
  }

 private:
  RingBuffer<T, N> ring_buffer_;
  Sequencer<N> sequencer_;
};
