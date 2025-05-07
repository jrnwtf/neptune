#include "Interfaces.h"

#include "../../Core/Core.h"
#include "../../Utils/Assert/Assert.h"

#define Validate(x) if (!x) { U::Core.AppendFailText("CNullInterfaces::Initialize() failed to initialize "#x); m_bFailed = true; }
#define ValidateNonLethal(x) if (!x) { const char* sMessage = "CNullInterfaces::Initialize() failed to initialize "#x; MessageBox(nullptr, sMessage, "Warning", MB_OK | MB_ICONERROR); U::Core.AppendFailText(sMessage); }

MAKE_SIGNATURE(Get_TFPartyClient, "client.dll", "48 8B 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56", 0x0);
MAKE_SIGNATURE(Get_SteamNetworkingUtils, "client.dll", "40 53 48 83 EC ? 48 8B D9 48 8D 15 ? ? ? ? 33 C9 FF 15 ? ? ? ? 33 C9", 0x0);

bool CNullInterfaces::Initialize()
{
	try {
		if (S::Get_TFPartyClient.IsValid()) {
			try {
				I::TFPartyClient = S::Get_TFPartyClient.Call<CTFPartyClient*>();
				Validate(I::TFPartyClient);
			}
			catch (...) {
				U::Core.AppendFailText("CNullInterfaces::Initialize() exception calling Get_TFPartyClient");
				I::TFPartyClient = nullptr;
				m_bFailed = true;
			}
		}
		else {
			U::Core.AppendFailText("CNullInterfaces::Initialize() Get_TFPartyClient signature invalid");
			m_bFailed = true;
		}
		
		// Steam Client
		if (!I::SteamClient) {
			U::Core.AppendFailText("CNullInterfaces::Initialize() SteamClient is null");
			m_bFailed = true;
			return !m_bFailed;
		}

		HSteamPipe hsNewPipe = 0;
		try {
			hsNewPipe = I::SteamClient->CreateSteamPipe();
		}
		catch (...) {
			U::Core.AppendFailText("CNullInterfaces::Initialize() exception in CreateSteamPipe");
			m_bFailed = true;
			return !m_bFailed;
		}

		if (!hsNewPipe) {
			U::Core.AppendFailText("CNullInterfaces::Initialize() CreateSteamPipe failed");
			m_bFailed = true;
			return !m_bFailed;
		}

		HSteamPipe hsNewUser = 0;
		try {
			hsNewUser = I::SteamClient->ConnectToGlobalUser(hsNewPipe);
		}
		catch (...) {
			U::Core.AppendFailText("CNullInterfaces::Initialize() exception in ConnectToGlobalUser");
			m_bFailed = true;
			return !m_bFailed;
		}

		if (!hsNewUser) {
			U::Core.AppendFailText("CNullInterfaces::Initialize() ConnectToGlobalUser failed");
			m_bFailed = true;
			return !m_bFailed;
		}

		try {
			I::SteamFriends = I::SteamClient->GetISteamFriends(hsNewUser, hsNewPipe, STEAMFRIENDS_INTERFACE_VERSION);
			Validate(I::SteamFriends);

			I::SteamUtils = I::SteamClient->GetISteamUtils(hsNewUser, STEAMUTILS_INTERFACE_VERSION);
			Validate(I::SteamUtils);

			I::SteamApps = I::SteamClient->GetISteamApps(hsNewUser, hsNewPipe, STEAMAPPS_INTERFACE_VERSION);
			ValidateNonLethal(I::SteamApps);

			I::SteamUserStats = I::SteamClient->GetISteamUserStats(hsNewUser, hsNewPipe, STEAMUSERSTATS_INTERFACE_VERSION);
			Validate(I::SteamUserStats);

			I::SteamUser = I::SteamClient->GetISteamUser(hsNewUser, hsNewPipe, STEAMUSER_INTERFACE_VERSION);
			Validate(I::SteamUser);
		}
		catch (...) {
			U::Core.AppendFailText("CNullInterfaces::Initialize() exception getting Steam interfaces");
			m_bFailed = true;
			return !m_bFailed;
		}

		if (S::Get_SteamNetworkingUtils.IsValid()) {
			try {
				S::Get_SteamNetworkingUtils.Call<ISteamNetworkingUtils*>(&I::SteamNetworkingUtils);
				Validate(I::SteamNetworkingUtils);
			}
			catch (...) {
				U::Core.AppendFailText("CNullInterfaces::Initialize() exception calling Get_SteamNetworkingUtils");
				I::SteamNetworkingUtils = nullptr;
				m_bFailed = true;
			}
		}
		else {
			U::Core.AppendFailText("CNullInterfaces::Initialize() Get_SteamNetworkingUtils signature invalid");
			m_bFailed = true;
		}
	}
	catch (...) {
		U::Core.AppendFailText("CNullInterfaces::Initialize() caught unhandled exception");
		m_bFailed = true;
	}

	return !m_bFailed;
}