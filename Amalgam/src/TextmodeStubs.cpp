#ifdef TEXTMODE

#include "Features/Visuals/Visuals.h"
#include "Features/Aimbot/AutoRocketJump/AutoRocketJump.h"
#include "Features/Simulation/MovementSimulation/MovementSimulation.h"
#include "Features/ImGui/Render.h"
#include <d3d9.h>

void CVisuals::RestoreWorldModulation()
{

}

std::vector<DrawBox_t> CVisuals::GetHitboxes(matrix3x4* /*aBones*/, CBaseAnimating* /*pEntity*/, std::vector<int> /*vHitboxes*/, int /*iTarget*/)
{
    return {};
}

void CAutoRocketJump::Run(CTFPlayer* /*pLocal*/, CTFWeaponBase* /*pWeapon*/, CUserCmd* /*pCmd*/)
{

}

void CMovementSimulation::Store()
{

}

bool CMovementSimulation::Initialize(CBaseEntity* /*pEntity*/, PlayerStorage& /*tStorage*/, bool /*bHitchance*/, bool /*bStrafe*/)
{
    return false;
}

bool CMovementSimulation::SetDuck(PlayerStorage& /*tStorage*/, bool /*bDuck*/)
{
    return false;
}

void CMovementSimulation::RunTick(PlayerStorage& /*tStorage*/, bool /*bPath*/, std::function<void(CMoveData&)>* /*pCallback*/)
{
    // No-op.
}

void CMovementSimulation::RunTick(PlayerStorage& /*tStorage*/, bool /*bPath*/, std::function<void(CMoveData&)> /*fCallback*/)
{
    // No-op.
}

void CMovementSimulation::Restore(PlayerStorage& /*tStorage*/)
{
    // No-op.
}

float CMovementSimulation::GetPredictedDelta(CBaseEntity* /*pEntity*/)
{
    return 0.0f;
}

void CRender::Render(IDirect3DDevice9* /*pDevice*/)
{
    // No rendering in textmode.
}

void CRender::Initialize(IDirect3DDevice9* /*pDevice*/)
{
    // Nothing to initialize in textmode.
}

void CRender::LoadColors()
{
}

void CRender::LoadFonts()
{
}

void CRender::LoadStyle()
{
}

#endif // TEXTMODE 