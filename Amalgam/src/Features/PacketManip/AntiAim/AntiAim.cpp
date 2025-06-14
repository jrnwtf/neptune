#include "AntiAim.h"

#include "../../Ticks/Ticks.h"
#include "../../Players/PlayerUtils.h"
#include "../../Misc/Misc.h"
#include "../../Aimbot/AutoRocketJump/AutoRocketJump.h"
#include "../../NavBot/NavEngine/NavEngine.h"

bool CAntiAim::AntiAimOn()
{
	return Vars::AntiAim::Enabled.Value
		&& (Vars::AntiAim::PitchReal.Value
		|| Vars::AntiAim::PitchFake.Value
		|| Vars::AntiAim::YawReal.Value
		|| Vars::AntiAim::YawFake.Value
		|| Vars::AntiAim::RealYawMode.Value
		|| Vars::AntiAim::FakeYawMode.Value
		|| Vars::AntiAim::RealYawOffset.Value
		|| Vars::AntiAim::FakeYawOffset.Value);
}

bool CAntiAim::YawOn()
{
	return Vars::AntiAim::Enabled.Value
		&& (Vars::AntiAim::YawReal.Value
		|| Vars::AntiAim::YawFake.Value
		|| Vars::AntiAim::RealYawMode.Value
		|| Vars::AntiAim::FakeYawMode.Value
		|| Vars::AntiAim::RealYawOffset.Value
		|| Vars::AntiAim::FakeYawOffset.Value);
}

bool CAntiAim::ShouldRun(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!pLocal || !pLocal->IsAlive() || pLocal->IsAGhost() || (pLocal->IsTaunting() && !Vars::AntiAim::TauntSpin.Value) || pLocal->m_MoveType() != MOVETYPE_WALK || pLocal->InCond(TF_COND_HALLOWEEN_KART)
		|| G::Attacking == 1 || F::AutoRocketJump.IsRunning() || F::Ticks.m_bDoubletap // this m_bDoubletap check can probably be removed if we fix tickbase correctly
		|| pWeapon && pWeapon->m_iItemDefinitionIndex() == Soldier_m_TheBeggarsBazooka && pCmd->buttons & IN_ATTACK && !(G::LastUserCmd->buttons & IN_ATTACK))
		return false;

	if (pLocal->InCond(TF_COND_SHIELD_CHARGE) || pCmd->buttons & IN_ATTACK2 && pLocal->m_bShieldEquipped() && pLocal->m_flChargeMeter() == 100.f)
		return false;

	return true;
}



void CAntiAim::FakeShotAngles(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::AntiAim::InvalidShootPitch.Value || G::Attacking != 1 || G::PrimaryWeaponType != EWeaponType::HITSCAN || !pLocal || pLocal->m_MoveType() != MOVETYPE_WALK)
		return;

	G::SilentAngles = true;
	pCmd->viewangles.x += 360 * (vFakeAngles.x < 0 ? -1 : 1);

	// messes with nospread accuracy
	//pCmd->viewangles.x = 180 - pCmd->viewangles.x;
	//pCmd->viewangles.y += 180;
}

float CAntiAim::EdgeDistance(CTFPlayer* pEntity, float flEdgeRayYaw, float flOffset)
{
	Vec3 vForward, vRight; Math::AngleVectors({ 0, flEdgeRayYaw, 0 }, &vForward, &vRight, nullptr);

	Vec3 vCenter = pEntity->GetCenter() + vRight * flOffset;
	Vec3 vEndPos = vCenter + vForward * 300.f;

	CGameTrace trace = {};
	CTraceFilterWorldAndPropsOnly filter = {};
	SDK::Trace(vCenter, vEndPos, MASK_SHOT | CONTENTS_GRATE, &filter, &trace);

	vEdgeTrace.emplace_back(vCenter, trace.endpos);

	return trace.fraction;
}

int CAntiAim::GetEdge(CTFPlayer* pEntity, const float flEdgeOrigYaw)
{
	float flSize = pEntity->m_vecMaxs().y - pEntity->m_vecMins().y;
	float flEdgeLeftDist = EdgeDistance(pEntity, flEdgeOrigYaw, -flSize);
	float flEdgeRightDist = EdgeDistance(pEntity, flEdgeOrigYaw, flSize);

	return flEdgeLeftDist > flEdgeRightDist ? -1 : 1;
}

