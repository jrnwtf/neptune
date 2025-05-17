#pragma once
#include "../../SDK/SDK.h"

class CMisc
{
	
	void AutoJump(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AutoJumpbug(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AutoStrafe(CTFPlayer* pLocal, CUserCmd* pCmd);
	void MovementLock(CTFPlayer* pLocal, CUserCmd* pCmd);
	void BreakJump(CTFPlayer* pLocal, CUserCmd* pCmd);
	void BreakShootSound(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AntiAFK(CTFPlayer* pLocal, CUserCmd* pCmd);
	void InstantRespawnMVM(CTFPlayer* pLocal);
	void NoiseSpam(CTFPlayer* pLocal);
	void VoiceCommandSpam(CTFPlayer* pLocal);
	void RandomVotekick(CTFPlayer* pLocal);
	void ChatSpam(CTFPlayer* pLocal);
	void KillSay(int victim);
	void AutoReply(int speaker, const char* text);
	void VotekickResponse(int target);
	std::string ProcessTextReplacements(std::string text);

	void CheatsBypass();
	void WeaponSway();
	void AutoReport();

	void TauntKartControl(CTFPlayer* pLocal, CUserCmd* pCmd);
	void FastMovement(CTFPlayer* pLocal, CUserCmd* pCmd);

	void AutoPeek(CTFPlayer* pLocal, CUserCmd* pCmd, bool bPost = false);
	void EdgeJump(CTFPlayer* pLocal, CUserCmd* pCmd, bool bPost = false);

	bool m_bPeekPlaced = false;
	Vec3 m_vPeekReturnPos = {};
	//bool bSteamCleared = false;

	std::vector<std::string> m_vChatSpamLines;
	std::vector<std::string> m_vKillSayLines;
	std::vector<std::string> m_vAutoReplyLines;
	Timer m_tChatSpamTimer;
	int m_iCurrentChatSpamIndex = 0;
	int m_iLastKilledPlayer = 0;
	std::string m_sLastKilledPlayerName;

public:
	void RunPre(CTFPlayer* pLocal, CUserCmd* pCmd);
	void RunPost(CTFPlayer* pLocal, CUserCmd* pCmd, bool pSendPacket);

	void Event(IGameEvent* pEvent, uint32_t uNameHash);
	int AntiBackstab(CTFPlayer* pLocal, CUserCmd* pCmd, bool bSendPacket);

	void PingReducer();
	void UnlockAchievements();
	void LockAchievements();
	bool SteamRPC();

	int m_iWishCmdrate = -1;
	//int m_iWishUpdaterate = -1;
	bool m_bAntiAFK = false;
	Timer m_tRandomVotekickTimer;
};

ADD_FEATURE(CMisc, Misc)
