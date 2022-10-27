import lockfree_queue;
import concurrency;
import platform;
import logger;

import <cstdlib>;
import <iostream>;
import <atomic>;
import <future>;
import <exception>;
import <set>;
import <mutex>;
import <queue>;


constexpr int kProducerCount = 32;
constexpr int kConsumerCount = 32;
constexpr int kNumValues = 10000000;

static std::atomic_int produced{};
static std::atomic_int consumed{};

template <typename Q, typename T>
concept Queue = requires(Q queue, T value)
{
    {queue.Enqueue(value)} -> std::same_as<void>;
    {queue.Dequeue()} -> std::same_as<std::optional<T>>;
};

template <typename T>
class BlockingQueue
{
private:

    std::mutex    m_mutex{};
    std::queue<T> m_queue{};

public:

    template <typename U>
    void Enqueue(U&& value)
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        m_queue.push(std::forward<U>(value));
    }

    std::optional<T> Dequeue()
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        if(m_queue.empty())
            return std::nullopt;
        auto&& ret = std::make_optional(m_queue.front());
        m_queue.pop();
        return ret;
    }
};

void assert(bool predicate, int line, std::string message = "")
{
    if(!predicate) [[unlikely]] {
        pe::dbgprint("Failed assert on line", line, 
            message.size() ? ":" : "", message);
        std::terminate();
    }
}

template <Queue<int> QueueType>
void producer(QueueType& queue)
{
    std::atomic_thread_fence(std::memory_order_acquire);
    while(produced.load(std::memory_order_relaxed) < kNumValues) {

        int expected = produced.load(std::memory_order_relaxed);
        do{
            if(expected == kNumValues)
                return;
        }while(!produced.compare_exchange_weak(expected, expected + 1,
            std::memory_order_release, std::memory_order_relaxed));

        queue.Enqueue(expected);
    }
}

template <Queue<int> InQueue, Queue<int> OutQueue>
void consumer(InQueue& queue, OutQueue& result)
{
    std::atomic_thread_fence(std::memory_order_acquire);
    while(consumed.load(std::memory_order_relaxed) < kNumValues) {

        auto elem = queue.Dequeue();
        if(!elem.has_value())
            continue;
        result.Enqueue(*elem);

        int expected = consumed.load(std::memory_order_relaxed);
        while(!consumed.compare_exchange_weak(expected, consumed + 1,
            std::memory_order_release, std::memory_order_relaxed));
    }
}

template <Queue<int> InQueue, Queue<int> OutQueue>
void test(InQueue& queue, OutQueue& result)
{
    std::vector<std::future<void>> tasks{};

    for(int i = 0; i < kProducerCount; i++) {
        tasks.push_back(
            std::async(std::launch::async, producer<decltype(queue)>, 
            std::ref(queue)));
    }
    for(int i = 0; i < kConsumerCount; i++) {
        tasks.push_back(std::async(std::launch::async, 
            consumer<decltype(queue), decltype(result)>, 
            std::ref(queue), std::ref(result)));
    }

    for(const auto& task : tasks) {
        task.wait();
    }
}

template <Queue<int> QueueType>
void verify(QueueType& result)
{
    std::set<int> set{};
    std::optional<int> elem;
    do{
        elem = result.Dequeue();
        if(elem.has_value())
            set.insert(elem.value());
    }while(elem.has_value());

    assert(set.size() == kNumValues, __LINE__);

    int current = 0;
    for(auto it = set.cbegin(); it != set.cend(); it++) {
        assert(*it == current, __LINE__);
        current++;
    }
}

int main()
{
    int ret = EXIT_SUCCESS;

    try{

        pe::dbgprint("Starting multi-producer multi-consumer test.");

        auto& lockfree_queue = pe::LockfreeQueue<int, 0>::Instance();
        auto& lockfree_result = pe::LockfreeQueue<int, 1>::Instance();

        pe::dbgtime([&](){
            test(lockfree_queue, lockfree_result);
        }, [&](uint64_t delta) {
            verify(lockfree_result);
            pe::dbgprint("Lockfree queue test with", kProducerCount, "producer(s),",
                kConsumerCount, "consumer(s) and", kNumValues, "value(s) took",
                pe::rdtsc_usec(delta), "microseconds.");
        });

        produced.store(0, std::memory_order_relaxed);
        consumed.store(0, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);

        BlockingQueue<int> blocking_queue{};
        BlockingQueue<int> blocking_result{};

        pe::dbgtime([&](){
            test(blocking_queue, blocking_result);
        }, [&](uint64_t delta) {
            verify(blocking_result);
            pe::dbgprint("Blocking queue test with", kProducerCount, "producer(s),",
                kConsumerCount, "consumer(s) and", kNumValues, "value(s) took",
                pe::rdtsc_usec(delta), "microseconds.");
        });

        pe::dbgprint("Finished multi-producer multi-consumer test.");

    }catch(std::exception &e){

        std::cerr << "Unhandled std::exception: " << e.what() << std::endl;
        ret = EXIT_FAILURE;

    }catch(...){

        std::cerr << "Unknown unhandled exception." << std::endl;
        ret = EXIT_FAILURE;
    }

    return ret;
}
