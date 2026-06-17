#pragma once

#include <string>

#include <OpenAL/al.h>
#include <OpenAL/alc.h>

class AudioEngine
{
public:

    AudioEngine();
    ~AudioEngine();

    bool Initialize();

    bool LoadEngineSound(const std::string& filepath);

    void PlayEngine();

    void UpdateEngineRPM(float speed, float maxSpeed);

    void StopEngine();

    void Shutdown();

private:

    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;

    ALuint engineBuffer = 0;
    ALuint engineSource = 0;
};