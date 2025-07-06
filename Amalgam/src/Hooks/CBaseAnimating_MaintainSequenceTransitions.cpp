#include "../SDK/SDK.h"

MAKE_SIGNATURE(CBaseAnimating_MaintainSequenceTransitions, "client.dll", "4C 89 4C 24 ? 41 56", 0x0);

MAKE_HOOK(CBaseAnimating_MaintainSequenceTransitions, S::CBaseAnimating_MaintainSequenceTransitions(), void,
	CBaseAnimating* rcx, void* boneSetup, float flCycle, Vec3 pos[], Vector4D q[])
{
    HOOK_TRY
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::CBaseAnimating_MaintainSequenceTransitions[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, boneSetup, flCycle, pos, q);
#endif
    HOOK_CATCH("CBaseAnimating_MaintainSequenceTransitions", void)
}
