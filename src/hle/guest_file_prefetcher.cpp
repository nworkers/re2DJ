#include "re2dj/hle/guest_file_prefetcher.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <new>

#include <spdlog/logger.h>

#include "re2dj/hle/guest_files.h"
#include "re2dj/logging/logging.h"

namespace re2dj::hle
{

GuestFilePrefetcher::GuestFilePrefetcher(const GuestFileSource* source, std::uint64_t budget_bytes)
    : source_(source), budget_bytes_(budget_bytes)
{
}

GuestFilePrefetcher::~GuestFilePrefetcher()
{
    {
        const std::lock_guard<std::mutex> guard(lock_);
        stopping_ = true;
        for (const std::shared_ptr<Job>& job : queue_)
        {
            job->cancelled_.store(true, std::memory_order_release);
        }
        queue_.clear();
        if (current_ != nullptr)
        {
            current_->cancelled_.store(true, std::memory_order_release);
        }
    }
    changed_.notify_all();
    if (worker_.joinable())
    {
        worker_.join();
    }
}

std::shared_ptr<GuestFilePrefetcher::Job> GuestFilePrefetcher::Start(const std::string& path, std::uint64_t size)
{
    if (source_ == nullptr || size == 0)
    {
        return nullptr;
    }
    auto job = std::make_shared<Job>();
    job->path_ = path;
    job->size_ = size;
    {
        const std::lock_guard<std::mutex> guard(lock_);
        if (stopping_ || size > budget_bytes_ - std::min(held_bytes_, budget_bytes_))
        {
            return nullptr;
        }
        held_bytes_ += size;
        queue_.push_back(job);
        if (!worker_.joinable())
        {
            worker_ = std::thread([this] { Run(); });
        }
    }
    changed_.notify_all();
    const std::shared_ptr<spdlog::logger> logger = logging::GetLogger();
    if (logger != nullptr)
    {
        logger->info("files: prefetch {} ({} bytes)", path, size);
    }
    return job;
}

bool GuestFilePrefetcher::Copy(const Job& job, std::uint64_t offset, void* destination, std::size_t length)
{
    const std::uint64_t loaded = job.loaded();
    if (offset > loaded || length > loaded - offset)
    {
        return false;
    }
    if (length != 0)
    {
        std::memcpy(destination, job.bytes_.get() + offset, length);
    }
    return true;
}

void GuestFilePrefetcher::Cancel(const std::shared_ptr<Job>& job)
{
    if (job == nullptr)
    {
        return;
    }
    job->cancelled_.store(true, std::memory_order_release);
    const std::lock_guard<std::mutex> guard(lock_);
    Release(job.get());
}

void GuestFilePrefetcher::WaitIdle()
{
    std::unique_lock<std::mutex> guard(lock_);
    changed_.wait(guard, [this] { return queue_.empty() && !busy_; });
}

void GuestFilePrefetcher::Release(Job* job)
{
    if (!job->released_)
    {
        job->released_ = true;
        held_bytes_ -= std::min(held_bytes_, job->size_);
    }
}

void GuestFilePrefetcher::Run()
{
    for (;;)
    {
        std::shared_ptr<Job> job;
        {
            std::unique_lock<std::mutex> guard(lock_);
            busy_ = false;
            current_.reset();
            changed_.notify_all();
            changed_.wait(guard, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_)
            {
                return;
            }
            job = std::move(queue_.front());
            queue_.pop_front();
            current_ = job;
            busy_ = true;
        }
        if (job->cancelled_.load(std::memory_order_acquire))
        {
            continue;
        }
        const auto started = std::chrono::steady_clock::now();
        job->bytes_.reset(new (std::nothrow) std::uint8_t[static_cast<std::size_t>(job->size_)]);
        bool failed = job->bytes_ == nullptr;
        std::uint64_t offset = 0;
        while (!failed && offset < job->size_ && !job->cancelled_.load(std::memory_order_acquire))
        {
            const auto length = static_cast<std::size_t>(std::min<std::uint64_t>(kPieceBytes, job->size_ - offset));
            if (!source_->ReadRange(job->path_, offset, job->bytes_.get() + offset, length))
            {
                failed = true;
                break;
            }
            offset += length;
            job->loaded_.store(offset, std::memory_order_release);
        }
        job->failed_.store(failed, std::memory_order_release);
        job->finished_.store(true, std::memory_order_release);
        const std::shared_ptr<spdlog::logger> logger = logging::GetLogger();
        if (logger == nullptr)
        {
            continue;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
        if (failed)
        {
            logger->warn("files: prefetch of {} failed at byte {}; reads go to the image", job->path_, offset);
        }
        else if (offset < job->size_)
        {
            logger->info("files: prefetch of {} stopped at {} of {} bytes (closed)", job->path_, offset, job->size_);
        }
        else
        {
            logger->info("files: prefetched {} in {} ms", job->path_, elapsed.count());
        }
    }
}

}  // namespace re2dj::hle
