#include "MPController.h"

std::optional<Vector> CMPController::GetPowerupPosition(PowerupBottleType_t type)
{
	for (auto& powerup : m_vPowerups)
	{
		if (powerup.m_Type == type)
			return powerup.m_vPosition;
	}
	return std::nullopt;
}

std::optional<PowerupInfo> CMPController::GetClosestPowerup(const Vector& vPosition)
{
	float flClosestDistance = FLT_MAX;
	PowerupInfo* pClosestPowerup = nullptr;

	for (auto& powerup : m_vPowerups)
	{
		float flDist = (powerup.m_vPosition - vPosition).Length();
		if (flDist < flClosestDistance)
		{
			flClosestDistance = flDist;
			pClosestPowerup = &powerup;
		}
	}

	if (pClosestPowerup)
		return *pClosestPowerup;

	return std::nullopt;
}

FlagInfo CMPController::GetFlag(int team)
{
	return F::FlagController.GetFlag(team);
}

Vector CMPController::GetPosition(CCaptureFlag* pFlag)
{
	return F::FlagController.GetPosition(pFlag);
}

std::optional<Vector> CMPController::GetPosition(int iTeam)
{
	return F::FlagController.GetPosition(iTeam);
}

std::optional<Vector> CMPController::GetSpawnPosition(int iTeam)
{
	return F::FlagController.GetSpawnPosition(iTeam);
}

int CMPController::GetCarrier(CCaptureFlag* pFlag)
{
	return F::FlagController.GetCarrier(pFlag);
}

int CMPController::GetCarrier(int iTeam)
{
	return F::FlagController.GetCarrier(iTeam);
}

int CMPController::GetStatus(CCaptureFlag* pFlag)
{
	return F::FlagController.GetStatus(pFlag);
}

int CMPController::GetStatus(int iTeam)
{
	return F::FlagController.GetStatus(iTeam);
}

void CMPController::Init()
{
	m_vPowerups.clear();
}

void CMPController::Update()
{
	F::FlagController.Update();

	m_vPowerups.clear();

	for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_OBJECTIVE))
	{
		// Skip invalid entities
		if (!pEntity || pEntity->IsDormant())
			continue;

		// Check for powerup entities
		if (pEntity->GetClassID() == ETFClassID::CTFPowerupBottle)
		{
			auto pPowerup = pEntity->As<CBaseEntity>();
			PowerupBottleType_t powerupType = POWERUP_BOTTLE_NONE;

			auto pEconEntity = pPowerup->As<CEconEntity>();
			if (pEconEntity)
			{
				int itemDefIndex = pEconEntity->m_iItemDefinitionIndex();
				
				// Item def mapping based on standard TF2 powerup IDs
				switch (itemDefIndex)
				{
				case 1086: powerupType = POWERUP_BOTTLE_CRITBOOST; break;
				case 1087: powerupType = POWERUP_BOTTLE_UBERCHARGE; break;
				case 1088: powerupType = POWERUP_BOTTLE_RECALL; break;
				case 1089: powerupType = POWERUP_BOTTLE_REFILL_AMMO; break;
				case 1090: powerupType = POWERUP_BOTTLE_BUILDINGS_INSTANT_UPGRADE; break;
				case 1091: powerupType = POWERUP_BOTTLE_RADIUS_STEALTH; break;
				default:
					powerupType = (PowerupBottleType_t)pPowerup->m_nModelIndex();
					break;
				}
			}
			else
			{
				powerupType = (PowerupBottleType_t)pPowerup->m_nModelIndex();
			}
            
			if (powerupType > POWERUP_BOTTLE_NONE && powerupType < POWERUP_BOTTLE_TOTAL)
			{
				PowerupInfo info(pPowerup, powerupType, pPowerup->GetAbsOrigin());
				m_vPowerups.push_back(info);
			}
		}
	}
}
