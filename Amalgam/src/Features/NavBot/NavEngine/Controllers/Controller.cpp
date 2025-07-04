#include "Controller.h"
#include "CPController/CPController.h"
#include "FlagController/FlagController.h"
#include "PLController/PLController.h"
#include "MPController/MPController.h"

ETFGameType GetGameType()
{
	auto map_name = std::string(I::EngineClient->GetLevelName());
	F::GameObjectiveController.m_bDoomsday = map_name.find("sd_doomsday") != std::string::npos;
	
	F::GameObjectiveController.m_bMannpower = (
		map_name.find("ctf_gorge") != std::string::npos ||
		map_name.find("ctf_hellfire") != std::string::npos ||
		map_name.find("ctf_thundermountain") != std::string::npos ||
		map_name.find("ctf_foundry") != std::string::npos
	);

	int iType = TF_GAMETYPE_UNDEFINED;
	if (auto pGameRules = I::TFGameRules())
		iType = pGameRules->m_nGameType();

	return static_cast<ETFGameType>(iType);
}

void CGameObjectiveController::Update()
{
	static Timer tUpdateGameType;
	if (m_eGameMode == TF_GAMETYPE_UNDEFINED || tUpdateGameType.Run(1.f))
		m_eGameMode = GetGameType();
	
	if (m_bMannpower)
	{
		F::MPController.Update();
		return;
	}

	switch (m_eGameMode)
	{
	case TF_GAMETYPE_CTF:
		F::FlagController.Update();
		break;
	case TF_GAMETYPE_CP:
		F::CPController.Update();
		break;
	case TF_GAMETYPE_ESCORT:
		F::PLController.Update();
		break;
	case TF_GAMETYPE_ARENA:
	{
		// Versus Saxton Hale maps use Arena gametype but behave like single control point capture
		std::string map_name = SDK::GetLevelName();
		std::transform(map_name.begin(), map_name.end(), map_name.begin(), [](unsigned char c){ return std::tolower(c); });
		if (map_name.rfind("vsh_", 0) == 0)
		{
			F::CPController.Update();
		}
		break;
	}
	default:
		if (m_bDoomsday)
		{
			F::FlagController.Update();
			F::CPController.Update();
		}
		break;
	}
}

void CGameObjectiveController::Reset()
{
	m_eGameMode = TF_GAMETYPE_UNDEFINED;
	F::FlagController.Init();
	F::PLController.Init();
	F::CPController.Init();
	F::MPController.Init();
}
