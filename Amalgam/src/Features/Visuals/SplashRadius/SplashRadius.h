#pragma once
#include "../../../SDK/SDK.h"
#include <vector>
#include <cmath>

class CSplashRadius
{
private:
    // Helper functions
    Vec3 CrossProduct(const Vec3& a, const Vec3& b);
    void Draw3DPolygon(const Vec3& center, float radius, const Vec3& normal, int segments);
    bool ShouldShowProjectile(CBaseEntity* pEntity);
    
public:
    void Draw();
};

ADD_FEATURE(CSplashRadius, SplashRadius)