void CAntiAim::RunOverlapping(CTFPlayer* pEntity, CUserCmd* pCmd, float& flRealYaw, bool bFake, float flEpsilon)
{
	if (!Vars::AntiAim::AntiOverlap.Value || bFake)
		return;

	float flFakeYaw = GetBaseYaw(pEntity, pCmd, true) + GetYawOffset(pEntity, true);
	const float flYawDiff = Math::NormalizeAngle(flRealYaw - flFakeYaw);
	if (fabsf(flYawDiff) < flEpsilon)
		flRealYaw += flYawDiff > 0 ? flEpsilon : -flEpsilon;
}

inline int GetJitter(uint32_t uHash)
{
	static std::unordered_map<uint32_t, bool> mJitter = {};

	if (!I::ClientState->chokedcommands)
		mJitter[uHash] = !mJitter[uHash];
	return mJitter[uHash] ? 1 : -1;
}

inline int GetDelayedJitter(uint32_t uHash, int iDelayTicks)
{
	static std::unordered_map<uint32_t, std::pair<int, bool>> mDelayedJitter = {};
	
	auto& [iTick, bState] = mDelayedJitter[uHash];
	
	if (!I::ClientState->chokedcommands)
	{
		if (++iTick >= iDelayTicks)
		{
			bState = !bState;
			iTick = 0;
		}
	}
	
	return bState ? 1 : -1;
}

inline float GetSineWave(float flSpeed, float flRange = 180.f)
{
	return sinf(I::GlobalVars->curtime * flSpeed) * flRange;
}

inline float GetRandomFloat(float flMin, float flMax)
{
	return flMin + (float(rand()) / RAND_MAX) * (flMax - flMin);
}

inline float GetMicroJitter(uint32_t uHash, float flRange)
{
	static std::unordered_map<uint32_t, float> mMicroJitter = {};
	
	if (!I::ClientState->chokedcommands)
		mMicroJitter[uHash] = GetRandomFloat(-flRange, flRange);
	
	return mMicroJitter[uHash];
}

float CAntiAim::GetYawOffset(CTFPlayer* pEntity, bool bFake)
{
	const int iMode = bFake ? Vars::AntiAim::YawFake.Value : Vars::AntiAim::YawReal.Value;
	int iJitter = GetJitter(FNV1A::Hash32Const("Yaw"));
	float flYawValue = bFake ? Vars::AntiAim::FakeYawValue.Value : Vars::AntiAim::RealYawValue.Value;

	switch (iMode)
	{
	case Vars::AntiAim::YawEnum::Forward: return 0.f;
	case Vars::AntiAim::YawEnum::Left: return 90.f;
	case Vars::AntiAim::YawEnum::Right: return -90.f;
	case Vars::AntiAim::YawEnum::Backwards: return 180.f;
	case Vars::AntiAim::YawEnum::Edge: return flYawValue * GetEdge(pEntity, I::EngineClient->GetViewAngles().y);
	case Vars::AntiAim::YawEnum::Jitter: return flYawValue * iJitter;
	case Vars::AntiAim::YawEnum::Spin: return fmod(I::GlobalVars->tickcount * Vars::AntiAim::SpinSpeed.Value + 180.f, 360.f) - 180.f;
	case Vars::AntiAim::YawEnum::SineWave: return GetSineWave(Vars::AntiAim::SineWaveSpeed.Value, flYawValue);
	case Vars::AntiAim::YawEnum::Random: 
	{
		static float flRandomYaw = 0.f;
		if (!I::ClientState->chokedcommands)
			flRandomYaw = (rand() % int(Vars::AntiAim::RandomRange.Value * 2)) - Vars::AntiAim::RandomRange.Value;
		return flRandomYaw;
	}
	case Vars::AntiAim::YawEnum::MicroJitter: return GetMicroJitter(FNV1A::Hash32Const("MicroYaw"), Vars::AntiAim::MicroJitterRange.Value);
	case Vars::AntiAim::YawEnum::DelayedJitter: return flYawValue * GetDelayedJitter(FNV1A::Hash32Const("DelayedYaw"), Vars::AntiAim::DelayedJitterTicks.Value);
	case Vars::AntiAim::YawEnum::LBYBreaker:
	{
		static int iAnimTick = 0;
		if (!I::ClientState->chokedcommands)
			iAnimTick++;
		
		bool bBreakAnim = (iAnimTick % 64) < 32;
		return bBreakAnim ? Vars::AntiAim::LBYBreakerDelta.Value : -Vars::AntiAim::LBYBreakerDelta.Value;
	}
	case Vars::AntiAim::YawEnum::Unhittable:
	{
		static int iUnhittableTick = 0;
		if (!I::ClientState->chokedcommands)
			iUnhittableTick++;
		
		float flSineComponent = GetSineWave(3.7f, 45.f); // Irregular sine wave
		float flMicroComponent = GetMicroJitter(FNV1A::Hash32Const("UnhittableMicro"), 8.f);
		float flDelayedComponent = flYawValue * GetDelayedJitter(FNV1A::Hash32Const("UnhittableDelayed"), 5 + (iUnhittableTick % 7));
		
		bool bLBYBreak = ((iUnhittableTick + 17) % 73) < 36;
		float flLBYComponent = bLBYBreak ? 135.f : -135.f;
		float flCombined = flSineComponent + flMicroComponent + (flDelayedComponent * 0.7f);
		if (iUnhittableTick % 13 < 7)
			flCombined += flLBYComponent * 0.6f;
		
		return Math::NormalizeAngle(flCombined);
	}
	}
	return 0.f;
}

