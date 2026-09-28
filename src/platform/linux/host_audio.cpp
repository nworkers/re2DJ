#include "re2dj/platform/linux/host_audio.h"

#include "re2dj/audio/legacy_audio_buffer.h"
#include "sdl3_mixer_audio_backend.h"

namespace re2dj::platform::linux
{

using Backend = audio::Sdl3MixerAudioBackend;

bool LinuxHostAudio::Initialize(float master_gain, std::string* error)
{
    Backend& backend = Backend::Instance();
    if (!backend.ready())
    {
        *error = "the SDL3 audio mixer cannot start: " + backend.error();
        return false;
    }
    if (!backend.has_playback_device())
    {
        headless_reason_ = backend.error();
    }
    if (!backend.SetMasterGain(master_gain))
    {
        *error = "the SDL3 audio mixer refuses the master gain";
        return false;
    }
    backend_ = &backend;
    error->clear();
    return true;
}

void* LinuxHostAudio::FindVoice(std::uint32_t voice) const
{
    const auto found = voices_.find(voice);
    return found == voices_.end() ? nullptr : found->second;
}

std::uint32_t LinuxHostAudio::CreateVoice()
{
    if (backend_ == nullptr)
    {
        return 0;
    }
    Backend::Voice* voice = backend_->CreateVoice();
    if (voice == nullptr)
    {
        return 0;
    }
    const std::uint32_t id = next_voice_++;
    voices_[id] = voice;
    return id;
}

void LinuxHostAudio::DestroyVoice(std::uint32_t voice)
{
    const auto found = voices_.find(voice);
    if (found == voices_.end())
    {
        return;
    }
    backend_->DestroyVoice(static_cast<Backend::Voice*>(found->second));
    voices_.erase(found);
}

bool LinuxHostAudio::Play(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer, bool streaming)
{
    return backend_ != nullptr && backend_->Play(static_cast<Backend::Voice*>(FindVoice(voice)), buffer, streaming);
}

bool LinuxHostAudio::CommitStreamingWrite(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer)
{
    return backend_ != nullptr && backend_->CommitStreamingWrite(static_cast<Backend::Voice*>(FindVoice(voice)), buffer).success;
}

bool LinuxHostAudio::Stop(std::uint32_t voice)
{
    return backend_ != nullptr && backend_->Stop(static_cast<Backend::Voice*>(FindVoice(voice)));
}

bool LinuxHostAudio::SetPosition(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer)
{
    return backend_ != nullptr && backend_->SetPosition(static_cast<Backend::Voice*>(FindVoice(voice)), buffer);
}

bool LinuxHostAudio::UpdateControls(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer)
{
    return backend_ != nullptr && backend_->UpdateControls(static_cast<Backend::Voice*>(FindVoice(voice)), buffer);
}

std::uint32_t LinuxHostAudio::PositionBytes(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer) const
{
    return backend_ == nullptr ? buffer.current_position() : backend_->PositionBytes(static_cast<Backend::Voice*>(FindVoice(voice)), buffer);
}

bool LinuxHostAudio::IsPlaying(std::uint32_t voice) const
{
    return backend_ != nullptr && backend_->IsPlaying(static_cast<Backend::Voice*>(FindVoice(voice)));
}

}  // namespace re2dj::platform::linux
