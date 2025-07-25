#include "../SDK/SDK.h"

std::unordered_map<void*, std::pair<int, float>> pAnimatingInfo;

MAKE_HOOK(CBaseAnimating_FrameAdvance, S::CBaseAnimating_FrameAdvance(), float, void* rcx, float flInterval)
{
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::CBaseAnimating_FrameAdvance[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, flInterval);
#endif

	if (!Vars::Visuals::Removals::Interpolation.Value || rcx == H::Entities.GetLocal())
		return CALL_ORIGINAL(rcx, flInterval);

	const auto pEntity = static_cast<CBaseEntity*>(rcx);

	if (pEntity && pEntity->IsPlayer())
	{
		if (pEntity->m_flSimulationTime() == pEntity->m_flOldSimulationTime() || I::GlobalVars->tickcount == pAnimatingInfo[rcx].first)
		{
			pAnimatingInfo[rcx].second += flInterval;
			return 0.f;
		}
	}

	flInterval = pAnimatingInfo[rcx].second; pAnimatingInfo[rcx].second = 0.f; pAnimatingInfo[rcx].first = I::GlobalVars->tickcount;
	return CALL_ORIGINAL(rcx, flInterval);
}