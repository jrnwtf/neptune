#ifdef TEXTMODE

#include "Features/Visuals/Visuals.h"
#include "Features/Aimbot/AutoRocketJump/AutoRocketJump.h"
#include "Features/Simulation/MovementSimulation/MovementSimulation.h"
#include "Features/Simulation/ProjectileSimulation/ProjectileSimulation.h"
#include "Features/Visuals/ESP/ESP.h"
#include "Features/Spectate/Spectate.h"
#include "Features/Killstreak/Killstreak.h"
#include "Features/ImGui/Render.h"
#include "Features/Aimbot/AimbotProjectile/AimbotProjectile.h"
#include "SDK/Definitions/Misc/VGUI.h"
#include "SDK/Definitions/Interfaces/IVModelRender.h"
#include <d3d9.h>
#include <vector>
#include "Features/Visuals/Materials/Materials.h"
#include "Features/Visuals/Notifications/Notifications.h"
#include <string>
#include "Features/Backtrack/Backtrack.h"
#include "Features/EnginePrediction/EnginePrediction.h"
#include "Features/Ticks/Ticks.h"
#include "Features/ImGui/Menu/Menu.h"

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

bool CAimbotProjectile::AutoAirblast(CTFPlayer* /*pLocal*/, CTFWeaponBase* /*pWeapon*/, CUserCmd* /*pCmd*/, CBaseEntity* /*pProjectile*/)
{
    return false;
}

float CAimbotProjectile::GetSplashRadius(CTFWeaponBase* /*pWeapon*/, CTFPlayer* /*pPlayer*/)
{
    return 0.0f;
}

float CAimbotProjectile::GetSplashRadius(CBaseEntity* /*pProjectile*/, CTFWeaponBase* /*pWeapon*/, CTFPlayer* /*pPlayer*/, float /*flScale*/, CTFWeaponBase* /*pAirblast*/)
{
    return 0.0f;
}

// ESP stubs
void CESP::Store(CTFPlayer* /*pLocal*/)
{
    // No-op.
}

void CESP::Draw()
{
    // No-op.
}

// Spectate stubs
void CSpectate::NetUpdateEnd(CTFPlayer* /*pLocal*/)
{
    // No-op.
}

void CSpectate::NetUpdateStart(CTFPlayer* /*pLocal*/)
{
    // No-op.
}

void CSpectate::CreateMove(CUserCmd* /*pCmd*/)
{
    // No-op.
}

void CSpectate::SetTarget(int /*iTarget*/)
{
    // No-op.
}

// Killstreak stubs
void CKillstreak::PlayerDeath(IGameEvent* /*pEvent*/)
{
    // No-op.
}

void CKillstreak::PlayerSpawn(IGameEvent* /*pEvent*/)
{
    // No-op.
}

void CKillstreak::Reset()
{
    // No-op.
}

// ProjectileSimulation stubs
bool CProjectileSimulation::GetInfo(CTFPlayer* /*pPlayer*/, CTFWeaponBase* /*pWeapon*/, Vec3 /*vAngles*/, ProjectileInfo& /*tProjInfo*/, int /*iFlags*/, float /*flAutoCharge*/)
{
    return false;
}

void CProjectileSimulation::SetupTrace(CTraceFilterCollideable& /*filter*/, int& /*nMask*/, CTFWeaponBase* /*pWeapon*/, int /*nTick*/, bool /*bQuick*/)
{
    // No-op.
}

void CProjectileSimulation::GetInfo(CBaseEntity* /*pProjectile*/, ProjectileInfo& /*tProjInfo*/)
{
    // No-op.
}

void CProjectileSimulation::SetupTrace(CTraceFilterCollideable& /*filter*/, int& /*nMask*/, CBaseEntity* /*pProjectile*/)
{
    // No-op.
}

bool CProjectileSimulation::Initialize(ProjectileInfo& /*tProjInfo*/, bool /*bSimulate*/, bool /*bWorld*/)
{
    return false;
}

void CProjectileSimulation::RunTick(ProjectileInfo& /*tProjInfo*/, bool /*bPath*/)
{
    // No-op.
}

Vec3 CProjectileSimulation::GetOrigin()
{
    return Vec3();
}

Vec3 CProjectileSimulation::GetVelocity()
{
    return Vec3();
}

