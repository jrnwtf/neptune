#include "Core.h"

#include "../SDK/SDK.h"
#include "../BytePatches/BytePatches.h"
#include "../Features/Visuals/Materials/Materials.h"
#include "../Features/Configs/Configs.h"
#include "../Features/Commands/Commands.h"
#include "../Features/ImGui/Menu/Menu.h"
#include "../Features/Visuals/Visuals.h"
#include "../SDK/Events/Events.h"
#include "../Features/Misc/NamedPipe/NamedPipe.h"
#include "../Utils/Hash/FNV1A.h"
#include "../Utils/Console/Console.h"

#include <Psapi.h>
#include <random>
#include <vector>
#include <TlHelp32.h>

static inline std::string GetProcessName(DWORD dwProcessID)
{
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessID);
	if (!hProcess)
		return "";

	char buffer[MAX_PATH];
	if (!GetModuleBaseName(hProcess, nullptr, buffer, sizeof(buffer) / sizeof(char)))
	{
		CloseHandle(hProcess);
		return "";
	}

	CloseHandle(hProcess);
	return buffer;
}

static inline bool CheckDXLevel()
{
	auto mat_dxlevel = U::ConVars.FindVar("mat_dxlevel");
	if (mat_dxlevel->GetInt() < 90)
	{
		//const char* sMessage = "You are running with graphics options that Amalgam does not support.\n-dxlevel must be at least 90.";
		const char* sMessage = "USE DXLEVEL 90 GAY ASS N1GGA";
		U::Core.AppendFailText(sMessage);
		SDK::Output("Amalgam", sMessage, { 175, 150, 255 }, true, true);
		//return false;
	}

	return true;
}

static inline void DumpClassIDs()
{
	std::ofstream fDump("ClassIDs.txt");
	fDump << "enum struct ETFClassID\n{\n";
	ClientClass* m_ClientClass = I::BaseClientDLL->GetAllClasses();

	while (m_ClientClass)
	{
		fDump << "	" << m_ClientClass->GetName() << " = " << m_ClientClass->m_ClassID << ",\n";
		m_ClientClass = m_ClientClass->m_pNext;
	}

	fDump << "}";
	fDump.close();
}

std::ofstream File;
const char *szClassName;

static inline void DumpTable(RecvTable* pTable, int nDepth)
{
	if (!pTable)
		return;

	const char* Types[7] = { "int", "float", "Vec3", "Vec2", "const char *", "Array", "void *" };

	if (nDepth == 0)
		File << "class " << szClassName << "\n{\npublic:\n";

	for (int n = 0; n < pTable->m_nProps; n++)
	{
		RecvProp* pProp = pTable->GetProp(n);

		if (!pProp)
			continue;

		std::string_view sVarName(pProp->m_pVarName);

		if (!sVarName.find("baseclass") || !sVarName.find("0") || !sVarName.find("1") || !sVarName.find("2"))
			continue;

		const char* szType = Types[pProp->GetType()];

		if (sVarName.find("m_b") == 0 && pProp->GetType() == 0)
			szType = "bool";

		if (sVarName.find("m_vec") == 0)
			szType = "Vec3";

		if (sVarName.find("m_h") == 0)
			szType = "EHandle";

		if (pProp->GetOffset())
			File << "\tNetVar(" << sVarName << ", " << szType << ", \"" << szClassName << "\", \"" << sVarName << "\");\n";

		if (auto DataTable = pProp->GetDataTable())
			DumpTable(DataTable, nDepth + 1);
	}

	if (nDepth == 0)
		File << "};\n";
}

static inline void DumpTables()
{
	File.open("NetVars.h");

	for (ClientClass* pClass = I::BaseClientDLL->GetAllClasses(); pClass; pClass = pClass->m_pNext)
	{
		szClassName = pClass->m_pNetworkName;
		DumpTable(pClass->m_pRecvTable, 0);
	}

	File.close();
}

const char* CCore::SearchForDLL(const char* pszDLLSearch)
{
	HANDLE hProcessSnap = INVALID_HANDLE_VALUE;
	PROCESSENTRY32 pe32;
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE)
		return pszDLLSearch;

	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hProcessSnap, &pe32))
	{
		CloseHandle(hProcessSnap);
		return pszDLLSearch;
	}

	do
	{
		if (pe32.szExeFile == strstr(pe32.szExeFile, "tf_win64.exe"))
		{
			HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
			MODULEENTRY32 me32;
			hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pe32.th32ProcessID);
			if (hModuleSnap == INVALID_HANDLE_VALUE)
				break;

			me32.dwSize = sizeof(MODULEENTRY32);
			if (!Module32First(hModuleSnap, &me32))
			{
				CloseHandle(hModuleSnap);
				break;
			}

			do
			{
				if (strstr(me32.szModule, pszDLLSearch))
				{
					CloseHandle(hProcessSnap);
					CloseHandle(hModuleSnap);
					return me32.szModule;
				}
			} while (Module32Next(hModuleSnap, &me32));

			CloseHandle(hModuleSnap);
			break;
		}
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);
	return pszDLLSearch;
}

void CCore::AppendFailText(const char* sMessage)
{
	m_ssFailStream << std::format("{}\n", sMessage);
	OutputDebugStringA(std::format("{}\n", sMessage).c_str());
}

