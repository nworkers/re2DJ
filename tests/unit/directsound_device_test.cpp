#include "re2dj/audio/directsound_device.h"

#include <cstdint>

#include "test_support.h"

namespace
{

namespace ds = re2dj::audio;

// The primary takes the fixed shape; a secondary needs PCM and a size.
void CheckPlans(re2dj::test::Context& context)
{
    ds::DsBufferDesc description;
    description.size = ds::kDsBufferDesc1Size;
    description.flags = ds::kDsbcapsPrimaryBuffer;
    const ds::SoundBufferPlan primary = ds::PlanSoundBuffer(description, nullptr);
    RE2DJ_CHECK_EQ(context, primary.result, ds::kDsOk);
    RE2DJ_CHECK(context, primary.primary);
    RE2DJ_CHECK_EQ(context, primary.bytes, ds::kPrimaryBufferBytes);
    RE2DJ_CHECK_EQ(context, primary.format.samples_per_second, 48000U);

    ds::WaveFormatEx format = {ds::kWaveFormatPcm, 2, 44100, 176400, 4, 16, 0};
    description.flags = 0x00010000U;
    description.buffer_bytes = 360448;
    const ds::SoundBufferPlan ring = ds::PlanSoundBuffer(description, &format);
    RE2DJ_CHECK_EQ(context, ring.result, ds::kDsOk);
    RE2DJ_CHECK(context, !ring.primary);
    RE2DJ_CHECK_EQ(context, ring.bytes, 360448U);
    RE2DJ_CHECK_EQ(context, ring.format.samples_per_second, 44100U);

    RE2DJ_CHECK_EQ(context, ds::PlanSoundBuffer(description, nullptr).result, ds::kDsErrBadFormat);
    description.buffer_bytes = 0;
    RE2DJ_CHECK_EQ(context, ds::PlanSoundBuffer(description, &format).result, ds::kDsErrBadFormat);
    description.buffer_bytes = ds::kMaxSoundBufferBytes + 1;
    RE2DJ_CHECK_EQ(context, ds::PlanSoundBuffer(description, &format).result, ds::kDsErrBadFormat);
    description.buffer_bytes = 100;
    format.format_tag = 2;
    RE2DJ_CHECK_EQ(context, ds::PlanSoundBuffer(description, &format).result, ds::kDsErrBadFormat);
    description.size = 16;
    RE2DJ_CHECK_EQ(context, ds::PlanSoundBuffer(description, &format).result, ds::kDsErrInvalidParam);
}

// Locks wrap to the start; an entire-buffer lock ignores the arguments.
void CheckLocks(re2dj::test::Context& context)
{
    ds::LockRegions regions;
    RE2DJ_CHECK(context, ds::PlanLock(100, 90, 30, 0, &regions));
    RE2DJ_CHECK_EQ(context, regions.first_offset, 90U);
    RE2DJ_CHECK_EQ(context, regions.first_bytes, 10U);
    RE2DJ_CHECK_EQ(context, regions.second_bytes, 20U);
    RE2DJ_CHECK(context, ds::PlanLock(100, 99, 999, ds::kDsbLockEntireBuffer, &regions));
    RE2DJ_CHECK_EQ(context, regions.first_offset, 0U);
    RE2DJ_CHECK_EQ(context, regions.first_bytes, 100U);
    RE2DJ_CHECK_EQ(context, regions.second_bytes, 0U);
    RE2DJ_CHECK(context, !ds::PlanLock(100, 100, 1, 0, &regions));
    RE2DJ_CHECK(context, !ds::PlanLock(100, 0, 101, 0, &regions));
    RE2DJ_CHECK(context, !ds::PlanLock(0, 0, 0, ds::kDsbLockEntireBuffer, &regions));
}

void CheckCaps(re2dj::test::Context& context)
{
    const ds::DsCaps device = ds::DeviceCaps();
    RE2DJ_CHECK_EQ(context, device.size, 96U);
    RE2DJ_CHECK_EQ(context, device.flags, ds::kDscapsPrimaryStereo | ds::kDscaps16Bit);
    const ds::DsbCaps buffer = ds::BufferCaps(0x10000U, 4096);
    RE2DJ_CHECK_EQ(context, buffer.size, 20U);
    RE2DJ_CHECK_EQ(context, buffer.buffer_bytes, 4096U);
    RE2DJ_CHECK_EQ(context, ds::CheckDuplicate(true), ds::kDsErrInvalidCall);
    RE2DJ_CHECK_EQ(context, ds::CheckDuplicate(false), ds::kDsOk);
}

}  // namespace

void RunDirectSoundDeviceTests(re2dj::test::Context& context)
{
    CheckPlans(context);
    CheckLocks(context);
    CheckCaps(context);
}
