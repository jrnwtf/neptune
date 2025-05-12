#pragma once
#include "../../SDK/SDK.h"
#include "HitSoundBytes.h"

namespace HitSounds
{
    enum class Type
    {
        Default = 0,
        Bonk,
        COD,
        Quake,
        Moan
    };

    void PlayHitSound(Type type);
    void OnPlayerHurt(IGameEvent* pEvent);
} 