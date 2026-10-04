#include "re2dj/graphics/present_sync.h"

#include <cstdint>
#include <string>

#include "re2dj/graphics/present_pacer.h"

#include "test_support.h"

namespace
{

using re2dj::graphics::ParsePresentSyncName;
using re2dj::graphics::PresentSync;
using re2dj::graphics::PresentSyncName;
using re2dj::graphics::PresentPacer;

// Runs frames of a guest that works work_ns between presents, honouring each
// wait; returns the time after the last present.
std::uint64_t RunFrames(PresentPacer* pacer, std::uint64_t start_ns, std::uint64_t work_ns, int frames,
                        std::uint64_t* waited_ns = nullptr)
{
    std::uint64_t now = start_ns;
    for (int frame = 0; frame < frames; ++frame)
    {
        const std::uint64_t wait = pacer->AfterPresent(now);
        if (waited_ns != nullptr)
        {
            *waited_ns += wait;
        }
        now += wait + work_ns;
    }
    return now;
}

// Software pacing engages only when presents come faster than the display
// refresh, then holds them to it without bursts after a late frame.
void CheckPresentPacer(re2dj::test::Context& context)
{
    constexpr std::uint64_t kPeriod = 16666667;
    // A swap that blocks: presents every period never engage it.
    PresentPacer blocking(60.0);
    std::uint64_t waited = 0;
    RunFrames(&blocking, 1000, kPeriod, 300, &waited);
    RE2DJ_CHECK(context, !blocking.engaged());
    RE2DJ_CHECK_EQ(context, waited, std::uint64_t{0});
    // A guest slower than the refresh never engages it either.
    PresentPacer slow(60.0);
    RunFrames(&slow, 1000, 2 * kPeriod, 300);
    RE2DJ_CHECK(context, !slow.engaged());

    // A swap that returns at once (about 112 presents a second): the first
    // 60 intervals are only watched, then presents come one period apart.
    PresentPacer fast(60.0);
    RE2DJ_CHECK_EQ(context, fast.period_ns(), kPeriod);
    std::uint64_t now = RunFrames(&fast, 1000, 8900000, PresentPacer::kProbeFrames);
    RE2DJ_CHECK(context, !fast.engaged());
    now = RunFrames(&fast, now, 8900000, 1);
    RE2DJ_CHECK(context, fast.engaged());
    const std::uint64_t start = now;
    now = RunFrames(&fast, now, 8900000, 120);
    RE2DJ_CHECK(context, now - start >= 119 * kPeriod && now - start <= 121 * kPeriod);
    // A late frame waits for nothing, and the next one is not rushed.
    RE2DJ_CHECK_EQ(context, fast.AfterPresent(now + 3 * kPeriod), std::uint64_t{0});
    RE2DJ_CHECK_EQ(context, fast.AfterPresent(now + 3 * kPeriod + 1000000), kPeriod - 1000000);

    // A policy that asks presents not to block, and an unknown refresh rate.
    PresentPacer immediate(60.0, false);
    RunFrames(&immediate, 1000, 1000000, 300);
    RE2DJ_CHECK(context, !immediate.engaged());
    RE2DJ_CHECK_EQ(context, PresentPacer(0.0).period_ns(), kPeriod);
    RE2DJ_CHECK_EQ(context, PresentPacer(144.0).period_ns(), std::uint64_t{6944444});
}

}  // namespace

void RunPresentSyncTests(re2dj::test::Context& context)
{
    // The launcher carries these words between two processes, so a rename is a
    // compatibility break rather than a cosmetic change.
    RE2DJ_CHECK(context, std::string(PresentSyncName(PresentSync::kVerticalSync)) == "vsync");
    RE2DJ_CHECK(context, std::string(PresentSyncName(PresentSync::kImmediate)) == "immediate");
    RE2DJ_CHECK(context, std::string(PresentSyncName(PresentSync::kAdaptive)) == "adaptive");

    for (const PresentSync value :
         {PresentSync::kVerticalSync, PresentSync::kImmediate, PresentSync::kAdaptive})
    {
        PresentSync parsed = PresentSync::kAdaptive;
        RE2DJ_CHECK(context, ParsePresentSyncName(PresentSyncName(value), &parsed));
        RE2DJ_CHECK(context, parsed == value);
    }

    // A typo on a command line must be reported, not silently resolved to a
    // policy the caller did not ask for. The product spells its own choices
    // on/off/adaptive, so those words are deliberately not accepted here.
    PresentSync parsed = PresentSync::kImmediate;
    RE2DJ_CHECK(context, !ParsePresentSyncName("", &parsed));
    RE2DJ_CHECK(context, !ParsePresentSyncName("on", &parsed));
    RE2DJ_CHECK(context, !ParsePresentSyncName("off", &parsed));
    RE2DJ_CHECK(context, !ParsePresentSyncName("VSYNC", &parsed));
    RE2DJ_CHECK(context, !ParsePresentSyncName("vsync ", &parsed));
    // A rejected parse leaves the caller's value alone.
    RE2DJ_CHECK(context, parsed == PresentSync::kImmediate);
    RE2DJ_CHECK(context, !ParsePresentSyncName("vsync", nullptr));

    CheckPresentPacer(context);
}
