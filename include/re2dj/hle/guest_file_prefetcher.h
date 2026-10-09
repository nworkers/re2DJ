#ifndef RE2DJ_HLE_GUEST_FILE_PREFETCHER_H_
#define RE2DJ_HLE_GUEST_FILE_PREFETCHER_H_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace re2dj::hle
{

class GuestFileSource;

// Reads image files into memory on a thread of its own (#9), so a guest that
// streams a file in small reads from its main thread (6th's music, 22,528
// bytes every ~133 ms) does not wait on the disk behind the CHD. The worker
// reads each file from the start in pieces and publishes how much it holds;
// a read inside that prefix is served from memory, one past it goes to the
// source as before. The source must allow calls from another thread.
class GuestFilePrefetcher
{
public:
    // One file being read ahead. Shared by the worker and the open handle;
    // the memory goes when both let go.
    class Job
    {
    public:
        const std::string& path() const { return path_; }
        std::uint64_t size() const { return size_; }
        // How many bytes from the start are in memory.
        std::uint64_t loaded() const { return loaded_.load(std::memory_order_acquire); }
        bool failed() const { return failed_.load(std::memory_order_acquire); }
        bool finished() const { return finished_.load(std::memory_order_acquire); }

    private:
        friend class GuestFilePrefetcher;
        std::string path_;
        std::uint64_t size_ = 0;
        std::unique_ptr<std::uint8_t[]> bytes_;
        std::atomic<std::uint64_t> loaded_{0};
        std::atomic<bool> cancelled_{false};
        std::atomic<bool> failed_{false};
        std::atomic<bool> finished_{false};
        // Set once the job's bytes are given back to the budget.
        bool released_ = false;
    };

    // The piece the worker reads at a time: one 6th cluster, so a guest read
    // that needs the same source lock waits for one piece at most.
    static constexpr std::size_t kPieceBytes = 16 * 1024;
    // The memory all unfinished or still open jobs may hold together.
    static constexpr std::uint64_t kDefaultBudgetBytes = 256ull * 1024 * 1024;

    explicit GuestFilePrefetcher(const GuestFileSource* source,
                                 std::uint64_t budget_bytes = kDefaultBudgetBytes);
    ~GuestFilePrefetcher();

    GuestFilePrefetcher(const GuestFilePrefetcher&) = delete;
    GuestFilePrefetcher& operator=(const GuestFilePrefetcher&) = delete;

    // Queues path (size bytes) for reading ahead, or nullptr when it would
    // take the jobs past the budget.
    std::shared_ptr<Job> Start(const std::string& path, std::uint64_t size);
    // Copies [offset, offset + length) when the job holds it.
    static bool Copy(const Job& job, std::uint64_t offset, void* destination, std::size_t length);
    // Stops reading at the next piece and gives the budget back.
    void Cancel(const std::shared_ptr<Job>& job);
    // Waits until the queue is empty and the worker idle (tests).
    void WaitIdle();

private:
    void Run();
    void Release(Job* job);

    const GuestFileSource* source_ = nullptr;
    std::uint64_t budget_bytes_ = 0;
    std::mutex lock_;
    std::condition_variable changed_;
    std::deque<std::shared_ptr<Job>> queue_;
    // The job the worker is reading.
    std::shared_ptr<Job> current_;
    // Bytes held by jobs not yet cancelled.
    std::uint64_t held_bytes_ = 0;
    bool busy_ = false;
    bool stopping_ = false;
    std::thread worker_;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_FILE_PREFETCHER_H_
