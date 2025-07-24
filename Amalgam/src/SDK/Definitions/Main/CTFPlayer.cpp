#include "CTFPlayer.h"

#include "../../SDK.h"

Vec3 CTFPlayer::GetEyeAngles()
{
	return { m_angEyeAnglesX(), m_angEyeAnglesY(), 0.f };
}

Vec3 CTFPlayer::GetEyePosition()
{
	return m_vecOrigin() + m_vecViewOffset();
}

Vec3 CTFPlayer::GetViewOffset()
{
	if (!IsPlayer())
		return GetOffset() / 2;

	auto getMainOffset = [this]() -> Vec3
		{
			if (IsDucking())
				return { 0.f, 0.f, 45.f };

			switch (m_iClass())
			{
			case TF_CLASS_SCOUT: return { 0.f, 0.f, 65.f };
			case TF_CLASS_SOLDIER: return { 0.f, 0.f, 68.f };
			case TF_CLASS_PYRO: return { 0.f, 0.f, 68.f };
			case TF_CLASS_DEMOMAN: return { 0.f, 0.f, 68.f };
			case TF_CLASS_HEAVY: return { 0.f, 0.f, 75.f };
			case TF_CLASS_ENGINEER: return { 0.f, 0.f, 68.f };
			case TF_CLASS_MEDIC: return { 0.f, 0.f, 75.f };
			case TF_CLASS_SNIPER: return { 0.f, 0.f, 75.f };
			case TF_CLASS_SPY: return { 0.f, 0.f, 75.f };
			}

			const Vec3 vOffset = m_vecViewOffset();
			if (vOffset.z)
				return vOffset;

			return { 0.f, 0.f, 68.f };
		};

	return getMainOffset() * m_flModelScale();
}

bool CTFPlayer::InCond(const ETFCond cond)
{
	const int iCond = static_cast<int>(cond);
	switch (iCond / 32)
	{
	case 0:
	{
		const int bit = (1 << iCond);
		if ((m_nPlayerCond() & bit) == bit || (_condition_bits() & bit) == bit)
			return true;
		break;
	}
	case 1:
	{
		const int bit = 1 << (iCond - 32);
		if ((m_nPlayerCondEx() & bit) == bit)
			return true;
		break;
	}
	case 2:
	{
		const int bit = 1 << (iCond - 64);
		if ((m_nPlayerCondEx2() & bit) == bit)
			return true;
		break;
	}
	case 3:
	{
		const int bit = 1 << (iCond - 96);
		if ((m_nPlayerCondEx3() & bit) == bit)
			return true;
		break;
	}
	case 4:
	{
		const int bit = 1 << (iCond - 128);
		if ((m_nPlayerCondEx4() & bit) == bit)
			return true;
		break;
	}
	}

	return false;
}

bool CTFPlayer::IsZoomed()
{
	return InCond(TF_COND_ZOOMED);
}

bool CTFPlayer::IsStealthed()
{
	return InCond(TF_COND_STEALTHED)
		|| InCond(TF_COND_STEALTHED_USER_BUFF)
		|| InCond(TF_COND_STEALTHED_USER_BUFF_FADING);
}

bool CTFPlayer::IsAGhost()
{
	return InCond(TF_COND_HALLOWEEN_GHOST_MODE);
};
bool CTFPlayer::IsTaunting()
{
	return InCond(TF_COND_TAUNTING);
};

bool CTFPlayer::IsInvisible()
{
	if (InCond(TF_COND_BURNING)
		|| InCond(TF_COND_BURNING_PYRO)
		|| InCond(TF_COND_MAD_MILK)
		|| InCond(TF_COND_URINE))
		return false;

	return m_flInvisibility() >= 1.f;
}

float CTFPlayer::GetInvisiblePercentage()
{
	static auto tf_spy_invis_time = I::CVar->FindVar("tf_spy_invis_time");
	const float flInvisTime = tf_spy_invis_time ? tf_spy_invis_time->GetFloat() : 1.f;
	const float GetInvisPercent = Math::RemapVal(m_flInvisChangeCompleteTime() - I::GlobalVars->curtime, flInvisTime, 0.f, 0.f, 100.f);

	return GetInvisPercent;
}

bool CTFPlayer::IsInvulnerable()
{
	return InCond(TF_COND_INVULNERABLE)
		|| InCond(TF_COND_INVULNERABLE_CARD_EFFECT)
		|| InCond(TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED)
		|| InCond(TF_COND_INVULNERABLE_USER_BUFF)
		|| InCond(TF_COND_PHASE);
}

bool CTFPlayer::IsUbered()
{
	return InCond(TF_COND_INVULNERABLE)
		|| InCond(TF_COND_INVULNERABLE_CARD_EFFECT)
		|| InCond(TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED)
		|| InCond(TF_COND_INVULNERABLE_USER_BUFF);
}

