#pragma once

#include <string>

#include <OpenAL/al.h>
#include <OpenAL/alc.h>

class AudioEngine
{
public:

    AudioEngine();
    ~AudioEngine();
    
    //======================================
    // Engine audio
    //======================================

    bool Initialize();

    bool LoadEngineSound(const std::string& filepath);

    void PlayEngine();

    void UpdateEngineRPM(float speed, float maxSpeed, float dt);

    void StopEngine();

    void Shutdown();

    //======================================
    // Menu audio
    //======================================

    bool LoadMenuMusic(const std::string& filepath);

    void PlayMenuMusic();

    void StopMenuMusic();

private:

    float currentPitch = 0.8f;

    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;

    ALuint engineBuffer = 0;
    ALuint engineSource = 0;

    ALuint menuBuffer = 0;
    ALuint menuSource = 0;
};