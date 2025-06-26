#include "AutoVote.h"

#include "../../Players/PlayerUtils.h"
#include "../../Misc/NamedPipe/NamedPipe.h"

void CAutoVote::UserMessage(bf_read& msgData)
{
	/*const int iTeam =*/ msgData.ReadByte();
	const int iVoteID = msgData.ReadLong();
	const int iCaller = msgData.ReadByte();
	char sReason[256]; msgData.ReadString(sReason, sizeof(sReason));
	char sTarget[256]; msgData.ReadString(sTarget, sizeof(sTarget));
	const int iTarget = msgData.ReadByte() >> 1;
	msgData.Seek(0);

	if (!Vars::Misc::Automation::AutoVote.Value)
		return;

	PlayerInfo_t pi{};
	if (I::EngineClient->GetPlayerInfo(iTarget, &pi))
	{
		if (F::NamedPipe::IsLocalBot(pi.friendsID))
		{
			I::ClientState->SendStringCmd(std::format("vote {} option2", iVoteID).c_str());
			return;
		}
	}

	PlayerInfo_t callerPi{};
	if (I::EngineClient->GetPlayerInfo(iCaller, &callerPi))
	{
		if (F::NamedPipe::IsLocalBot(callerPi.friendsID))
		{
			I::ClientState->SendStringCmd(std::format("vote {} option1", iVoteID).c_str());
			return;
		}
	}

	bool bIsKickVote = iTarget != 0;
	
	if (bIsKickVote)
	{
		// For kick votes, vote F2 (no) if target is ignored, friend, or party member
		if (F::PlayerUtils.IsIgnored(iTarget)
			|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Friends && H::Entities.IsFriend(iTarget)
			|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Party && H::Entities.InParty(iTarget))
		{
			I::ClientState->SendStringCmd(std::format("vote {} option2", iVoteID).c_str());
			return;
		}
	}
}