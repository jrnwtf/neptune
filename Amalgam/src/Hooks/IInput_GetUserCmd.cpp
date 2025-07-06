#include "../SDK/SDK.h"

MAKE_HOOK(IInput_GetUserCmd, U::Memory.GetVFunc(I::Input, 8), CUserCmd*,
	void* rcx, int sequence_number)
{
    HOOK_TRY
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::IInput_GetUserCmd[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, sequence_number);
#endif

	return &I::Input->GetCommands()[sequence_number % MULTIPLAYER_BACKUP];
    HOOK_CATCH("IInput_GetUserCmd", CUserCmd*)
}
