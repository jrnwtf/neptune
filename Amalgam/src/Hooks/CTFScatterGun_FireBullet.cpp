#include "../SDK/SDK.h"

MAKE_SIGNATURE(CTFScatterGun_FireBullet, "client.dll", "E9 ? ? ? ? CC CC CC CC CC CC CC CC CC CC CC 48 89 74 24 ? 57 48 83 EC ? 48 8B F9 E8", 0x0);
MAKE_SIGNATURE(CTFGameMovement_SetGroundEntity, "client.dll", "48 89 5C 24 ? 57 48 83 EC ? 48 8B D9 48 8B FA 48 8B 89 ? ? ? ? BA", 0x0);
MAKE_SIGNATURE(CTFPlayer_ApplyAbsVelocityImpulse, "client.dll", "48 89 5C 24 ? 57 48 83 EC ? F3 0F 10 05 ? ? ? ? 48 8B FA", 0x0);
MAKE_SIGNATURE(CTFPlayerShared_AddCond, "client.dll", "48 89 5C 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 48 8B F9 0F 29 74 24", 0x0);

static bool bScattergunJump = false;

static inline bool HasKnockback(CTFWeaponBase* pWeapon)
{
	return SDK::AttribHookValue(0, "set_scattergun_has_knockback", pWeapon) == 1;
}

static inline void StunPlayer(CTFPlayer* pPlayer, float flTime, float flReductionAmount, int iStunFlags, CTFPlayer* pAttacker = nullptr)
{
	if (pPlayer->InCond(TF_COND_PHASE) || pPlayer->InCond(TF_COND_PASSTIME_INTERCEPTION) || pPlayer->InCond(TF_COND_MEGAHEAL)
		|| pPlayer->InCond(TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED) && !pPlayer->InCond(TF_COND_MVM_BOT_STUN_RADIOWAVE))
		return;

	S::CTFPlayerShared_AddCond.Call<void>(pPlayer->m_Shared(), TF_COND_STUNNED, -1.f, pAttacker);
}

inline float AirBurstDamageForce(const Vector& size, float damage, float scale)
{
	float force = damage * ((48 * 48 * 82.0) / (size.x * size.y * size.z)) * scale;

	if (force > 1000.0)
		force = 1000.0;

	return force;
}

inline void ApplyAbsVelocityImpulse(const Vector& vecImpulse)
{
	Vector vecForce = vecImpulse;
	float flImpulseScale = 1.0f;

	if (this->IsInCondition(TF_COND_HALLOWEEN_TINY) && !this->IsInCondition(TF_COND_HALLOWEEN_KART))
	{
		flImpulseScale *= 2.f;
	}

	reinterpret_cast<void(__fastcall*)(void*, const Vector&)>(S::CTFPlayer_ApplyAbsVelocityImpulse)
		(this, vecForce * flImpulseScale);
}

MAKE_HOOK(CTFScatterGun_FireBullet, S::CTFScatterGun_FireBullet(), void,
	void* rcx, CTFPlayer* pPlayer)
{
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::CTFScatterGun_FireBullet[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, pPlayer);
#endif

	auto pWeapon = reinterpret_cast<CTFWeaponBase*>(rcx);
	if (HasKnockback(pWeapon))
	{
		auto pGameRules = I::TFGameRules();
		if (pGameRules && pGameRules->m_iRoundState() == GR_STATE_PREROUND)
			return;

		if (!(pPlayer->m_fFlags() & FL_ONGROUND) && !pPlayer->m_flPhysics())
		{
			pPlayer->m_flPhysics() = true;

			auto NetChannel = gClientState->NetChannel;//
			auto Latency = NetChannel->GetLatency(FLOW_OUTGOING) + NetChannel->GetLatency(FLOW_INCOMING);//
			auto Difference = gGlobals->curtime - Globals->LastFANAngleTime;//

			if (Difference > Latency)//
			{
				pPlayer->m_flPhysics() = LocalPlayer->GetAbsAngles();//
				Globals->LastFANAngleTime = gGlobals->curtime;//
			}//

			StunPlayer(pPlayer, 0.3f, 1.f, TF_STUN_MOVEMENT | TF_STUN_MOVEMENT_FORWARD_ONLY);

			Vector Size = LocalPlayer->Maxs() - LocalPlayer->Mins();//
			float flForce = AirBurstDamageForce(Size, 60, 6.f);//

			Vector vecForward;//
			gMath->AngleVectors(Globals->FANJumpAngles, &vecForward);//
			Vector vecForce = vecForward * -flForce;//

			VMatrix mtxPlayer;
			mtxPlayer.SetupMatrixOrgAngles(pPlayer->m_vecOrigin(), pPlayer->GetEyeAngles());
			Vector vecAbsVelocity = pPlayer->GetAbsVelocity();
			Vector vecAbsVelocityAsPoint = vecAbsVelocity + pPlayer->m_vecOrigin();
			Vector vecLocalVelocity = mtxPlayer.WorldToLocal(vecAbsVelocityAsPoint);

			vecLocalVelocity.x = -300;

			vecAbsVelocityAsPoint = mtxPlayer.LocalToWorld(vecLocalVelocity);
			vecAbsVelocity = vecAbsVelocityAsPoint - pPlayer->m_vecOrigin();
			pPlayer->SetAbsVelocity(vecAbsVelocity);
			
			S::CTFPlayer_ApplyAbsVelocityImpulse.Call<void>(pPlayer, Vec3(0, 0, 50));
			//ApplyAbsVelocityImpulse(Vector(0, 0, 50.f));

			pPlayer->m_fFlags() &= ~FL_ONGROUND;
		}
	}

	CALL_ORIGINAL(rcx, pPlayer);
}

MAKE_HOOK(CTFGameMovement_SetGroundEntity, S::CTFGameMovement_SetGroundEntity(), void,
	void* rcx, trace_t* pm)
{
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::CTFScatterGun_FireBullet[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, pm);
#endif

	CALL_ORIGINAL(rcx, pm);

	if (pm && pm->m_pEnt)
	{
		auto pPlayer = *reinterpret_cast<CTFPlayer**>(uintptr_t(rcx) + 5832);
		pPlayer->m_flPhysics() = false;
	}
}