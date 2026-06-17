#include "AudioEngine.h"

#define DR_WAV_IMPLEMENTATION
#include "../../../third_party/audio/dr_wav.h"

#include <iostream>
#include <vector>

AudioEngine::AudioEngine()
{

}

AudioEngine::~AudioEngine()
{
    Shutdown();
}

bool AudioEngine::Initialize()
{
    device = alcOpenDevice(nullptr);

    if (!device)
    {
        std::cout << "[AUDIO] Failed to open device\n";
        return false;
    }

    context = alcCreateContext(device, nullptr);

    if (!context)
    {
        std::cout << "[AUDIO] Failed to create context\n";
        return false;
    }

    alcMakeContextCurrent(context);

    std::cout << "[AUDIO] OpenAL initialized\n";

    return true;
}

bool AudioEngine::LoadEngineSound(const std::string& filepath)
{
    drwav wav;

    if (!drwav_init_file(&wav, filepath.c_str(), nullptr))
    {
        std::cout << "[AUDIO] Failed loading WAV: "
                  << filepath << "\n";
        return false;
    }

    std::vector<int16_t> samples(
        wav.totalPCMFrameCount *
        wav.channels);

    drwav_read_pcm_frames_s16(
        &wav,
        wav.totalPCMFrameCount,
        samples.data());

    ALenum format;

    if (wav.channels == 1)
        format = AL_FORMAT_MONO16;
    else
        format = AL_FORMAT_STEREO16;

    alGenBuffers(1, &engineBuffer);

    alBufferData(
        engineBuffer,
        format,
        samples.data(),
        static_cast<ALsizei>(
            samples.size() * sizeof(int16_t)),
        wav.sampleRate);

    alGenSources(1, &engineSource);

    alSourcei(
        engineSource,
        AL_BUFFER,
        engineBuffer);

    alSourcei(
        engineSource,
        AL_LOOPING,
        AL_TRUE);

    alSourcef(
        engineSource,
        AL_GAIN,
        0.5f);

    drwav_uninit(&wav);

    std::cout << "[AUDIO] Loaded: "
              << filepath << "\n";

    return true;
}

void AudioEngine::PlayEngine()
{
    alSourcePlay(engineSource);
}

void AudioEngine::UpdateEngineRPM(
    float speed,
    float maxSpeed)
{
    float normalized =
        speed / maxSpeed;

    if (normalized < 0.0f)
        normalized = -normalized;

    if (normalized > 1.0f)
        normalized = 1.0f;

    float pitch =
        0.8f +
        normalized * 1.5f;

    alSourcef(
        engineSource,
        AL_PITCH,
        pitch);
}

void AudioEngine::StopEngine()
{
    if (engineSource != 0)
    {
        alSourceStop(engineSource);
    }
}

void AudioEngine::Shutdown()
{
    if (engineSource != 0)
    {
        alDeleteSources(1, &engineSource);
        engineSource = 0;
    }

    if (engineBuffer != 0)
    {
        alDeleteBuffers(1, &engineBuffer);
        engineBuffer = 0;
    }

    if (context)
    {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        context = nullptr;
    }

    if (device)
    {
        alcCloseDevice(device);
        device = nullptr;
    }

    std::cout << "[AUDIO] Shutdown complete\n";
}