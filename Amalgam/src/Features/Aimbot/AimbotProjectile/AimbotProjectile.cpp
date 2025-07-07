#include "AimbotProjectile.h"

#include "../Aimbot.h"
#include "../../Simulation/MovementSimulation/MovementSimulation.h"
#include "../../Simulation/ProjectileSimulation/ProjectileSimulation.h"
#include "../../Ticks/Ticks.h"
#include "../../Visuals/Visuals.h"
#include "../../NavBot/NavBot.h"
#include "../AutoAirblast/AutoAirblast.h"
#include <algorithm>

#ifndef TEXTMODE
// ... existing code ...
#else
// TEXTMODE stubs for all public methods
void CAimbotProjectile::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
    // Empty implementation for TEXTMODE
}

float CAimbotProjectile::GetSplashRadius(CTFWeaponBase* pWeapon, CTFPlayer* pPlayer)
{
    return 0.f;
}

bool CAimbotProjectile::AutoAirblast(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, CBaseEntity* pProjectile)
{
    return false;
}

float CAimbotProjectile::GetSplashRadius(CBaseEntity* pProjectile, CTFWeaponBase* pWeapon, CTFPlayer* pPlayer, float flScale, CTFWeaponBase* pAirblast)
{
    return 0.f;
}
#endif