bool CTFPlayer::IsCritBoosted()
{
	return InCond(TF_COND_CRITBOOSTED)
		|| InCond(TF_COND_CRITBOOSTED_BONUS_TIME)
		|| InCond(TF_COND_CRITBOOSTED_CARD_EFFECT)
		|| InCond(TF_COND_CRITBOOSTED_CTF_CAPTURE)
		|| InCond(TF_COND_CRITBOOSTED_FIRST_BLOOD)
		|| InCond(TF_COND_CRITBOOSTED_ON_KILL)
		|| InCond(TF_COND_CRITBOOSTED_PUMPKIN)
		|| InCond(TF_COND_CRITBOOSTED_RAGE_BUFF)
		|| InCond(TF_COND_CRITBOOSTED_RUNE_TEMP)
		|| InCond(TF_COND_CRITBOOSTED_USER_BUFF);
}

bool CTFPlayer::IsMiniCritBoosted()
{
	return InCond(TF_COND_OFFENSEBUFF)
		|| InCond(TF_COND_ENERGY_BUFF)
		|| InCond(TF_COND_MINICRITBOOSTED_ON_KILL)
		|| InCond(TF_COND_NOHEALINGDAMAGEBUFF)
		/*|| InCond(TF_COND_CRITBOOSTED_DEMO_CHARGE)*/;
}

bool CTFPlayer::IsMarked()
{
	return InCond(TF_COND_URINE)
		|| InCond(TF_COND_MARKEDFORDEATH)
		|| InCond(TF_COND_MARKEDFORDEATH_SILENT)
		|| InCond(TF_COND_PASSTIME_PENALTY_DEBUFF);
}

bool CTFPlayer::CanAttack(bool bCloak, bool bLocal)
{
	if (!IsAlive() || IsAGhost() || IsTaunting() || m_bViewingCYOAPDA()
		|| InCond(TF_COND_PHASE) || InCond(TF_COND_HALLOWEEN_KART) || InCond(TF_COND_STUNNED) && m_iStunFlags() & (TF_STUN_CONTROLS | TF_STUN_LOSER_STATE))
		return false;

	if (bCloak)
	{
		if (bLocal
			? (m_flStealthNoAttackExpire() > TICKS_TO_TIME(m_nTickBase()) && !InCond(TF_COND_STEALTHED_USER_BUFF)) || InCond(TF_COND_STEALTHED)
			: m_flInvisibility() && (InCond(TF_COND_STEALTHED) || !InCond(TF_COND_STEALTHED_USER_BUFF) && !InCond(TF_COND_STEALTHED_USER_BUFF_FADING)))
		{
			auto pWeapon = m_hActiveWeapon()->As<CTFWeaponBase>();
			if (!pWeapon || pWeapon->GetWeaponID() != TF_WEAPON_GRAPPLINGHOOK)
				return false;
		}

		if (m_bFeignDeathReady())
			return false;
	}

	auto pGameRules = I::TFGameRules();
	if (pGameRules)
	{
		switch (pGameRules->m_iRoundState())
		{
		case GR_STATE_TEAM_WIN:
			if (m_iTeamNum() != pGameRules->m_iWinningTeam())
				return false;
			break;
		case GR_STATE_BETWEEN_RNDS:
			if (m_fFlags() & FL_FROZEN)
				return false;
			break;
		case GR_STATE_GAME_OVER:
			if (m_fFlags() & FL_FROZEN || m_iTeamNum() != pGameRules->m_iWinningTeam())
				return false;
			break;
		}
	}

	if (SDK::AttribHookValue(0, "no_attack", this))
		return false;

	return true;
}

float CTFPlayer::GetCritMult()
{
	return Math::RemapVal(m_iCritMult(), 0.f, 255.f, 1.f, 4.f);
}

Vec3 CTFPlayer::GetBoneOrigin(int bone)
{
	matrix3x4 boneMatrix[128];

	try
	{
		if (SetupBones(boneMatrix, 128, BONE_USED_BY_HITBOX, I::GlobalVars->curtime))
			return Vec3(boneMatrix[bone][0][3], boneMatrix[bone][1][3], boneMatrix[bone][2][3]);
	}
	catch (...)
	{
		//SDK::Output("GetBoneOrigin->Unknown error", "\n", { 133, 0, 255, 255 });
		return Vec3{};
	}

	return Vec3{};
}