void IPanel_PaintTraverse(void* /*rcx*/, VPANEL /*vguiPanel*/, bool /*forceRepaint*/, bool /*allowForce*/)
{
    // No-op.
}

void IVModelRender_DrawModelExecute(void* /*rcx*/, const DrawModelState_t& /*pState*/, const ModelRenderInfo_t& /*pInfo*/, matrix3x4* /*pBoneToWorld*/)
{
    // No-op.
}

// -----------------------------
// CDraw stubs
// -----------------------------

Vec2 CDraw::GetTextSize(const char* /*text*/, const Font_t& /*tFont*/)
{
    return Vec2{};
}

void CDraw::String(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const EAlign& /*eAlign*/, const wchar_t* /*wstr*/)
{
}

void CDraw::StringOutlined(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const Color_t& /*tColorOut*/, const EAlign& /*eAlign*/, const wchar_t* /*wstr*/)
{
}

void CDraw::StringWithBackground(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const Color_t& /*tColorBg*/, const EAlign& /*eAlign*/, const wchar_t* /*wstr*/)
{
}

void CDraw::GradientRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const Color_t& /*tColorTop*/, const Color_t& /*tColorBottom*/, bool /*bHorizontal*/)
{
}

void CDraw::Line(int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/, const Color_t& /*tColor*/)
{
}

void CDraw::FillRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const Color_t& /*tColor*/)
{
}

void CDraw::LineRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const Color_t& /*tColor*/)
{
}

void CDraw::LineRectOutline(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const Color_t& /*tColor*/, const Color_t& /*tColorOut*/, bool /*bInside*/)
{
}

void CDraw::FillRectPercent(int /*x*/, int /*y*/, int /*w*/, int /*h*/, float /*t*/, const Color_t& /*tColor*/, const Color_t& /*tColorOut*/, const EAlign& /*eAlign*/, bool /*bAdjust*/)
{
}

void CDraw::FillCircle(int /*x*/, int /*y*/, float /*iRadius*/, int /*iSegments*/, Color_t /*clr*/)
{
}

void CDraw::LineCircle(int /*x*/, int /*y*/, float /*iRadius*/, int /*iSegments*/, const Color_t& /*clr*/)
{
}

void CDraw::FillRoundRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/, int /*iRadius*/, const Color_t& /*tColor*/, int /*iCount*/)
{
}

void CDraw::FillPolygon(std::vector<Vertex_t> /*vVertices*/, const Color_t& /*tColor*/)
{
}

void CDraw::Texture(int /*x*/, int /*y*/, int /*w*/, int /*h*/, int /*iId*/, const EAlign& /*eAlign*/)
{
}

void CDraw::DrawHudTexture(float /*x*/, float /*y*/, float /*s*/, const CHudTexture* /*pTexture*/, Color_t /*tColor*/)
{
}

void CDraw::Avatar(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const uint32 /*nFriendID*/, const EAlign& /*eAlign*/)
{
}

CHudTexture* CDraw::GetIcon(const char* /*szIcon*/, int /*eIconFormat*/)
{
    return nullptr;
}

Vec2 CDraw::GetTextSize(const wchar_t* /*text*/, const Font_t& /*tFont*/)
{
    return Vec2{};
}

void CDraw::FillRectOutline(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const Color_t& /*tColor*/, const Color_t& /*tColorOut*/)
{
}

// Overloads with char* parameters
void CDraw::String(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const EAlign& /*eAlign*/, const char* /*str*/)
{
}

void CDraw::StringOutlined(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const Color_t& /*tColorOut*/, const EAlign& /*eAlign*/, const char* /*str*/)
{
}

void CDraw::StringWithBackground(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const Color_t& /*tColorBg*/, const EAlign& /*eAlign*/, const char* /*str*/)
{
}

// -----------------------------
// CMaterials stubs
// -----------------------------

void CMaterials::UnloadMaterials()
{
}

void CMaterials::ReloadMaterials()
{
}

IMaterial* CMaterials::Create(char const* /*szName*/, KeyValues* /*pKV*/)
{
    return nullptr;
}

void CMaterials::RemoveMaterial(const char* /*sName*/)
{
}

void CMaterials::EditMaterial(const char* /*sName*/, const char* /*sVMT*/)
{
}

void CMaterials::AddMaterial(const char* /*sName*/)
{
}

std::string CMaterials::GetVMT(unsigned int /*uHash*/)
{
    return std::string{};
}

