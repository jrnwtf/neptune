#include "../SDK/SDK.h"

#include "../Features/Aimbot/Aimbot.h"
#include "../Features/Backtrack/Backtrack.h"
#include "../Features/CheaterDetection/CheaterDetection.h"
#include "../Features/CritHack/CritHack.h"
#include "../Features/Players/PlayerUtils.h"
#include "../Features/Simulation/MovementSimulation/MovementSimulation.h"
#include "../Features/Visuals/Visuals.h"
#include "../Features/Visuals/ESP/ESP.h"
#include "../Features/Visuals/Chams/Chams.h"
#include "../Features/Visuals/Glow/Glow.h"
#include "../Features/Visuals/SentryESP/SentryESP.h"
#include "../Features/Visuals/StickyESP/StickyESP.h"
#include "../Features/Visuals/StickyCam/StickyCam.h"
#include "../Features/Visuals/FocusFire/FocusFire.h"
#include "../Features/Spectate/Spectate.h"
#include "../Features/Binds/Binds.h"

MAKE_HOOK(IBaseClientDLL_FrameStageNotify, U::Memory.GetVirtual(I::BaseClientDLL, 35), void,
	void* rcx, ClientFrameStage_t curStage)
{
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::IBaseClientDLL_FrameStageNotify[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, curStage);
#endif

	if (G::Unload)
		return CALL_ORIGINAL(rcx, curStage);

	CALL_ORIGINAL(rcx, curStage);

	switch (curStage)
	{
	case FRAME_NET_UPDATE_START:
	{
		auto pLocal = H::Entities.GetLocal();
		F::Spectate.NetUpdateStart(pLocal);

		H::Entities.Clear();
		break;
	}
	case FRAME_NET_UPDATE_END:
	{
		H::Entities.Store();
		F::PlayerUtils.UpdatePlayers();

		F::Backtrack.Store();
		F::MoveSim.Store();
		F::CritHack.Store();
		F::Aimbot.Store();

		auto pLocal = H::Entities.GetLocal();
		
		// Update competitive feature chams entities before chams system runs
		F::SentryESP.UpdateChamsEntities();
		F::StickyESP.UpdateChamsEntities();
		F::StickyCam.UpdateChamsEntities();
		F::FocusFire.UpdateChamsEntities();
		
		F::ESP.Store(pLocal);
		F::Chams.Store(pLocal);
		F::Glow.Store(pLocal);
		F::Visuals.Store(pLocal);

		F::CheaterDetection.Run();
		F::Spectate.NetUpdateEnd(pLocal);

		// Disable freezecam feature
		if (Vars::Competitive::Features::DisableFreezeCam.Value && pLocal && pLocal->m_iObserverMode() == OBS_MODE_FREEZECAM)
		{
			// Use third person by default to avoid inheriting player viewangles
			pLocal->m_iObserverMode() = OBS_MODE_THIRDPERSON;
			// Also reset observer target to prevent freezecam lock
			pLocal->m_hObserverTarget().Set(nullptr);
		}

		F::Visuals.Modulate();
		F::Visuals.DrawHitboxes(1);
		break;
	}
	case FRAME_RENDER_START:
		for (auto it = F::Binds.m_vBinds.begin(); it < F::Binds.m_vBinds.end(); it++)
		{	// don't drop inputs for binds
			auto& tBind = *it;
			if (tBind.m_iType != BindEnum::Key)
				continue;

			auto& tKey = tBind.m_tKeyStorage;

			bool bOldIsDown = tKey.m_bIsDown;
			bool bOldIsPressed = tKey.m_bIsPressed;
			bool bOldIsDouble = tKey.m_bIsDouble;
			bool bOldIsReleased = tKey.m_bIsReleased;

			U::KeyHandler.StoreKey(tBind.m_iKey, &tKey);

			tKey.m_bIsDown = tKey.m_bIsDown || bOldIsDown;
			tKey.m_bIsPressed = tKey.m_bIsPressed || bOldIsPressed;
			tKey.m_bIsDouble = tKey.m_bIsDouble || bOldIsDouble;
			tKey.m_bIsReleased = tKey.m_bIsReleased || bOldIsReleased;
		}
	}
}