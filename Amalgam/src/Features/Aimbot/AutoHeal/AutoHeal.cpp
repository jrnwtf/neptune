#include "AutoHeal.h"

#include "../../Players/PlayerUtils.h"
#include "../../NavBot/FollowBot.h"

bool CAutoHeal::IsValidHealTarget(CTFPlayer* pLocal, CBaseEntity* pEntity)
{
	if (!pEntity)
		return false;
	
	if (!pEntity->IsPlayer())
		return false;
	
	CTFPlayer* pTarget = pEntity->As<CTFPlayer>();
	if (!pTarget || !pTarget->IsAlive())
		return false;
	
	if (Vars::Aimbot::Healing::FriendsOnly.Value && 
	    !H::Entities.IsFriend(pTarget->entindex()) && 
	    !H::Entities.InParty(pTarget->entindex()))
		return false;
	
	if (Vars::Aimbot::Healing::FBotTargetOnly.Value)
	{
		if (F::FollowBot.m_iTargetIndex == -1)
			return false;
		
		return (pTarget->entindex() == F::FollowBot.m_iTargetIndex);
	}
	
	return true;
}

bool CAutoHeal::ActivateOnVoice(CTFPlayer* pLocal, CWeaponMedigun* pWeapon, CUserCmd* pCmd)
{
	if (!Vars::Aimbot::Healing::ActivateOnVoice.Value)
		return false;

	auto pTarget = pWeapon->m_hHealingTarget().Get();
	if (!IsValidHealTarget(pLocal, pTarget))
		return false;

	bool bReturn = m_mMedicCallers.contains(pTarget->entindex());
	if (bReturn)
		pCmd->buttons |= IN_ATTACK2;

	return bReturn;
}

EVaccinatorResist CAutoHeal::GetCurrentResistType(CWeaponMedigun* pWeapon)
{
	int nResistType = pWeapon->m_nChargeResistType();
	
	switch (nResistType)
	{
	case 0: return VACC_BULLET;
	case 1: return VACC_BLAST;
	case 2: return VACC_FIRE;
	default: return VACC_BULLET; // Default to bullet
	}
}

void CAutoHeal::SwitchResistance(CWeaponMedigun* pWeapon, CUserCmd* pCmd, EVaccinatorResist eResist)
{
	EVaccinatorResist eCurrentResist = GetCurrentResistType(pWeapon);
	
	if (eCurrentResist == eResist)
		return;
	
	int nClicks = 0;
	switch (eCurrentResist)
	{
	case VACC_BULLET:
		nClicks = (eResist == VACC_BLAST) ? 1 : 2; // BULLET -> BLAST = 1, BULLET -> FIRE = 2
		break;
	case VACC_BLAST:
		nClicks = (eResist == VACC_FIRE) ? 1 : 2; // BLAST -> FIRE = 1, BLAST -> BULLET = 2
		break;
	case VACC_FIRE:
		nClicks = (eResist == VACC_BULLET) ? 1 : 2; // FIRE -> BULLET = 1, FIRE -> BLAST = 2
		break;
	}
	
	pCmd->buttons |= IN_RELOAD;
	
	m_flLastSwitchTime = I::GlobalVars->curtime;
	m_eCurrentAutoResist = eResist;
}

void CAutoHeal::CycleToNextResistance(CWeaponMedigun* pWeapon, CUserCmd* pCmd)
{
	if (m_vActiveResistances.empty())
	{
		m_vActiveResistances.push_back(VACC_BULLET);
		m_iCurrentResistIndex = 0;
		return;
	}
	
	m_iCurrentResistIndex = (m_iCurrentResistIndex + 1) % m_vActiveResistances.size();
	EVaccinatorResist eNextResist = m_vActiveResistances[m_iCurrentResistIndex];
	SwitchResistance(pWeapon, pCmd, eNextResist);
	m_flLastCycleTime = I::GlobalVars->curtime;
}

EVaccinatorResist CAutoHeal::GetResistTypeForClass(int nClass)
{
	switch (nClass)
	{
	case TF_CLASS_SCOUT:
	case TF_CLASS_HEAVY:
	case TF_CLASS_SNIPER:
		return VACC_BULLET;
	
	case TF_CLASS_SOLDIER:
	case TF_CLASS_DEMOMAN:
		return VACC_BLAST;
	
	case TF_CLASS_PYRO:
		return VACC_FIRE;
	
	case TF_CLASS_ENGINEER: // Engineer has sentry (bullet) but also shotgun
	case TF_CLASS_MEDIC:    // Medic's syringe gun is projectile but acts like hitscan
	case TF_CLASS_SPY:      // Spy's revolver is hitscan
	default:
		return VACC_BULLET;
	}
}

