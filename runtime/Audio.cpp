#include "Audio.h"

#include <cmath>
#include <cstdio>
#include <map>

#include "Target.h"
#include "miniaudio.h"

namespace scratch {

using Voices = std::map<int, ma_sound*>;

static ma_engine* E(void* p) { return static_cast<ma_engine*>(p); }

AudioEngine::~AudioEngine() { Close(); }

void AudioEngine::Init()
{
    auto* e = new ma_engine;
    if (ma_engine_init(nullptr, e) != MA_SUCCESS)
    {
        std::fprintf(stderr, "WARNING: AUDIO: Failed to initialize playback device\n");
        delete e;
        return;
    }
    engine_ = e;
    ready_ = true;
}

void AudioEngine::Load(SoundAsset& s)
{
    if (s.Loaded) return;
    s.Loaded = true;
    auto* snd = new ma_sound;
    // Decoded fully up front: starting a sound has no delay, like Scratch.
    if (ma_sound_init_from_file(E(engine_), s.File.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, snd) != MA_SUCCESS)
    {
        std::fprintf(stderr, "[audio] could not load %s\n", s.File.c_str());
        delete snd;
        return;
    }
    s.Sound = snd;
    masters_.push_back(snd);
}

void AudioEngine::Preload(const std::vector<Target*>& targets)
{
    LoadProgress::Phase = "sounds";
    for (Target* t : targets)
        for (auto& s : t->Sounds->Items)
        {
            if (ready_) Load(s);
            LoadProgress::Step();
        }
}

static void Apply(const Target& t, ma_sound* voice)
{
    ma_sound_set_volume(voice, (float)(t.Volume / 100));
    ma_sound_set_pitch(voice, (float)std::pow(2.0, t.SoundPitch / 120)); // 10 pitch units = 1 semitone
    ma_sound_set_pan(voice, (float)(t.SoundPan / 100));
}

void AudioEngine::Play(Target& t, int index)
{
    if (!ready_) return;
    SoundAsset& asset = t.Sounds->Items[index];
    Load(asset);
    if (!asset.Sound) return;
    if (!t.SoundVoices) t.SoundVoices = new Voices();
    auto& voices = *static_cast<Voices*>(t.SoundVoices);
    ma_sound*& voice = voices[index];
    if (!voice)
    {
        voice = new ma_sound;
        if (ma_sound_init_copy(E(engine_), static_cast<ma_sound*>(asset.Sound), 0, nullptr, voice) != MA_SUCCESS)
        {
            delete voice;
            voice = nullptr;
            return;
        }
    }
    Apply(t, voice);
    ma_sound_seek_to_pcm_frame(voice, 0);
    ma_sound_start(voice);
    withVoices_.insert(&t);
}

void AudioEngine::Sync(Target& t)
{
    if (!ready_ || !t.SoundVoices) return;
    for (auto& [i, v] : *static_cast<Voices*>(t.SoundVoices)) if (v) Apply(t, v);
}

void AudioEngine::StopAll()
{
    if (!ready_) return;
    for (Target* t : withVoices_)
        if (t->SoundVoices)
            for (auto& [i, v] : *static_cast<Voices*>(t->SoundVoices)) if (v) ma_sound_stop(v);
}

void AudioEngine::Release(Target& t)
{
    if (!t.SoundVoices) return;
    for (auto& [i, v] : *static_cast<Voices*>(t.SoundVoices))
        if (v) { ma_sound_uninit(v); delete v; }
    delete static_cast<Voices*>(t.SoundVoices);
    t.SoundVoices = nullptr;
    withVoices_.erase(&t);
}

void AudioEngine::Close()
{
    if (!ready_) return;
    for (Target* t : std::set<Target*>(withVoices_)) Release(*t);
    for (void* m : masters_) { ma_sound_uninit(static_cast<ma_sound*>(m)); delete static_cast<ma_sound*>(m); }
    masters_.clear();
    ma_engine_uninit(E(engine_));
    delete E(engine_);
    engine_ = nullptr;
    ready_ = false;
}

} // namespace scratch
