#pragma once

#include <string>

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

class AudioEngine
{
public:

    AudioEngine();
    ~AudioEngine();
    
    //========================================================
    // Engine audio
    //========================================================

    bool Initialize();

    bool LoadEngineSound(const std::string& filepath);

    void PlayEngine();

    void UpdateEngineRPM(float speed, float maxSpeed, float dt);

    void StopEngine();

    void Shutdown();

    //========================================================
    // Menu audio
    //========================================================

    bool LoadMenuMusic(const std::string& filepath);

    void PlayMenuMusic();

    void StopMenuMusic();

    //========================================================
    // Delivery audio
    //========================================================

    bool LoadDeliveryMusic(const std::string& filepath);
    void PlayDeliveryMusic();
    void StopDeliveryMusic();

    void SetDeliveryPitch(float pitch);
    
    //========================================================
    // Ambient audio
    //========================================================

    bool LoadAmbientMusic(const std::string& filepath);
    void PlayAmbientMusic();
    void StopAmbientMusic();    

private:

    float currentPitch = 0.8f;

    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;

    ALuint engineBuffer = 0;
    ALuint engineSource = 0;

    ALuint menuBuffer = 0;
    ALuint menuSource = 0;

    ALuint ambientBuffer = 0;
    ALuint ambientSource = 0;

    ALuint deliveryBuffer = 0; 
    ALuint deliverySource = 0;
};