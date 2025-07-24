#include "../SDK/SDK.h"
#include "../Features/Visuals/Materials/Materials.h"

MAKE_SIGNATURE(CBaseClient_Disconnect, "engine.dll", "48 8B C4 48 89 50 ? 4C 89 40 ? 4C 89 48 ? 57 48 81 EC ? ? ? ? 83 B9 ? ? ? ? ? 48 8B F9 0F 84 ? ? ? ? 48 89 58", 0x0);
//MAKE_SIGNATURE(CBaseClient_Disconnect, "engine.dll", "E8 ? ? ? ? 48 8B B4 24 ? ? ? ? 48 81 C4 ? ? ? ? 5F", 0x0);

MAKE_HOOK(CBaseClient_Disconnect, S::CBaseClient_Disconnect(), void, void* rcx, const char* fmt, ...)
{
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::CBaseClient_Disconnect[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, fmt);
#endif

	F::Materials.UnloadMaterials();
	CALL_ORIGINAL(rcx, fmt);
}