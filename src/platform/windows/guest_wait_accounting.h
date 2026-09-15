#pragma once

#include <cstdint>

#include "re2dj/graphics/present_interval_histogram.h"

namespace re2dj::platform::windows
{

// Where a guest's frame time goes when it is not computing.
//
// A frame interval says how long a frame took; it cannot say whether that time
// was spent in the guest's own code, in a sleep, or blocked on an object. This
// accounts the blocking calls so the remainder can be attributed.
//
// The recording side is called from the guest's threads on its hot path, so it
// only increments counters. Formatting happens when a summary is taken.
struct GuestWaitSummary
{
    std::uint32_t sleep_calls = 0;
    double sleep_milliseconds = 0.0;
    // Distribution of measured sleep durations, which separates "many short
    // sleeps" from "one long one" at the same total.
    graphics::PresentIntervalHistogram::Summary sleep_durations;
    // What the guest asked for, as opposed to what it got. A 1 ms request that
    // measures 2 ms is the scheduler's granularity rather than the guest's
    // intent, and only both numbers together show that.
    double requested_sleep_milliseconds = 0.0;

    std::uint32_t wait_calls = 0;
    double wait_milliseconds = 0.0;
    graphics::PresentIntervalHistogram::Summary wait_durations;

    // Counted but not timed: reading a clock is not a wait, and its call rate
    // is what indicates a polling loop.
    std::uint32_t time_query_calls = 0;
};

// How a guest arrives at the sleep it asks for.
//
// Two designs produce the same average request and cannot be told apart by it.
// A fixed constant sleeps the same amount every frame regardless of how long
// the frame's work took. A `target - elapsed` limiter sleeps less after a
// heavier frame, which makes the request move opposite to the work.
//
// Pairing each request with the non-sleeping time that preceded it separates
// them, and three derived numbers say which it is: a negative correlation is a
// feedback loop, a near-zero request deviation is a constant, and a near-zero
// deviation of request-plus-work means the guest holds a period, whose mean is
// then that target period.
//
// Samples are not stored. Everything below comes from five running sums, so
// the call path stays a handful of additions.
struct GuestSleepModelSummary
{
    std::uint32_t pairs = 0;
    double requested_mean_milliseconds = 0.0;
    double requested_deviation_milliseconds = 0.0;
    // The non-sleeping segment: from the previous sleep's return to this
    // sleep's entry, which for a one-sleep-per-frame guest is the frame's work
    // plus its present.
    double awake_mean_milliseconds = 0.0;
    double awake_deviation_milliseconds = 0.0;
    // Request plus the work that preceded it, which is the period the guest
    // would be holding if it were holding one.
    double period_mean_milliseconds = 0.0;
    double period_deviation_milliseconds = 0.0;
    // Pearson correlation between request and preceding work, in -1..1.
    double correlation = 0.0;
};

// `awake_milliseconds` is the time since the previous sleep returned. Pass a
// negative value for the first sleep of a run, which has no predecessor and so
// contributes no pair; it still counts toward the totals above.
void RecordGuestSleep(double measured_milliseconds,
                      unsigned requested_milliseconds,
                      double awake_milliseconds);
void RecordGuestWait(double measured_milliseconds);
void RecordGuestTimeQuery();

// Returns the accumulated summary and clears the accumulator, so each caller
// sees one window's worth. Returns false when nothing was recorded, which is
// how a run that never reached the wrappers stays silent instead of emitting
// a line of zeroes that reads like a measurement.
bool TakeGuestWaitSummary(GuestWaitSummary* summary);

// Same contract as above, over the pairs recorded in the same window. Returns
// false while fewer than two pairs exist, since a deviation and a correlation
// are undefined below that and a single pair would print as a settled reading.
bool TakeGuestSleepModelSummary(GuestSleepModelSummary* summary);

}  // namespace re2dj::platform::windows