std::vector<EVaccinatorResist> CAutoHeal::GetResistTypesFromNearbyEnemies(CTFPlayer* pLocal, float flRange)
{
	std::vector<EVaccinatorResist> resistTypes;
	std::unordered_map<EVaccinatorResist, int> resistCounts;
	for (int i = 1; i <= I::GlobalVars->maxClients; i++)
	{
		auto pEntity = I::ClientEntityList->GetClientEntity(i);
		if (!pEntity || !pEntity->IsPlayer())
			continue;
		
		CTFPlayer* pPlayer = pEntity->As<CTFPlayer>();
		if (!pPlayer || !pPlayer->IsAlive() || pPlayer->m_iTeamNum() == pLocal->m_iTeamNum())
			continue;
		
		Vector vLocalPos = pLocal->GetAbsOrigin();
		Vector vEnemyPos = pPlayer->GetAbsOrigin();
		float flDistance = vLocalPos.DistTo(vEnemyPos);
		
		if (flDistance <= flRange)
		{
			EVaccinatorResist resistType = GetResistTypeForClass(pPlayer->m_iClass());
			resistCounts[resistType]++;
		}
	}
	
	std::vector<std::pair<EVaccinatorResist, int>> sortedResists;
	for (const auto& pair : resistCounts)
	{
		sortedResists.push_back(pair);
	}
	
	std::sort(sortedResists.begin(), sortedResists.end(), 
		[](const std::pair<EVaccinatorResist, int>& a, const std::pair<EVaccinatorResist, int>& b) {
			return a.second > b.second;
		});
	
	for (const auto& pair : sortedResists)
	{
		if (pair.second > 0)
		{
			resistTypes.push_back(pair.first);
		}
	}
	
	if (resistTypes.empty())
	{
		resistTypes.push_back(VACC_BULLET);
	}
	
	return resistTypes;
}

EVaccinatorResist CAutoHeal::GetBestResistType(CTFPlayer* pLocal)
{
	if (Vars::Aimbot::Healing::VaccinatorClassBased.Value)
	{
		auto resistTypes = GetResistTypesFromNearbyEnemies(pLocal, Vars::Aimbot::Healing::VaccinatorRange.Value);
		m_vActiveResistances = resistTypes;
		if (!resistTypes.empty())
		{
			return resistTypes[0];
		}
	}

	// Fall back to damage-based detection if class-based detection is disabled or no enemies found
	// Remove old damage records (older than 5 seconds)
	const float MAX_RECORD_AGE = 5.0f;
	while (!m_DamageRecords.empty() && (I::GlobalVars->curtime - m_DamageRecords.front().Time) > MAX_RECORD_AGE)
	{
		m_DamageRecords.pop_front();
	}
	
	// Count damage by type
	float bulletDamage = 0.0f;
	float blastDamage = 0.0f;
	float fireDamage = 0.0f;
	
	for (const auto& record : m_DamageRecords)
	{
		switch (record.Type)
		{
		case VACC_BULLET: bulletDamage += record.Amount; break;
		case VACC_BLAST: blastDamage += record.Amount; break;
		case VACC_FIRE: fireDamage += record.Amount; break;
		}
	}
	m_vActiveResistances.clear();
	
	// Sort damage types by amount
	std::vector<std::pair<EVaccinatorResist, float>> sortedDamage = {
		{VACC_BULLET, bulletDamage},
		{VACC_BLAST, blastDamage},
		{VACC_FIRE, fireDamage}
	};
	
	std::sort(sortedDamage.begin(), sortedDamage.end(),
		[](const std::pair<EVaccinatorResist, float>& a, const std::pair<EVaccinatorResist, float>& b) {
			return a.second > b.second;
		});
	
	for (const auto& pair : sortedDamage)
	{
		if (pair.second > 0.0f)
		{
			m_vActiveResistances.push_back(pair.first);
		}
	}
	
	// If no damage records, default to bullet
	if (m_vActiveResistances.empty())
	{
		m_vActiveResistances.push_back(VACC_BULLET);
	}
	
	return m_vActiveResistances[0];
}

