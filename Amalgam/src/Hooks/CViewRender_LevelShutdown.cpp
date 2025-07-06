#ifndef TEXTMODE
#include "../SDK/SDK.h"

#include "../Features/Visuals/Materials/Materials.h"
#include "../Features/Killstreak/Killstreak.h"
#include "../Features/Spectate/Spectate.h"

MAKE_HOOK(CViewRender_LevelShutdown, U::Memory.GetVFunc(I::ViewRender, 2), void,
	void* rcx)
{
    HOOK_TRY
	F::Materials.UnloadMaterials();
	F::Killstreak.Reset();
	F::Spectate.m_iIntendedTarget = -1;

	CALL_ORIGINAL(rcx);
    HOOK_CATCH("CViewRender_LevelShutdown", void)
}
#endif
