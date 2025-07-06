#ifdef TEXTMODE

#include "Features/Visuals/Visuals.h"
#include "Features/Aimbot/AutoRocketJump/AutoRocketJump.h"
#include "Features/Simulation/MovementSimulation/MovementSimulation.h"
#include "Features/ImGui/Render.h"
#include "SDK/Helpers/Draw/Draw.h"
#include "Features/Visuals/Materials/Materials.h"
#include "Features/Binds/Binds.h"
#include <cstdarg>
#include <vector>
#include "SDK/Definitions/Steam/SteamTypes.h"

// Forward declaration to avoid pulling d3d9 headers in TEXTMODE
struct IDirect3DDevice9;

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

void CMaterials::UnloadMaterials()
{
    // No materials to unload in textmode.
}

void CMaterials::ReloadMaterials()
{
    // No materials to reload in textmode.
}

void CMaterials::LoadMaterials()
{
    // No material loading in textmode.
}

// CDraw stubs
void CDraw::LineCircle(int /*x*/, int /*y*/, float /*radius*/, int /*segments*/, const Color_t& /*clr*/)
{
}

void CDraw::StringOutlined(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const Color_t& /*tColorOut*/, const EAlign& /*eAlign*/, const char* /*str*/, ...)
{
}

void CDraw::StringOutlined(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const Color_t& /*tColorOut*/, const EAlign& /*eAlign*/, const wchar_t* /*str*/, ...)
{
}

void CDraw::GradientRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const Color_t& /*tColorTop*/, const Color_t& /*tColorBottom*/, bool /*bHorizontal*/)
{
}

void CDraw::String(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const EAlign& /*eAlign*/, const char* /*str*/, ...)
{
}

void CDraw::String(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const EAlign& /*eAlign*/, const wchar_t* /*str*/, ...)
{
}

void CDraw::RenderWireframeBox(const Vec3& /*vOrigin*/, const Vec3& /*vMins*/, const Vec3& /*vMaxs*/, const Vec3& /*vAngles*/, Color_t /*tColor*/, bool /*bZBuffer*/)
{
}

void CDraw::RenderBox(const Vec3& /*vOrigin*/, const Vec3& /*vMins*/, const Vec3& /*vMaxs*/, const Vec3& /*vAngles*/, Color_t /*tColor*/, bool /*bZBuffer*/, bool /*bInsideOut*/)
{
}

void CDraw::RenderLine(const Vec3& /*vStart*/, const Vec3& /*vEnd*/, Color_t /*tColor*/, bool /*bZBuffer*/)
{
}

void CDraw::DrawHudTexture(float /*x*/, float /*y*/, float /*s*/, const CHudTexture* /*pTexture*/, Color_t /*tColor*/)
{
}

void CDraw::Texture(int /*x*/, int /*y*/, int /*w*/, int /*h*/, int /*iId*/, const EAlign& /*eAlign*/)
{
}

void CDraw::FillRectPercent(int /*x*/, int /*y*/, int /*w*/, int /*h*/, float /*t*/, const Color_t& /*tColor*/, const Color_t& /*tColorOut*/, const EAlign& /*eAlign*/, bool /*bAdjust*/)
{
}

void CDraw::LineRectOutline(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const Color_t& /*tColor*/, const Color_t& /*tColorOut*/, bool /*bInside*/)
{
}

void CDraw::FillRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const Color_t& /*tColor*/)
{
}

void CDraw::Line(int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/, const Color_t& /*tColor*/)
{
}

void CDraw::StringWithBackground(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const Color_t& /*tColorBg*/, const EAlign& /*eAlign*/, const char* /*str*/, ...)
{
}

void CDraw::StringWithBackground(const Font_t& /*tFont*/, int /*x*/, int /*y*/, const Color_t& /*tColor*/, const Color_t& /*tColorBg*/, const EAlign& /*eAlign*/, const wchar_t* /*str*/, ...)
{
}

void CDraw::LineRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const Color_t& /*tColor*/)
{
}

Vec2 CDraw::GetTextSize(const char* /*text*/, const Font_t& /*tFont*/)
{
    return {0, 0};
}

Vec2 CDraw::GetTextSize(const wchar_t* /*text*/, const Font_t& /*tFont*/)
{
    return {0, 0};
}

void CDraw::FillPolygon(std::vector<Vertex_t> /*vVertices*/, const Color_t& /*tColor*/)
{
}

void CDraw::FillRoundRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/, int /*iRadius*/, const Color_t& /*tColor*/, int /*iCount*/)
{
}

CHudTexture* CDraw::GetIcon(const char* /*szIcon*/, int /*eIconFormat*/)
{
    return nullptr;
}

// CBinds stub
void CBinds::Run(CTFPlayer* /*pLocal*/, CTFWeaponBase* /*pWeapon*/)
{
    // No bind processing in textmode.
}

void CDraw::FillRectOutline(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const Color_t& /*tColor*/, const Color_t& /*tColorOut*/)
{
}

void CDraw::LinePolygon(std::vector<Vertex_t> /*vVertices*/, const Color_t& /*tColor*/)
{
}

void CDraw::Avatar(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const uint32 /*nFriendID*/, const EAlign& /*eAlign*/)
{
}

#endif // TEXTMODE 