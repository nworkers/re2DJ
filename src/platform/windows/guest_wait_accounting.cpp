#define NOMINMAX
#include <windows.h>

#include <cmath>

#include "guest_wait_accounting.h"

namespace re2dj::platform::windows
{
namespace
{

// Variance from running sums, floored at zero. The textbook form subtracts two
// nearly equal quantities, so at a small spread around a large mean it can
// land just below zero on rounding alone; a negative variance would then
// become a NaN deviation and read as a broken measurement rather than a
// steady one.
double VarianceOf(double sum, double sum_of_squares, double count)
{
    const double mean = sum / count;
    const double variance = sum_of_squares / count - mean * mean;
    return variance > 0.0 ? variance : 0.0;
}

// One lock rather than atomics per field: a summary must be internally
// consistent, and a torn read across counters would be worse than the cost of
// an uncontended shared lock. The recording side holds it for a few increments
// and never calls out while holding it.
SRWLOCK g_lock = SRWLOCK_INIT;

struct Accumulator
{
    std::uint32_t sleep_calls = 0;
    double sleep_milliseconds = 0.0;
    double requested_sleep_milliseconds = 0.0;
    graphics::PresentIntervalHistogram sleep_durations;

    std::uint32_t wait_calls = 0;
    double wait_milliseconds = 0.0;
    graphics::PresentIntervalHistogram wait_durations;

    std::uint32_t time_query_calls = 0;

    // Running sums over (requested sleep, preceding non-sleeping time) pairs.
    // x is the request, y the work before it.
    std::uint32_t model_pairs = 0;
    double model_sum_x = 0.0;
    double model_sum_y = 0.0;
    double model_sum_xx = 0.0;
    double model_sum_yy = 0.0;
    double model_sum_xy = 0.0;

    bool empty() const
    {
        return sleep_calls == 0 && wait_calls == 0 && time_query_calls == 0;
    }

    void ResetWaits()
    {
        sleep_calls = 0;
        sleep_milliseconds = 0.0;
        requested_sleep_milliseconds = 0.0;
        sleep_durations.Reset();
        wait_calls = 0;
        wait_milliseconds = 0.0;
        wait_durations.Reset();
        time_query_calls = 0;
    }

    // Cleared separately from the wait counters above because the two
    // summaries are taken one after the other: a shared reset would hand the
    // second caller an emptied accumulator.
    void ResetModel()
    {
        model_pairs = 0;
        model_sum_x = 0.0;
        model_sum_y = 0.0;
        model_sum_xx = 0.0;
        model_sum_yy = 0.0;
        model_sum_xy = 0.0;
    }
};

Accumulator g_accumulator;

}  // namespace

void RecordGuestSleep(double measured_milliseconds,
                     unsigned requested_milliseconds,
                     double awake_milliseconds)
{
    const double requested = static_cast<double>(requested_milliseconds);
    AcquireSRWLockExclusive(&g_lock);
    ++g_accumulator.sleep_calls;
    g_accumulator.sleep_milliseconds += measured_milliseconds;
    g_accumulator.requested_sleep_milliseconds += requested;
    g_accumulator.sleep_durations.Add(measured_milliseconds);
    if (awake_milliseconds >= 0.0)
    {
        ++g_accumulator.model_pairs;
        g_accumulator.model_sum_x += requested;
        g_accumulator.model_sum_y += awake_milliseconds;
        g_accumulator.model_sum_xx += requested * requested;
        g_accumulator.model_sum_yy += awake_milliseconds * awake_milliseconds;
        g_accumulator.model_sum_xy += requested * awake_milliseconds;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void RecordGuestWait(double measured_milliseconds)
{
    AcquireSRWLockExclusive(&g_lock);
    ++g_accumulator.wait_calls;
    g_accumulator.wait_milliseconds += measured_milliseconds;
    g_accumulator.wait_durations.Add(measured_milliseconds);
    ReleaseSRWLockExclusive(&g_lock);
}

void RecordGuestTimeQuery()
{
    AcquireSRWLockExclusive(&g_lock);
    ++g_accumulator.time_query_calls;
    ReleaseSRWLockExclusive(&g_lock);
}

bool TakeGuestWaitSummary(GuestWaitSummary* summary)
{
    if (summary == nullptr)
    {
        return false;
    }
    AcquireSRWLockExclusive(&g_lock);
    const bool empty = g_accumulator.empty();
    if (!empty)
    {
        summary->sleep_calls = g_accumulator.sleep_calls;
        summary->sleep_milliseconds = g_accumulator.sleep_milliseconds;
        summary->requested_sleep_milliseconds = g_accumulator.requested_sleep_milliseconds;
        summary->sleep_durations = g_accumulator.sleep_durations.Summarize();
        summary->wait_calls = g_accumulator.wait_calls;
        summary->wait_milliseconds = g_accumulator.wait_milliseconds;
        summary->wait_durations = g_accumulator.wait_durations.Summarize();
        summary->time_query_calls = g_accumulator.time_query_calls;
        g_accumulator.ResetWaits();
    }
    ReleaseSRWLockExclusive(&g_lock);
    return !empty;
}

bool TakeGuestSleepModelSummary(GuestSleepModelSummary* summary)
{
    if (summary == nullptr)
    {
        return false;
    }
    AcquireSRWLockExclusive(&g_lock);
    const std::uint32_t pairs = g_accumulator.model_pairs;
    const double count = static_cast<double>(pairs);
    const double sum_x = g_accumulator.model_sum_x;
    const double sum_y = g_accumulator.model_sum_y;
    const double sum_xx = g_accumulator.model_sum_xx;
    const double sum_yy = g_accumulator.model_sum_yy;
    const double sum_xy = g_accumulator.model_sum_xy;
    if (pairs >= 2)
    {
        g_accumulator.ResetModel();
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (pairs < 2)
    {
        return false;
    }

    const double mean_x = sum_x / count;
    const double mean_y = sum_y / count;
    const double variance_x = VarianceOf(sum_x, sum_xx, count);
    const double variance_y = VarianceOf(sum_y, sum_yy, count);
    const double covariance = sum_xy / count - mean_x * mean_y;
    summary->pairs = pairs;
    summary->requested_mean_milliseconds = mean_x;
    summary->requested_deviation_milliseconds = std::sqrt(variance_x);
    summary->awake_mean_milliseconds = mean_y;
    summary->awake_deviation_milliseconds = std::sqrt(variance_y);
    summary->period_mean_milliseconds = mean_x + mean_y;
    // var(x+y) = var(x) + var(y) + 2*cov(x,y), so the period's spread comes
    // from the same sums without keeping a third series.
    const double variance_period = variance_x + variance_y + 2.0 * covariance;
    summary->period_deviation_milliseconds =
        std::sqrt(variance_period > 0.0 ? variance_period : 0.0);
    // A series with no spread has no correlation to report; leaving it at zero
    // says "no relationship measured" rather than dividing by zero.
    const double spread = std::sqrt(variance_x * variance_y);
    summary->correlation = spread > 0.0 ? covariance / spread : 0.0;
    return true;
}

}  // namespace re2dj::platform::windows
