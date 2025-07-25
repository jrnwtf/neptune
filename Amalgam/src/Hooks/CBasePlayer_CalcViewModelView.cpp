#include "../SDK/SDK.h"

MAKE_SIGNATURE(CBasePlayer_CalcViewModelView, "client.dll", "48 89 74 24 ? 55 41 56 41 57 48 8D AC 24", 0x0);

MAKE_HOOK(CBasePlayer_CalcViewModelView, S::CBasePlayer_CalcViewModelView(), void,
	void* rcx, CBaseEntity* pOwner, const Vec3& vEyePosition, Vec3& vEyeAngles)
{
#ifndef TEXTMODE
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::CBasePlayer_CalcViewModelView[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, pOwner, vEyePosition, vEyeAngles);
#endif

	Vec3 vOffset = { float(Vars::Visuals::Viewmodel::OffsetX.Value), float(Vars::Visuals::Viewmodel::OffsetY.Value), float(Vars::Visuals::Viewmodel::OffsetZ.Value) };
	bool bOffset = !vOffset.IsZero();

	if (!Vars::Visuals::Viewmodel::ViewmodelAim.Value && !bOffset && !Vars::Visuals::Viewmodel::Roll.Value
		|| Vars::Visuals::UI::CleanScreenshots.Value && I::EngineClient->IsTakingScreenshot())
		return CALL_ORIGINAL(rcx, pOwner, vEyePosition, vEyeAngles);

	bool bFlip = false;
	{
		static auto cl_flipviewmodels = U::ConVars.FindVar("cl_flipviewmodels");
		if (cl_flipviewmodels->GetBool())
			bFlip = !bFlip;
		if (G::FlipViewmodels)
			bFlip = !bFlip;
	}

	if (Vars::Visuals::Viewmodel::ViewmodelAim.Value)
	{
		auto pLocal = H::Entities.GetLocal();
		if (pLocal && pLocal->IsAlive() && G::AimPoint.m_iTickCount)
		{
			Vec3 vDiff = I::EngineClient->GetViewAngles() - Math::CalcAngle(vEyePosition, G::AimPoint.m_vOrigin);
			if (bFlip)
				vDiff.y *= -1;
			vEyeAngles = I::EngineClient->GetViewAngles() - vDiff;
		}
	}

	Vec3 vNewEyePosition = vEyePosition;
	if (bOffset)
	{
		Vec3 vForward, vRight, vUp; Math::AngleVectors(vEyeAngles, &vForward, &vRight, &vUp);
		vNewEyePosition += vForward * vOffset.y;
		vNewEyePosition += vRight * vOffset.x * (bFlip ? -1 : 1);
		vNewEyePosition += vUp * vOffset.z;
	}
	if (Vars::Visuals::Viewmodel::Pitch.Value)
		vEyeAngles.x += Vars::Visuals::Viewmodel::Pitch.Value;
	if (Vars::Visuals::Viewmodel::Yaw.Value)
		vEyeAngles.y += Vars::Visuals::Viewmodel::Yaw.Value * (bFlip ? -1 : 1);
	if (Vars::Visuals::Viewmodel::Roll.Value)
		vEyeAngles.z += Vars::Visuals::Viewmodel::Roll.Value * (bFlip ? -1 : 1);

	CALL_ORIGINAL(rcx, pOwner, vNewEyePosition, vEyeAngles);
#endif
}

MAKE_HOOK(ClientModeTFNormal_GetViewModelFOV, U::Memory.GetVirtual(I::ClientModeShared, 32), float,
	/*void* rcx*/)
{
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::CBasePlayer_CalcViewModelView[DEFAULT_BIND])
		return CALL_ORIGINAL(/*rcx*/);
#endif
	return Vars::Visuals::Viewmodel::FieldOfView.Value ? Vars::Visuals::Viewmodel::FieldOfView.Value : CALL_ORIGINAL(/*rcx*/);
}