Material_t* CMaterials::GetMaterial(unsigned int /*uHash*/)
{
    return nullptr;
}

void CMaterials::SetColor(Material_t* /*pMaterial*/, Color_t /*tColor*/)
{
}

void CMaterials::Remove(IMaterial* /*pMaterial*/)
{
}

void CNotifications::Add(const std::string& /*sText*/, float /*flLifeTime*/, float /*flPanTime*/, const Color_t& /*tAccent*/, const Color_t& /*tBackground*/, const Color_t& /*tActive*/)
{
}

void CNotifications::Draw()
{
    // No-op.
}

// -----------------------------
// CBacktrack stubs
// -----------------------------

void CBacktrack::Store() {}
void CBacktrack::SendLerp() {}
void CBacktrack::Draw(CTFPlayer*) {}
void CBacktrack::Reset() {}
bool CBacktrack::GetRecords(CBaseEntity*, std::vector<TickRecord*>&) { return false; }
std::vector<TickRecord*> CBacktrack::GetValidRecords(std::vector<TickRecord*>&, CTFPlayer*, bool, float) { return {}; }
float CBacktrack::GetReal(int, bool) { return 0.f; }
float CBacktrack::GetWishFake() { return 0.f; }
float CBacktrack::GetWishLerp() { return 0.f; }
float CBacktrack::GetFakeLatency() { return 0.f; }
float CBacktrack::GetFakeInterp() { return 0.f; }
float CBacktrack::GetWindow() { return 0.f; }
void CBacktrack::SetLerp(IGameEvent*) {}
int CBacktrack::GetAnticipatedChoke(int) { return 0; }
void CBacktrack::ResolverUpdate(CBaseEntity*) {}
void CBacktrack::ReportShot(int) {}
void CBacktrack::AdjustPing(CNetChannel*) {}
void CBacktrack::RestorePing(CNetChannel*) {}
void CBacktrack::BacktrackToCrosshair(CUserCmd*) {}

// -----------------------------
// CEnginePrediction stubs
// -----------------------------

void CEnginePrediction::Start(CTFPlayer*, CUserCmd*) {}
void CEnginePrediction::End(CTFPlayer*, CUserCmd*) {}
void CEnginePrediction::ScalePlayers(CBaseEntity*) {}
void CEnginePrediction::RestorePlayers() {}

// -----------------------------
// CTicks stubs
// -----------------------------

void CTicks::Run(float /*accumulated_extra_samples*/, bool /*bFinalTick*/, CTFPlayer* /*pLocal*/)
{
    // No operation in textmode.
}

void CTicks::Draw(CTFPlayer* /*pLocal*/)
{
    // No operation in textmode.
}

void CTicks::Reset()
{
    // Nothing to reset in textmode.
}

void CTicks::CLMove(float /*accumulated_extra_samples*/, bool /*bFinalTick*/)
{
    // No operation in textmode.
}

void CTicks::CLMoveManage(CTFPlayer* /*pLocal*/)
{
    // No operation in textmode.
}

void CTicks::CreateMove(CTFPlayer* /*pLocal*/, CUserCmd* /*pCmd*/, bool* /*pSendPacket*/)
{
    // No operation in textmode.
}

void CTicks::AntiWarp(CTFPlayer* /*pLocal*/, CUserCmd* /*pCmd*/)
{
    // No operation in textmode.
}

int CTicks::GetTicks(CTFWeaponBase* /*pWeapon*/)
{
    return 0;
}

int CTicks::GetShotsWithinPacket(CTFWeaponBase* /*pWeapon*/, int /*iTicks*/)
{
    return 1;
}

int CTicks::GetMinimumTicksNeeded(CTFWeaponBase* /*pWeapon*/)
{
    return 1;
}

bool CTicks::CanChoke()
{
    return false;
}

void CTicks::SaveShootPos(CTFPlayer* /*pLocal*/)
{
    // No operation.
}

Vec3 CTicks::GetShootPos()
{
    return Vec3();
}

void CTicks::SaveShootAngle(CUserCmd* /*pCmd*/, bool /*bSendPacket*/)
{
    // No operation.
}

Vec3* CTicks::GetShootAngle()
{
    return nullptr;
}

void CMenu::AddOutput(const std::string& /*sFunction*/, const std::string& /*sLog*/, const Color_t& /*tColor*/)
{
    // No output in textmode.
}

#endif // TEXTMODE 