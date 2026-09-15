#include "re2dj/graphics/present_sync.h"

#include <string>

#include "re2dj/target/target_profile.h"
#include "test_support.h"

namespace
{

using re2dj::graphics::ParsePresentSyncName;
using re2dj::graphics::PresentSync;
using re2dj::graphics::PresentSyncName;

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

    // Every profile inherits the behavior the product had before the policy
    // became explicit. A profile that opts out must do so deliberately.
    const re2dj::target::TargetRunDefaults defaults;
    RE2DJ_CHECK(context, defaults.present_sync == PresentSync::kVerticalSync);
}