bool CAutoHeal::RunVaccinator(CTFPlayer* pLocal, CWeaponMedigun* pWeapon, CUserCmd* pCmd)
{
	if (!Vars::Aimbot::Healing::AutoVaccinator.Value)
		return false;
	if (pWeapon->m_iItemDefinitionIndex() != 998) // Vaccinator item def index
		return false;
	auto pTarget = pWeapon->m_hHealingTarget().Get();
	if (!IsValidHealTarget(pLocal, pTarget))
		return false;
	
	int nMode = Vars::Aimbot::Healing::VaccinatorMode.Value;
	if (nMode == Vars::Aimbot::Healing::VaccinatorModeEnum::Manual)
	{
		EVaccinatorResist eResist;
		switch (Vars::Aimbot::Healing::VaccinatorResist.Value)
		{
		case Vars::Aimbot::Healing::VaccinatorResistEnum::Bullet: eResist = VACC_BULLET; break;
		case Vars::Aimbot::Healing::VaccinatorResistEnum::Blast: eResist = VACC_BLAST; break;
		case Vars::Aimbot::Healing::VaccinatorResistEnum::Fire: eResist = VACC_FIRE; break;
		default: eResist = VACC_BULLET; break;
		}
		
		SwitchResistance(pWeapon, pCmd, eResist);
		return true;
	}
	if (nMode == Vars::Aimbot::Healing::VaccinatorModeEnum::Auto)
	{
		GetBestResistType(pLocal);
		if (Vars::Aimbot::Healing::VaccinatorMultiResist.Value && m_vActiveResistances.size() > 1)
		{
			if ((I::GlobalVars->curtime - m_flLastCycleTime) >= Vars::Aimbot::Healing::VaccinatorDelay.Value)
			{
				CycleToNextResistance(pWeapon, pCmd);
			}
			return true;
		}
		
		else
		{
			if ((I::GlobalVars->curtime - m_flLastSwitchTime) < Vars::Aimbot::Healing::VaccinatorDelay.Value)
				return true;
			EVaccinatorResist eBestResist = !m_vActiveResistances.empty() ? m_vActiveResistances[0] : VACC_BULLET;
			if (eBestResist != GetCurrentResistType(pWeapon))
			{
				SwitchResistance(pWeapon, pCmd, eBestResist);
			}
		}
		
		return true;
	}
	
	return false;
}
void CAutoHeal::Event(IGameEvent* pEvent, uint32_t uHash, CTFPlayer* pLocal)
{
    if (!pEvent || !pLocal || !I::EngineClient || !I::GlobalVars)
        return;

    if (!pLocal->IsAlive() || pLocal->m_iClass() != TF_CLASS_MEDIC)
        return;

    if (strcmp(pEvent->GetName(), "player_hurt") != 0)
        return;

    if (!Vars::Aimbot::Healing::AutoVaccinator.Value || !Vars::Aimbot::Healing::VaccinatorSmart.Value)
        return;

    int userid_val = pEvent->GetInt("userid", 0);
    if (userid_val <= 0)
        return;

    int iVictim = I::EngineClient->GetPlayerForUserID(userid_val);
    if (iVictim <= 0 || iVictim != pLocal->entindex())
        return;

    int nDamage = pEvent->GetInt("damageamount", 0);
    if (nDamage <= 0)
        return;

    EVaccinatorResist eResistType = VACC_BULLET;
    int iDamageType = pEvent->GetInt("damagetype", 0);
    
    if (iDamageType & DMG_BURN || iDamageType & DMG_IGNITE || iDamageType & DMG_PLASMA)
        eResistType = VACC_FIRE;
    else if (iDamageType & DMG_BLAST || iDamageType & DMG_BLAST_SURFACE || iDamageType & DMG_SONIC)
        eResistType = VACC_BLAST;

    int nWeaponID = pEvent->GetInt("weaponid", 0);
    
    if (nWeaponID == TF_WEAPON_ROCKETLAUNCHER || 
        nWeaponID == TF_WEAPON_GRENADELAUNCHER || 
        nWeaponID == TF_WEAPON_PIPEBOMBLAUNCHER)
        eResistType = VACC_BLAST;
    else if (nWeaponID == TF_WEAPON_FLAMETHROWER || 
             nWeaponID == TF_WEAPON_FLAREGUN)
        eResistType = VACC_FIRE;

    if (I::GlobalVars->curtime > 0.0f)
    {
        DamageRecord_t record;
        record.Type = eResistType;
        record.Amount = static_cast<float>(nDamage);
        record.Time = I::GlobalVars->curtime;
        m_DamageRecords.push_back(record);
    }
}

