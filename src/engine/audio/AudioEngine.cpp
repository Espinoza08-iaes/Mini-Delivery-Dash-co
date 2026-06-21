#include "AudioEngine.h"
#include <cstdint>

#define DR_WAV_IMPLEMENTATION
#include "../../../third_party/audio/dr_wav.h"

#include <iostream>
#include <vector>
// ===============================================
// Constructor methods
// ===============================================
AudioEngine::AudioEngine()
{

}

AudioEngine::~AudioEngine()
{
    Shutdown();
}

// ===============================================
// Engine audio method
// ===============================================
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
        std::cout << "[AUDIO] Failed loading WAV: " << filepath << "\n";
        return false;
    }

    std::vector<int16_t> samples(wav.totalPCMFrameCount * wav.channels);

    drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, samples.data());

    ALenum format;

    if (wav.channels == 1)
        format = AL_FORMAT_MONO16;
    else
        format = AL_FORMAT_STEREO16;

    alGenBuffers(1, &engineBuffer);

    alBufferData(engineBuffer, format, samples.data(),
        static_cast<ALsizei>(
            samples.size() * sizeof(int16_t)),
        wav.sampleRate);

    alGenSources(1, &engineSource);

    alSourcei(engineSource, AL_BUFFER, engineBuffer);

    alSourcei(engineSource, AL_LOOPING, AL_TRUE);

    alSourcef(engineSource, AL_GAIN, 0.5f);

    drwav_uninit(&wav);

    std::cout << "[AUDIO] Loaded: " << filepath << "\n";

    return true;
}

void AudioEngine::PlayEngine()
{
    alSourcePlay(engineSource);
}

void AudioEngine::UpdateEngineRPM(float speed, float maxSpeed, float dt)
{
    float normalized =
        std::abs(speed) / maxSpeed;

    if (normalized < 0.0f)
        normalized = 0.0f;

    if (normalized > 1.0f)
        normalized = 1.0f;

    float targetPitch = 0.8f + normalized * 1.5f;

    float response = (targetPitch > currentPitch) ? 8.0f : 1.6f;  // sube rapido : baja lento

    currentPitch += (targetPitch - currentPitch) * response * dt;

    alSourcef( engineSource, AL_PITCH, currentPitch);
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

    if (menuSource != 0)
    {
        alDeleteSources(1, &menuSource);
        menuSource = 0;
    }

    if (menuBuffer != 0)
    {
        alDeleteBuffers(1, &menuBuffer);
        menuBuffer = 0;
    }

    std::cout << "[AUDIO] Shutdown complete\n";
}

// ===============================================
// Menu audio methods
// ===============================================

bool AudioEngine::LoadMenuMusic(const std::string& filepath)
{
    drwav wav;

    if (!drwav_init_file(&wav, filepath.c_str(), nullptr))
    {
        std::cout << "[AUDIO] Failed loading WAV: " << filepath << "\n";
        return false;
    }

    std::vector<int16_t> samples(wav.totalPCMFrameCount * wav.channels);

    drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, samples.data());

    ALenum format;

    if (wav.channels == 1)
        format = AL_FORMAT_MONO16;
    else
        format = AL_FORMAT_STEREO16;

    alGenBuffers(1, &menuBuffer);

    alBufferData(menuBuffer, format, samples.data(),
        static_cast<ALsizei>(
            samples.size() * sizeof(int16_t)),
        wav.sampleRate);

    alGenSources(1, &menuSource);

    alSourcei(menuSource, AL_BUFFER, menuBuffer);

    alSourcei(menuSource, AL_LOOPING, AL_TRUE);

    alSourcef(menuSource, AL_GAIN, 0.2f);

    drwav_uninit(&wav);

    std::cout << "[AUDIO] Loaded: " << filepath << "\n";

    return true;
}

void AudioEngine::PlayMenuMusic()
{
    alSourceStop(menuSource);
    alSourceRewind(menuSource);
    alSourcePlay(menuSource);
}

