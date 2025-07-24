#include "../SDK/SDK.h"

MAKE_SIGNATURE(CTFPartyClient_BCanRequestToJoinPlayer, "client.dll", "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC ? 48 8B E9 48 89 54 24 ? 48 8B 49", 0x0);

MAKE_HOOK(CTFPartyClient_BCanRequestToJoinPlayer, S::CTFPartyClient_BCanRequestToJoinPlayer(), bool, CTFPartyClient* rcx, int64 targetPlayer)
{
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::CTFPartyClient_BCanRequestToJoinPlayer[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, targetPlayer);
#endif

	return true;
}