bool CAutoHeal::ShouldPopOnHealth(CTFPlayer* pLocal, CWeaponMedigun* pWeapon)
{
	if (!pWeapon)
		return false;
	
	// Get health threshold percentage
	float flThreshold = Vars::Aimbot::Healing::HealthThreshold.Value / 100.0f;
	
	// Check medic's own health if enabled
	if (Vars::Aimbot::Healing::SelfLowHealth.Value)
	{
		int iMaxHealth = pLocal->GetMaxHealth();
		int iCurrentHealth = pLocal->m_iHealth();
		
		if (iCurrentHealth <= iMaxHealth * flThreshold)
			return true;
	}
	
	// Check patient's health if enabled
	if (Vars::Aimbot::Healing::PatientLowHealth.Value)
	{
		auto pTarget = pWeapon->m_hHealingTarget().Get();
		if (pTarget && pTarget->IsPlayer())
		{
			CTFPlayer* pPlayerTarget = pTarget->As<CTFPlayer>();
			if (pPlayerTarget && pPlayerTarget->IsAlive())
			{
				int iMaxHealth = pPlayerTarget->GetMaxHealth();
				int iCurrentHealth = pPlayerTarget->m_iHealth();
				
				if (iCurrentHealth <= iMaxHealth * flThreshold)
					return true;
			}
		}
	}
	
	return false;
}

bool CAutoHeal::ShouldPopOnEnemyCount(CTFPlayer* pLocal, CWeaponMedigun* pWeapon)
{
	if (!Vars::Aimbot::Healing::PopOnMultipleEnemies.Value)
		return false;
	
	int iEnemyThreshold = Vars::Aimbot::Healing::EnemyCountThreshold.Value;
	
	// Get our position (use patient position if we're healing someone)
	Vector vPosition = pLocal->GetAbsOrigin();
	auto pTarget = pWeapon->m_hHealingTarget().Get();
	if (pTarget && pTarget->IsPlayer())
	{
		CTFPlayer* pPlayerTarget = pTarget->As<CTFPlayer>();
		if (pPlayerTarget && pPlayerTarget->IsAlive())
		{
			vPosition = pPlayerTarget->GetAbsOrigin();
		}
	}
	
	// Count nearby enemies
	int iEnemyCount = 0;
	float flMaxDistance = 800.0f; // Consider enemies within this range
	
	for (int i = 1; i <= I::GlobalVars->maxClients; i++)
	{
		auto pEntity = I::ClientEntityList->GetClientEntity(i);
		if (!pEntity || !pEntity->IsPlayer())
			continue;
		
		CTFPlayer* pPlayer = pEntity->As<CTFPlayer>();
		if (!pPlayer || !pPlayer->IsAlive() || pPlayer->m_iTeamNum() == pLocal->m_iTeamNum())
			continue;
		
		// Check distance
		Vector vEnemyPos = pPlayer->GetAbsOrigin();
		float flDistance = vPosition.DistTo(vEnemyPos);
		
		if (flDistance <= flMaxDistance)
			iEnemyCount++;
	}
	
	return (iEnemyCount >= iEnemyThreshold);
}

