#include "../SDK/SDK.h"

MAKE_SIGNATURE(CBaseEntity_GetTeamNumber, "client.dll", "8B 81 EC 00 00 00 C3", 0x0);
MAKE_SIGNATURE(CBaseEntity_GetTeamNumber_retaddr, "client.dll", "83 F8 03 74 45", 0x0);

MAKE_HOOK(CBaseEntity_GetTeamNumber, S::CBaseEntity_GetTeamNumber(), int,
	void* thisptr)
{
    HOOK_TRY
#ifndef TEXTMODE
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::CBaseEntity_GetTeamNumber[DEFAULT_BIND])
		return CALL_ORIGINAL(thisptr);
#endif

	if (reinterpret_cast<uintptr_t>(_ReturnAddress()) == S::CBaseEntity_GetTeamNumber_retaddr() && Vars::Misc::MannVsMachine::RobotDeathAnims.Value)
		return 69696969;
#endif

	return CALL_ORIGINAL(thisptr);
    HOOK_CATCH("CBaseEntity_GetTeamNumber", int)
} 
