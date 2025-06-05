#include "AutoJoin.h"

// Doesnt work with custom huds!1!!
void CAutoJoin::Run(CTFPlayer* pLocal)
{
	static Timer tJoinTimer{};

	if (Vars::Misc::Automation::ForceClass.Value && pLocal)
	{
		if (tJoinTimer.Run(1.f))
		{
			if (pLocal->IsInValidTeam())
			{
				I::EngineClient->ClientCmd_Unrestricted(std::format("joinclass {}", m_aClassNames[Vars::Misc::Automation::ForceClass.Value - 1]).c_str());
				I::EngineClient->ClientCmd_Unrestricted("menuclosed");
			}
			else
			{
				I::EngineClient->ClientCmd_Unrestricted("team_ui_setup");
				I::EngineClient->ClientCmd_Unrestricted("menuopen");
				I::EngineClient->ClientCmd_Unrestricted("autoteam");
				I::EngineClient->ClientCmd_Unrestricted("menuclosed");
			}
		}
	}
}