void CCore::LogFailText()
{
	m_ssFailStream << "\nBuilt @ " __DATE__ ", " __TIME__ ", " __CONFIGURATION__ "\n";
	m_ssFailStream << "Ctrl + C to copy. \n";
	try
	{
		std::ofstream file;
		file.open(F::Configs.m_sConfigPath + "fail_log.txt", std::ios_base::app);
		file << m_ssFailStream.str() + "\n\n\n";
		file.close();
		m_ssFailStream << "Logged to Amalgam\\fail_log.txt. ";
	}
	catch (...) {}
#ifndef TEXTMODE
	SDK::Output("Failed to load", m_ssFailStream.str().c_str(), {}, false, true, false, false, false, false, MB_OK | MB_ICONERROR);
#endif
}

static bool ModulesLoaded()
{
#ifndef TEXTMODE
	if (!SDK::GetTeamFortressWindow())
		return false;
#endif

	if (GetModuleHandleA("client.dll"))
	{
		// Check I::ClientModeShared and I::UniformRandomStream here so that we wait until they get initialized
		auto dwDest = U::Memory.FindSignature("client.dll", "48 8B 0D ? ? ? ? 48 8B 10 48 8B 19 48 8B C8 FF 92");
		if (!dwDest || !*reinterpret_cast<void**>(U::Memory.RelToAbs(dwDest)))
			return false;

		dwDest = U::Memory.FindSignature("client.dll", "48 8B 0D ? ? ? ? F3 0F 59 CA 44 8D 42");
		if (!dwDest || !*reinterpret_cast<void**>(U::Memory.RelToAbs(dwDest)))
			return false;
	}
	else 
		return false;

	return GetModuleHandleA("engine.dll") &&
		GetModuleHandleA("server.dll") &&
		GetModuleHandleA("tier0.dll") &&
		GetModuleHandleA("vstdlib.dll") &&
		GetModuleHandleA("vgui2.dll") &&
		GetModuleHandleA("vguimatsurface.dll") &&
		GetModuleHandleA("materialsystem.dll") &&
		GetModuleHandleA("inputsystem.dll") &&
		GetModuleHandleA("vphysics.dll") &&
		GetModuleHandleA("steamclient64.dll") &&
		FNV1A::Hash32(U::Core.SearchForDLL("shaderapi")) != FNV1A::Hash32Const("shaderapi");
}

void CCore::Load()
{
	if (m_bUnload = m_bFailed = FNV1A::Hash32(GetProcessName(GetCurrentProcessId()).c_str()) != FNV1A::Hash32Const("tf_win64.exe"))
	{
		AppendFailText("Invalid process");
		return;
	}

	float flTime = 0.f;
	while (!ModulesLoaded())
	{
		Sleep(500), flTime += 0.5f;
		if (m_bUnload = m_bFailed = flTime >= 60.f)
		{
			AppendFailText("Failed to load");
			return;
		}
		if (m_bUnload = m_bFailed = U::KeyHandler.Down(VK_F11, true))
		{
			AppendFailText("Cancelled load");
			return;
		}
	}

	U::Console.Initialize("Amalgam");

	if (m_bUnload = m_bFailed = !U::Signatures.Initialize() || !U::Interfaces.Initialize() || !CheckDXLevel())
		return;

	if (m_bUnload = m_bFailed2 = !U::Hooks.Initialize() || !U::BytePatches.Initialize() || !H::Events.Initialize())
		return;

	DumpClassIDs();
	DumpTables();

#ifndef TEXTMODE
	F::Materials.LoadMaterials();
#endif
	U::ConVars.Initialize();
	F::Commands.Initialize();
	F::NamedPipe::Initialize();

	F::Configs.LoadConfig(F::Configs.m_sCurrentConfig, false);
	F::Configs.m_bConfigLoaded = true;

#ifdef TEXTMODE
	I::EngineClient->ClientCmd_Unrestricted(std::format("cl_hud_playerclass_use_playermodel 0").c_str());
	I::EngineClient->ClientCmd_Unrestricted(std::format("exec autoexec").c_str());
#endif

	SDK::Output("Amalgam", "Loaded", { 175, 150, 255 }, true, true, true);
}

void CCore::Loop()
{
	while (true)
	{
#ifdef TEXTMODE
		if (m_bUnload)
			break;		
#else
		bool bShouldUnload = U::KeyHandler.Down(VK_F11) && SDK::IsGameWindowInFocus() || m_bUnload;
		if (bShouldUnload)
			break;
#endif 
		Sleep(15);
	}
}

void CCore::Unload()
{
	if (m_bFailed)
	{
		LogFailText();
		return;
	}

	G::Unload = true;
	m_bFailed2 = !U::Hooks.Unload() || m_bFailed2;
	U::BytePatches.Unload();
	H::Events.Unload();

	if (F::Menu.m_bIsOpen)
		I::MatSystemSurface->SetCursorAlwaysVisible(false);
	F::Visuals.RestoreWorldModulation();
	if (I::Input->CAM_IsThirdPerson())
	{
		if (auto pLocal = H::Entities.GetLocal())
		{
			I::Input->CAM_ToFirstPerson();
			pLocal->ThirdPersonSwitch();
		}
	}
	U::ConVars.FindVar("cl_wpn_sway_interp")->SetValue(0.f);
	U::ConVars.FindVar("cl_wpn_sway_scale")->SetValue(0.f);

	Sleep(250);
	F::NamedPipe::Shutdown();
	U::ConVars.Unload();
	F::Materials.UnloadMaterials();
	U::Console.Unload();

	if (m_bFailed2)
	{
		LogFailText();
		return;
	}
	
	SDK::Output("Amalgam", "Unloaded", { 175, 150, 255 }, true, true);
}