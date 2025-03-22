#include "../SDK/SDK.h"

#include "../Features/CritHack/CritHack.h"

// I didnt account for the randomizer to be called multiple times with the same seed which changes the outcoming results
MAKE_SIGNATURE(CUserCmd_ReadUserCmd, "server.dll", "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC ? 49 8B F0", 0x0);
static unsigned uLastCheckedCmdNum;
MAKE_HOOK(CUserCmd_ReadUserCmd, S::CUserCmd_ReadUserCmd(), void,
		  CUserCmd* rcx, bf_read* buf, CUserCmd* move, CUserCmd* from)
{
	CALL_ORIGINAL(rcx, buf, move, from);
	if (uLastCheckedCmdNum != F::CritHack.uLastCritCmdNum && move->buttons & IN_ATTACK)
	{
		uLastCheckedCmdNum = F::CritHack.uLastCritCmdNum;
		int iExpectedSeed = MD5_PseudoRandom(uLastCheckedCmdNum) & 0x7FFFFFFF;
		SDK::Output("CUserCmd_ReadUserCmd", std::format("command_number: {} random_seed:{} (expected:{}, G::RandomSeed():{})", move->command_number, move->random_seed, iExpectedSeed, *G::RandomSeed()).c_str());
		//CValve_Random* Random = new CValve_Random( );
		//Random->SetSeed( iExpectedSeed );
		SDK::RandomSeed(move->random_seed);
		if (SDK::RandomInt(-1200, 1200) != 0)
		{
			SDK::Output("CUserCmd_ReadUserCmd", std::format("random_seed - {} does not give no pipe rotation effect.", move->random_seed).c_str());
		}
	}
	return;
}