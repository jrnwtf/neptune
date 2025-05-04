#pragma once
#include "../../../SDK/SDK.h"
#include "../../../SDK/Events/Events.h"

enum EVaccinatorResist
{
	VACC_BULLET = 0,
	VACC_BLAST,
	VACC_FIRE,
	VACC_NONE
};

class CAutoHeal
{
	bool IsValidHealTarget(CTFPlayer* pLocal, CBaseEntity* pEntity);
	bool ActivateOnVoice(CTFPlayer* pLocal, CWeaponMedigun* pWeapon, CUserCmd* pCmd);
	bool RunAutoUber(CTFPlayer* pLocal, CWeaponMedigun* pWeapon, CUserCmd* pCmd);
	bool ShouldPop(CTFPlayer* pLocal, CWeaponMedigun* pWeapon);
	bool ShouldPopOnHealth(CTFPlayer* pLocal, CWeaponMedigun* pWeapon);
	bool ShouldPopOnEnemyCount(CTFPlayer* pLocal, CWeaponMedigun* pWeapon);
	bool ShouldPopOnDangerProjectiles(CTFPlayer* pLocal, CWeaponMedigun* pWeapon);
	bool RunVaccinator(CTFPlayer* pLocal, CWeaponMedigun* pWeapon, CUserCmd* pCmd);
	void SwitchResistance(CWeaponMedigun* pWeapon, CUserCmd* pCmd, EVaccinatorResist eResist);
	EVaccinatorResist GetCurrentResistType(CWeaponMedigun* pWeapon);
	EVaccinatorResist GetBestResistType(CTFPlayer* pLocal);
	EVaccinatorResist GetResistTypeForClass(int nClass);
	std::vector<EVaccinatorResist> GetResistTypesFromNearbyEnemies(CTFPlayer* pLocal, float flRange);
	void CycleToNextResistance(CWeaponMedigun* pWeapon, CUserCmd* pCmd);
	std::vector<EVaccinatorResist> m_vActiveResistances;

	struct DamageRecord_t
	{
		EVaccinatorResist Type;
		float Amount;
		float Time;
	};

	std::deque<DamageRecord_t> m_DamageRecords;
	float m_flLastSwitchTime = 0.0f;
	EVaccinatorResist m_eCurrentAutoResist = VACC_NONE;
	int m_iCurrentResistIndex = 0;
	float m_flLastCycleTime = 0.0f;

public:
	void Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd);
	void Event(IGameEvent* pEvent, uint32_t uHash, CTFPlayer* pLocal);
	std::unordered_map<int, bool> m_mMedicCallers = {};
};

ADD_FEATURE(CAutoHeal, AutoHeal)