float CAntiAim::GetBaseYaw(CTFPlayer* pLocal, CUserCmd* pCmd, bool bFake)
{
	const int iMode = bFake ? Vars::AntiAim::FakeYawMode.Value : Vars::AntiAim::RealYawMode.Value;
	const float flOffset = bFake ? Vars::AntiAim::FakeYawOffset.Value : Vars::AntiAim::RealYawOffset.Value;
	switch (iMode)
	{
	case Vars::AntiAim::YawModeEnum::View: return pCmd->viewangles.y + flOffset;
	case Vars::AntiAim::YawModeEnum::Target:
	{
		float flSmallestAngleTo = 0.f; float flSmallestFovTo = 360.f;
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
		{
			auto pPlayer = pEntity->As<CTFPlayer>();
			if (pPlayer->IsDormant() || !pPlayer->IsAlive() || pPlayer->IsAGhost() || F::PlayerUtils.IsIgnored(pPlayer->entindex()))
				continue;
			
			const Vec3 vAngleTo = Math::CalcAngle(pLocal->m_vecOrigin(), pPlayer->m_vecOrigin());
			const float flFOVTo = Math::CalcFov(I::EngineClient->GetViewAngles(), vAngleTo);

			if (flFOVTo < flSmallestFovTo)
			{
				flSmallestAngleTo = vAngleTo.y;
				flSmallestFovTo = flFOVTo;
			}
		}
		return (flSmallestFovTo == 360.f ? pCmd->viewangles.y + flOffset : flSmallestAngleTo + flOffset);
	}
	case Vars::AntiAim::YawModeEnum::Freestanding:
	{
		float flBestYaw = pCmd->viewangles.y;
		float flBestDistance = 0.f;
		
		for (int i = 0; i < 8; i++)
		{
			float flTestYaw = pCmd->viewangles.y + (i * 45.f);
			float flDistance = EdgeDistance(pLocal, flTestYaw, 0.f);
			
			if (flDistance > flBestDistance)
			{
				flBestDistance = flDistance;
				flBestYaw = flTestYaw;
			}
		}
		
		return flBestYaw + flOffset;
	}
	}
	return pCmd->viewangles.y;
}

float CAntiAim::GetYaw(CTFPlayer* pLocal, CUserCmd* pCmd, bool bFake)
{
	float flYaw = GetBaseYaw(pLocal, pCmd, bFake) + GetYawOffset(pLocal, bFake);
	RunOverlapping(pLocal, pCmd, flYaw, bFake);
	return flYaw;
}

