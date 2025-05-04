#pragma once
#include "../../../../../SDK/SDK.h"
#include "../FlagController/FlagController.h"

struct PowerupInfo
{
	CBaseEntity* m_pPowerup = nullptr;
	PowerupBottleType_t m_Type = POWERUP_BOTTLE_NONE;
	Vector m_vPosition;

	PowerupInfo() = default;
	PowerupInfo(CBaseEntity* pPowerup, PowerupBottleType_t type, const Vector& vPos)
	{
		m_pPowerup = pPowerup;
		m_Type = type;
		m_vPosition = vPos;
	}
};

class CMPController
{
private:
	std::vector<PowerupInfo> m_vPowerups;
	CFlagController* m_pFlagController = nullptr;

public:
	std::vector<PowerupInfo> GetPowerups() { return m_vPowerups; }
	std::optional<Vector> GetPowerupPosition(PowerupBottleType_t type);
	std::optional<PowerupInfo> GetClosestPowerup(const Vector& vPosition);
	FlagInfo GetFlag(int team);
	Vector GetPosition(CCaptureFlag* pFlag);
	std::optional<Vector> GetPosition(int iTeam);
	std::optional<Vector> GetSpawnPosition(int iTeam);
	int GetCarrier(CCaptureFlag* pFlag);
	int GetCarrier(int iTeam);
	int GetStatus(CCaptureFlag* pFlag);
	int GetStatus(int iTeam);

	void Init();
	void Update();
};

ADD_FEATURE(CMPController, MPController)
