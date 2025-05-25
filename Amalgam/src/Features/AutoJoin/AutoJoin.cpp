#include "AutoJoin.h"
#include <random>

int CAutoJoin::GetRandomClass()
{
	static std::random_device rd;
	static std::mt19937 rng(rd());

	std::vector<int> availableClasses;
	for (int i = 1; i <= 9; i++)
	{
		bool excluded = false;
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 0)) && i == 3) excluded = true; // Soldier
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 1)) && i == 4) excluded = true; // Demoman
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 2)) && i == 7) excluded = true; // Pyro
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 3)) && i == 5) excluded = true; // Medic

		if (!excluded)
			availableClasses.push_back(i);
	}

	if (!availableClasses.empty())
	{
		std::uniform_int_distribution<int> dist(0, availableClasses.size() - 1);
		int randomIndex = dist(rng);
		return availableClasses[randomIndex];
	}
	return 1;
}

// Doesnt work with custom huds!1!!
void CAutoJoin::Run(CTFPlayer* pLocal)
{
	static Timer tJoinTimer{};
	static Timer tRandomClassTimer{};

	if ((Vars::Misc::Automation::ForceClass.Value || Vars::Misc::Automation::RandomClassSwitch.Value) && pLocal)
	{
		if (tJoinTimer.Run(1.f))
		{
			if (Vars::Misc::Automation::RandomClassSwitch.Value)
			{
				bool needsInitialClassSelect = (pLocal->m_iTeamNum() == TF_TEAM_RED || pLocal->m_iTeamNum() == TF_TEAM_BLUE) && 
					pLocal->m_iClass() == 0; // Class is not selected yet

				float switchInterval = Vars::Misc::Automation::RandomClassInterval.Value * 60.f; // Convert minutes to seconds
				
				if (needsInitialClassSelect || tRandomClassTimer.Run(switchInterval))
				{
					// Select random class
					int selectedClass = GetRandomClass();
					
					if (pLocal->m_iTeamNum() == TF_TEAM_RED || pLocal->m_iTeamNum() == TF_TEAM_BLUE)
					{
						I::EngineClient->ClientCmd_Unrestricted(std::format("joinclass {}", m_aClassNames[selectedClass - 1]).c_str());
						I::EngineClient->ClientCmd_Unrestricted("menuclosed");
					}
				}
			}
			else if (Vars::Misc::Automation::ForceClass.Value)
			{
				if (pLocal->m_iTeamNum() == TF_TEAM_RED || pLocal->m_iTeamNum() == TF_TEAM_BLUE)
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
}