float CAntiAim::GetPitch(float flCurPitch)
{
	float flRealPitch = 0.f, flFakePitch = 0.f;
	int iJitter = GetJitter(FNV1A::Hash32Const("Pitch"));

	switch (Vars::AntiAim::PitchReal.Value)
	{
	case Vars::AntiAim::PitchRealEnum::Up: flRealPitch = -89.f; break;
	case Vars::AntiAim::PitchRealEnum::Down: flRealPitch = 89.f; break;
	case Vars::AntiAim::PitchRealEnum::Zero: flRealPitch = 0.f; break;
	case Vars::AntiAim::PitchRealEnum::Jitter: flRealPitch = -89.f * iJitter; break;
	case Vars::AntiAim::PitchRealEnum::ReverseJitter: flRealPitch = 89.f * iJitter; break;
	case Vars::AntiAim::PitchRealEnum::Random:
	{
		static float flRandomPitch = 0.f;
		if (!I::ClientState->chokedcommands)
			flRandomPitch = GetRandomFloat(-89.f, 89.f);
		flRealPitch = flRandomPitch;
		break;
	}
	case Vars::AntiAim::PitchRealEnum::SineWave:
		flRealPitch = GetSineWave(Vars::AntiAim::SineWaveSpeed.Value, 89.f);
		break;
	case Vars::AntiAim::PitchRealEnum::Micro:
		flRealPitch = GetMicroJitter(FNV1A::Hash32Const("MicroPitch"), 15.f);
		break;
	case Vars::AntiAim::PitchRealEnum::AntiResolver:
	{
		static int iResolverTick = 0;
		if (!I::ClientState->chokedcommands)
			iResolverTick++;
		
		float flBasePitch = (iResolverTick % 3 == 0) ? -89.f : 89.f;
		float flMicroAdjust = GetMicroJitter(FNV1A::Hash32Const("ResolverPitchMicro"), 2.f);
		
		if ((iResolverTick + 7) % 11 < 5)
			flBasePitch *= 0.98f; // avoid exact -89/89
		
		flRealPitch = flBasePitch + flMicroAdjust;
		break;
	}
	}

	switch (Vars::AntiAim::PitchFake.Value)
	{
	case Vars::AntiAim::PitchFakeEnum::Up: flFakePitch = -89.f; break;
	case Vars::AntiAim::PitchFakeEnum::Down: flFakePitch = 89.f; break;
	case Vars::AntiAim::PitchFakeEnum::Jitter: flFakePitch = -89.f * iJitter; break;
	case Vars::AntiAim::PitchFakeEnum::ReverseJitter: flFakePitch = 89.f * iJitter; break;
	case Vars::AntiAim::PitchFakeEnum::Random:
	{
		static float flRandomPitch = 0.f;
		if (!I::ClientState->chokedcommands)
			flRandomPitch = GetRandomFloat(-89.f, 89.f);
		flFakePitch = flRandomPitch;
		break;
	}
	case Vars::AntiAim::PitchFakeEnum::SineWave:
		flFakePitch = GetSineWave(Vars::AntiAim::SineWaveSpeed.Value, 89.f);
		break;
	case Vars::AntiAim::PitchFakeEnum::Micro:
		flFakePitch = GetMicroJitter(FNV1A::Hash32Const("MicroFakePitch"), 15.f);
		break;
	}

	if (Vars::AntiAim::PitchReal.Value && Vars::AntiAim::PitchFake.Value)
		return flRealPitch + (flFakePitch > 0.f ? 360 : -360);
	else if (Vars::AntiAim::PitchReal.Value)
		return flRealPitch;
	else if (Vars::AntiAim::PitchFake.Value)
		return flFakePitch;
	else
		return flCurPitch;
}

void CAntiAim::MinWalk(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::AntiAim::MinWalk.Value || !F::AntiAim.YawOn() || !pLocal->m_hGroundEntity() || pLocal->InCond(TF_COND_HALLOWEEN_KART))
		return;

	if (!pCmd->forwardmove && !pCmd->sidemove && pLocal->m_vecVelocity().Length2D() < 2.f)
	{
		static bool bVar = true;
		float flMove = (pLocal->IsDucking() ? 3 : 1) * ((bVar = !bVar) ? 1 : -1);
		Vec3 vDir = { flMove, flMove, 0 };

		Vec3 vMove = Math::RotatePoint(vDir, {}, { 0, -pCmd->viewangles.y, 0 });
		pCmd->forwardmove = vMove.x * (fmodf(fabsf(pCmd->viewangles.x), 180.f) > 90.f ? -1 : 1);
		pCmd->sidemove = -vMove.y;

		pLocal->m_vecVelocity() = { 1, 1 }; // a bit stupid but it's probably fine
	}
}

