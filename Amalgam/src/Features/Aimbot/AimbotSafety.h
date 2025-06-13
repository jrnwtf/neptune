#pragma once
#include "../../SDK/SDK.h"

namespace AimbotSafety
{
    // Centralized safety validation functions
    inline bool IsValidEntity(CBaseEntity* pEntity)
    {
        return pEntity && !pEntity->IsDormant() && pEntity->entindex() > 0;
    }
    
    inline bool IsValidPlayer(CTFPlayer* pPlayer)
    {
        return IsValidEntity(pPlayer) && pPlayer->IsAlive();
    }
    
    inline bool IsValidGameState()
    {
        return I::EngineClient && I::EngineClient->IsInGame() && I::GlobalVars;
    }
    
    inline bool IsValidBoneIndex(int iBone, int maxBones = MAXSTUDIOBONES)
    {
        return iBone >= 0 && iBone < maxBones;
    }
    
    inline bool IsValidPosition(const Vec3& pos)
    {
        return !isnan(pos.x) && !isnan(pos.y) && !isnan(pos.z) && 
               pos.Length() < 32768.0f && pos.Length() > -32768.0f;
    }
    
    inline bool IsValidBoneMatrix(const matrix3x4& matrix)
    {
        // Check if matrix has reasonable values (not all zeros or NaN)
        return (matrix[0][0] != 0.0f || matrix[1][1] != 0.0f || matrix[2][2] != 0.0f) &&
               !isnan(matrix[0][3]) && !isnan(matrix[1][3]) && !isnan(matrix[2][3]);
    }
    
    // Safe vector operations with size limits
    template<typename T>
    void SafeResize(std::vector<T>& vec, size_t maxSize)
    {
        if (vec.size() > maxSize)
            vec.resize(maxSize);
    }
    
    // Safe entity iteration with validation
    template<typename Func>
    void SafeEntityIteration(const std::vector<CBaseEntity*>& entities, Func func)
    {
        for (auto pEntity : entities)
        {
            if (!IsValidEntity(pEntity))
                continue;
                
            try {
                func(pEntity);
            } catch (...) {
                // Skip problematic entities
                continue;
            }
        }
    }
} 