void AudioEngine::StopMenuMusic()
{
    alSourceStop(menuSource);
}

// ===============================================
// Delivery audio methods
// ===============================================

bool AudioEngine::LoadAmbientMusic(const std::string& filepath)
{
    drwav wav;

    if (!drwav_init_file(&wav, filepath.c_str(), nullptr))
    {
        std::cout << "[AUDIO] Failed loading WAV: " << filepath << "\n";
        return false;
    }

    std::vector<int16_t> samples(wav.totalPCMFrameCount * wav.channels);

    drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, samples.data());

    ALenum format;

    if (wav.channels == 1)
        format = AL_FORMAT_MONO16;
    else
        format = AL_FORMAT_STEREO16;

    alGenBuffers(1, &ambientBuffer);

    alBufferData(ambientBuffer, format, samples.data(),
        static_cast<ALsizei>(
            samples.size() * sizeof(int16_t)),
        wav.sampleRate);

    alGenSources(1, &ambientSource);

    alSourcei(ambientSource, AL_BUFFER, ambientBuffer);

    alSourcei(ambientSource, AL_LOOPING, AL_TRUE);

    alSourcef(ambientSource, AL_GAIN, 0.2f);

    drwav_uninit(&wav);

    std::cout << "[AUDIO] Loaded: " << filepath << "\n";

    return true;
}

void AudioEngine::PlayAmbientMusic()
{
    ALint state;
    alGetSourcei(ambientSource, AL_SOURCE_STATE, &state);

    if(state != AL_PLAYING)
        alSourcePlay(ambientSource);
}

void AudioEngine::StopAmbientMusic()
{
    alSourceStop(ambientSource);
}

void AudioEngine::SetDeliveryPitch(float pitch)
{
    alSourcef(deliverySource, AL_PITCH, pitch);
}

// ===============================================
// Delivery audio methods
// ===============================================

bool AudioEngine::LoadDeliveryMusic(const std::string& filepath)
{
    drwav wav;

    if (!drwav_init_file(&wav, filepath.c_str(), nullptr))
    {
        std::cout << "[AUDIO] Failed loading WAV: " << filepath << "\n";
        return false;
    }

    std::vector<int16_t> samples(wav.totalPCMFrameCount * wav.channels);

    drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, samples.data());

    ALenum format;

    if (wav.channels == 1)
        format = AL_FORMAT_MONO16;
    else
        format = AL_FORMAT_STEREO16;

    alGenBuffers(1, &deliveryBuffer);

    alBufferData(deliveryBuffer, format, samples.data(),
        static_cast<ALsizei>(
            samples.size() * sizeof(int16_t)),
        wav.sampleRate);

    alGenSources(1, &deliverySource);

    alSourcei(deliverySource, AL_BUFFER, deliveryBuffer);

    alSourcei(deliverySource, AL_LOOPING, AL_TRUE);

    alSourcef(deliverySource, AL_GAIN, 0.2f);

    drwav_uninit(&wav);

    std::cout << "[AUDIO] Loaded: " << filepath << "\n";

    return true;
}

void AudioEngine::PlayDeliveryMusic()
{
    ALint state;
    alGetSourcei(deliverySource, AL_SOURCE_STATE, &state);

    if(state != AL_PLAYING)
        alSourcePlay(deliverySource);
}

void AudioEngine::StopDeliveryMusic()
{
    alSourceStop(deliverySource);
}

void AudioEngine::UpdateVolumes(bool musicOn, bool sfxOn)
{
    float musicGain = musicOn ? 0.2f : 0.0f;
    float sfxGain = sfxOn ? 0.5f : 0.0f;

    alSourcef(menuSource, AL_GAIN, musicGain);
    alSourcef(ambientSource, AL_GAIN, musicGain);
    alSourcef(deliverySource, AL_GAIN, musicGain);

    alSourcef(engineSource, AL_GAIN, sfxGain);
}