void CAntiAim::MaximizeDesync(CTFPlayer* pLocal, CUserCmd* pCmd, bool bSendPacket)
{
	if (!Vars::AntiAim::MaxDesync.Value)
		return;
	
	if (!bSendPacket)
	{
		float flAngleDiff = Math::NormalizeAngle(vRealAngles.y - vFakeAngles.y);
		
		if (fabsf(flAngleDiff) < 90.f)
		{
			float flPushDirection = flAngleDiff > 0 ? 1.f : -1.f;
			vRealAngles.y = Math::NormalizeAngle(vFakeAngles.y + (120.f * flPushDirection));
		}
		
		static int iVariationTick = 0;
		if (++iVariationTick % 3 == 0)
		{
			vRealAngles.y += GetMicroJitter(FNV1A::Hash32Const("FakeAngleMicro"), 2.f);
		}
	}
}

bool CAntiAim::IsBreakingLBY(CTFPlayer* pLocal)
{
	static int iAnimBreakTick = 0;
	iAnimBreakTick++;
	
	return (iAnimBreakTick % 64) > 50 && (iAnimBreakTick % 64) < 64;
}

void CAntiAim::ApplyAntiResolverTechniques(CTFPlayer* pLocal, CUserCmd* pCmd, bool bSendPacket)
{
	if (!Vars::AntiAim::AntiResolverMode.Value)
		return;
	
	static int iAntiResolverTick = 0;
	iAntiResolverTick++;
	
	if (!bSendPacket) 
	{
		static float flLastRealYaw = 0.f;
		float flYawChange = Math::NormalizeAngle(vRealAngles.y - flLastRealYaw);
		
		if (fabsf(flYawChange) < 10.f && iAntiResolverTick % 7 == 0)
		{
			vRealAngles.y += GetRandomFloat(-30.f, 30.f);
		}
		flLastRealYaw = vRealAngles.y;
		
		if (iAntiResolverTick % 11 == 0)
		{
			vRealAngles.y = Math::NormalizeAngle(vRealAngles.y + 180.f);
		}
		
		if (IsBreakingLBY(pLocal) && iAntiResolverTick % 13 < 7)
		{
			vRealAngles.y += GetMicroJitter(FNV1A::Hash32Const("AnimAntiResolver"), 15.f);
		}
		
		if (iAntiResolverTick % 4 == 0)
		{
			vRealAngles.x += GetMicroJitter(FNV1A::Hash32Const("PitchNoise"), 1.f);
			vRealAngles.y += GetMicroJitter(FNV1A::Hash32Const("YawNoise"), 2.f);
		}
	}
	else // Fake angles (sent packets)
	{
		if (iAntiResolverTick % 6 == 0)
		{
			vFakeAngles.y += GetMicroJitter(FNV1A::Hash32Const("FakeNoise"), 3.f);
		}
		
		if (iAntiResolverTick % 17 < 8)
		{
			if (fabsf(vFakeAngles.x) > 45.f)
				vFakeAngles.x *= 0.7f;
		}
	}
}



void CAntiAim::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, bool bSendPacket)
{
	G::AntiAim = AntiAimOn() && ShouldRun(pLocal, pWeapon, pCmd);

	int iAntiBackstab = F::Misc.AntiBackstab(pLocal, pCmd, bSendPacket);
	if (!iAntiBackstab)
		FakeShotAngles(pLocal, pCmd);

	if (!G::AntiAim)
	{
		vRealAngles = { pCmd->viewangles.x, pCmd->viewangles.y };
		vFakeAngles = { pCmd->viewangles.x, pCmd->viewangles.y };
		return;
	}

	vEdgeTrace.clear();

	Vec2& vAngles = bSendPacket ? vFakeAngles : vRealAngles;
	vAngles.x = iAntiBackstab != 2 ? GetPitch(pCmd->viewangles.x) : pCmd->viewangles.x;
	vAngles.y = !iAntiBackstab ? GetYaw(pLocal, pCmd, bSendPacket) : pCmd->viewangles.y;

	MaximizeDesync(pLocal, pCmd, bSendPacket);
	ApplyAntiResolverTechniques(pLocal, pCmd, bSendPacket);

	if (Vars::Misc::Game::AntiCheatCompatibility.Value)
		Math::ClampAngles(vAngles);

	SDK::FixMovement(pCmd, vAngles);
	pCmd->viewangles.x = vAngles.x;
	pCmd->viewangles.y = vAngles.y;

	MinWalk(pLocal, pCmd);
}