bool CAutoHeal::ShouldPopOnDangerProjectiles(CTFPlayer* pLocal, CWeaponMedigun* pWeapon)
{
	if (!Vars::Aimbot::Healing::PopOnDangerProjectiles.Value)
		return false;
	
	Vector vPosition = pLocal->GetAbsOrigin();
	auto pTarget = pWeapon->m_hHealingTarget().Get();
	if (pTarget && pTarget->IsPlayer())
	{
		CTFPlayer* pPlayerTarget = pTarget->As<CTFPlayer>();
		if (pPlayerTarget && pPlayerTarget->IsAlive())
		{
			vPosition = pPlayerTarget->GetAbsOrigin();
		}
	}
	
	// Loop through all entities to find dangerous projectiles
	for (int i = I::GlobalVars->maxClients + 1; i <= I::ClientEntityList->GetHighestEntityIndex(); i++)
	{
		auto pEntity = I::ClientEntityList->GetClientEntity(i);
		if (!pEntity)
			continue;
		
		const char* pszClassname = pEntity->GetClientClass()->m_pNetworkName;
		
		// Skip if not a projectile (using string comparison instead of FStrEq)
		if (strcmp(pszClassname, "CTFProjectile_Rocket") != 0 && 
		    strcmp(pszClassname, "CTFProjectile_SentryRocket") != 0 && 
		    strcmp(pszClassname, "CTFGrenadePipebombProjectile") != 0 && 
		    strcmp(pszClassname, "CTFProjectile_Flare") != 0 && 
		    strcmp(pszClassname, "CTFProjectile_Arrow") != 0 && 
		    strcmp(pszClassname, "CTFProjectile_Jar") != 0)
			continue;
		
		// Check if the projectile is from an enemy
		CTFBaseProjectile* pProjectile = pEntity->As<CTFBaseProjectile>();
		if (pProjectile && pProjectile->m_iTeamNum() == pLocal->m_iTeamNum())
			continue;
		
		// Check if it's close enough
		Vector vProjectilePos = pEntity->GetAbsOrigin();
		float flDistance = vPosition.DistTo(vProjectilePos);
		
		// Pop uber if dangerous projectile is close
		if (flDistance < 300.0f)
			return true;
	}
	
	return false;
}

bool CAutoHeal::ShouldPop(CTFPlayer* pLocal, CWeaponMedigun* pWeapon)
{
	// Don't pop if we're preserving uber and not in immediate danger
	if (Vars::Aimbot::Healing::PreserveUber.Value)
	{
		// Check if we're in a safe area with no enemies nearby
		bool bEnemiesNearby = ShouldPopOnEnemyCount(pLocal, pWeapon);
		
		// If there are no enemies nearby and our health is good, preserve uber
		if (!bEnemiesNearby && !ShouldPopOnHealth(pLocal, pWeapon))
			return false;
	}
	
	// Now check all the conditions for popping uber
	if (ShouldPopOnHealth(pLocal, pWeapon))
		return true;
	
	if (ShouldPopOnEnemyCount(pLocal, pWeapon))
		return true;
	
	if (ShouldPopOnDangerProjectiles(pLocal, pWeapon))
		return true;
	
	return false;
}

bool CAutoHeal::RunAutoUber(CTFPlayer* pLocal, CWeaponMedigun* pWeapon, CUserCmd* pCmd)
{
	if (!Vars::Aimbot::Healing::AutoUber.Value)
		return false;
	
	if (!pWeapon)
		return false;
	
	// Check if we have a heal target and it's valid with our settings
	auto pTarget = pWeapon->m_hHealingTarget().Get();
	if (!IsValidHealTarget(pLocal, pTarget))
		return false;
	
	float flChargeLevel = pWeapon->m_flChargeLevel();
	
	// Different thresholds for different mediguns
	float flMinLevel = 1.0f; // Default for stock, kritz, and quickfix
	int iItemDef = pWeapon->m_iItemDefinitionIndex();
	
	// Vaccinator needs 25% charge for a bubble
	if (iItemDef == 998) // Vaccinator
		flMinLevel = 0.25f;
	
	// Check if we have enough charge
	if (flChargeLevel < flMinLevel)
		return false;
	
	// If we're already ubering, don't need to do anything
	if (pWeapon->m_bChargeRelease())
		return true;
	
	// Check if we should pop uber
	if (ShouldPop(pLocal, pWeapon))
	{
		// Pop uber! Press M2
		pCmd->buttons |= IN_ATTACK2;
		return true;
	}
	
	return false;
}

void CAutoHeal::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!pLocal || !pWeapon || !pCmd)
		return;
	
	CWeaponMedigun* pMedigun = pWeapon->As<CWeaponMedigun>();
	if (!pMedigun)
		return;

	if (RunAutoUber(pLocal, pMedigun, pCmd))
		return;

	if (RunVaccinator(pLocal, pMedigun, pCmd))
		return;

	bool bActivated = ActivateOnVoice(pLocal, pMedigun, pCmd);
	m_mMedicCallers.clear();
	if (bActivated)
		return;
}