// Sound playback (miniaudio). Each sprite (and clone) gets its own voice per sound, so starting a sound
// restarts it for that sprite only, and volume/pitch/pan apply per sprite, as in Scratch.
#pragma once
#include <set>
#include <vector>

namespace scratch {

class Target;
struct SoundAsset;

class AudioEngine
{
public:
    ~AudioEngine();
    void Init();
    void Preload(const std::vector<Target*>& targets);
    void Play(Target& t, int index);
    /// Applies changed volume/effects to sounds already playing.
    void Sync(Target& t);
    void StopAll();
    /// A deleted clone's sounds stop.
    void Release(Target& t);
    void Close();
    bool Ready() const { return ready_; }

private:
    void* engine_ = nullptr; // ma_engine
    bool ready_ = false;
    std::set<Target*> withVoices_;
    std::vector<void*> masters_; // decoded sounds (ma_sound), shared by all voices
    void Load(SoundAsset& s);
};

} // namespace scratch