Vec3 CTFPlayer::GetHitboxOrigin(int hitbox)
{
	matrix3x4 boneMatrix[128];

	const model_t* model;
	studiohdr_t* hdr;

	mstudiohitboxset_t* set;
	mstudiobbox_t* hitboxMat;

	Vec3 min, max;

	try
	{
		if (!SetupBones(boneMatrix, 128, BONE_USED_BY_HITBOX, I::GlobalVars->curtime))
		{
			//SDK::Output("GetHitboxOrigin->SetupBones invalid", "\n", { 133, 0, 255, 255 });
			return Vec3{};
		}

		if ((model = GetModel()) == nullptr)
		{
			//SDK::Output("GetHitboxOrigin->GetModel invalid", "\n", { 133, 0, 255, 255 });
			return Vec3{};
		}

		if ((hdr = I::ModelInfoClient->GetStudiomodel(model)) == nullptr)
		{
			//SDK::Output("GetHitboxOrigin->GetStudioModel invalid", "\n", { 133, 0, 255, 255 });
			return Vec3{};
		}

		if ((set = hdr->pHitboxSet(0)) == nullptr)
		{
			//SDK::Output("GetHitboxOrigin->GetHitboxSet invalid", "\n", { 133, 0, 255, 255 });
			return Vec3{};
		}

		if ((hitboxMat = set->pHitbox(hitbox)) == nullptr)
		{
			//SDK::Output("GetHitboxOrigin->GetHitbox invalid", "\n", { 133, 0, 255, 255 });
			return Vec3{};
		}
	}
	catch (...)
	{
		//SDK::Output("GetHitboxOrigin->Unknown error", "\n", { 133, 0, 255, 255 });
		return Vec3{};
	}

	//M::VectorTransform(hitboxMat->bbmin, boneMatrix[hitboxMat->bone], min);
	//M::VectorTransform(hitboxMat->bbmax, boneMatrix[hitboxMat->bone], max);

	return ((min + max) * 0.5f);
}

bool CTFPlayer::ConstructBones(CTFPlayer* pEntity, matrix3x4* pBones, int mask)
{
	if (!pEntity)
		return false;

	Vec3 m_vecUninterpolatedOrigin = pEntity->m_vecOrigin();
	Vec3 m_vecInterpolatedOrigin = pEntity->GetAbsOrigin();
	Vec3 m_vecAbsAngles = pEntity->GetAbsAngles();
	Vec3 m_vecNetworkedAngles = pEntity->GetEyeAngles();

	static ConVar* anim_showstate = I::CVar->FindVar("anim_showstate");
	anim_showstate->SetValue(pEntity->entindex());

	//G::ConstructingBones = true;

	pEntity->SetAbsAngles(m_vecNetworkedAngles);
	pEntity->SetAbsOrigin(m_vecUninterpolatedOrigin);
	//pEntity->m_fFlags() = (EFL_DIRTY_ABSTRANSFORM, EFL_DIRTY_ABSVELOCITY);
	//pEntity->CalcAbsolutePosition();

	if (!pEntity->SetupBones(pBones, 128, mask, I::GlobalVars->curtime))
		return false;

	pEntity->SetAbsOrigin(m_vecInterpolatedOrigin);
	pEntity->SetAbsAngles(m_vecAbsAngles);

	//G::ConstructingBones = false;

	return true;
}

void CTFPlayer::UpdateButtonState(const int nUserCmdButtonMask)
{
	m_afButtonLast() = m_nButtons();
	m_nButtons() = nUserCmdButtonMask;

	const int buttonsChanged = m_afButtonLast() ^ m_nButtons();

	m_afButtonPressed() = buttonsChanged & m_nButtons();
	m_afButtonReleased() = buttonsChanged & (~m_nButtons());
}

std::pair<Vec3, Vec3> CTFPlayer::GetBoundingBox()
{
	static int minOffset = U::NetVars.GetNetVar("CBasePlayer", "m_vecMins");
	static int maxOffset = U::NetVars.GetNetVar("CBasePlayer", "m_vecMaxs");

	Vec3 origin = GetAbsOrigin();
	return std::make_pair(origin + minOffset, origin + maxOffset);
}

CTFWeaponBase* CTFPlayer::GetWeaponFromSlot(int nSlot)
{
	static int nOffset = U::NetVars.GetNetVar("CBaseCombatCharacter", "m_hMyWeapons");
	int hWeapon = *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(this) + nOffset + nSlot * 4);
	return I::ClientEntityList->GetClientEntityFromHandle(hWeapon)->As<CTFWeaponBase>();
}

std::string CTFPlayer::GetName()
{
	PlayerInfo_t info{};

	if (I::EngineClient->GetPlayerInfo(entindex(), &info))
		return "";

	return info.name;
}

float CTFPlayer::HealthFraction()
{
	if (GetMaxHealth() == 0)
		return 1.0f;

	return std::clamp(static_cast<float>(m_iHealth()) / static_cast<float>(GetMaxHealth()), 0.0f, 1.0f);
}