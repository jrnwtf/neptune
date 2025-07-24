#include "Memory.h"

#include <format>
#include <Psapi.h>

#define INRANGE(x, a, b) (x >= a && x <= b) 
#define GetBits(x) (INRANGE((x & (~0x20)),'A','F') ? ((x & (~0x20)) - 'A' + 0xA) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define GetBytes(x) (GetBits(x[0]) << 4 | GetBits(x[1]))

std::vector<byte> CMemory::PatternToByte(const char* szPattern)
{
	std::vector<byte> vPattern = {};

	const auto pStart = const_cast<char*>(szPattern);
	const auto pEnd = const_cast<char*>(szPattern) + strlen(szPattern);
	for (char* pCurrent = pStart; pCurrent < pEnd; ++pCurrent)
		vPattern.push_back(byte(std::strtoul(pCurrent, &pCurrent, 16)));

	return vPattern;
}

std::vector<int> CMemory::PatternToInt(const char* szPattern)
{
	std::vector<int> vPattern = {};

	const auto pStart = const_cast<char*>(szPattern);
	const auto pEnd = const_cast<char*>(szPattern) + strlen(szPattern);
	for (char* pCurrent = pStart; pCurrent < pEnd; ++pCurrent)
	{
		if (*pCurrent == '?') // Is current byte a wildcard? Simply ignore that that byte later
		{
			++pCurrent;
			if (*pCurrent == '?') // Check if following byte is also a wildcard
				++pCurrent;

			vPattern.push_back(-1);
		}
		else
			vPattern.push_back(std::strtoul(pCurrent, &pCurrent, 16));
	}

	return vPattern;
}

uintptr_t CMemory::FindSignature(const char* szModule, const char* szPattern)
{
	if (const auto hMod = GetModuleHandleA(szModule))
	{
		// Get module information to search in the given module
		MODULEINFO lpModuleInfo;
		if (!GetModuleInformation(GetCurrentProcess(), hMod, &lpModuleInfo, sizeof(MODULEINFO)))
			return 0x0;

		// The region where we will search for the byte sequence
		const auto dwImageSize = lpModuleInfo.SizeOfImage;

		// Check if the image is faulty
		if (!dwImageSize)
			return 0x0;

		// Convert IDA-Style signature to a byte sequence
		const auto vPattern = PatternToInt(szPattern);
		const auto iPatternSize = vPattern.size();
		const int* iPatternBytes = vPattern.data();

		const auto pImageBytes = reinterpret_cast<byte*>(hMod);

		// Now loop through all bytes and check if the byte sequence matches
		for (auto i = 0ul; i < dwImageSize - iPatternSize; ++i)
		{
			auto bFound = true;

			// Go through all bytes from the signature and check if it matches
			for (auto j = 0ul; j < iPatternSize; ++j)
			{
				if (pImageBytes[i + j] != iPatternBytes[j] // Bytes don't match
					&& iPatternBytes[j] != -1)             // Byte isn't a wildcard either
				{
					bFound = false;
					break;
				}
			}

			if (bFound)
				return uintptr_t(&pImageBytes[i]);
		}

		return 0x0;
	}

	return 0x0;
}

using CreateInterfaceFn = void*(*)(const char* pName, int* pReturnCode);

PVOID CMemory::FindInterface(const char* szModule, const char* szObject)
{
	const auto hModule = GetModuleHandleA(szModule);
	if (!hModule)
		return nullptr;

	const auto fnCreateInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hModule, "CreateInterface"));
	if (!fnCreateInterface)
		return nullptr;

	return fnCreateInterface(szObject, nullptr);
}

std::string CMemory::GetModuleOffset(uintptr_t uAddress)
{
	HMODULE hModule;
	if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, LPCSTR(uAddress), &hModule))
		return std::format("{:#x}", uAddress);

	uintptr_t uBase = uintptr_t(hModule);
	char buffer[MAX_PATH];
	if (!GetModuleBaseName(GetCurrentProcess(), hModule, buffer, sizeof(buffer) / sizeof(char)))
		return std::format("{:#x}+{:#x}", uBase, uAddress - uBase);

	return std::format("{}+{:#x}", buffer, uAddress - uBase);
}

HMODULE CMemory::GetModuleHandleSafe(const std::string& pszModuleName)
{
	HMODULE hmModuleHandle = NULL;

	do
	{
		if (pszModuleName.empty())
			hmModuleHandle = GetModuleHandleA(NULL);
		else
			hmModuleHandle = GetModuleHandleA(pszModuleName.c_str());

		std::this_thread::sleep_for(std::chrono::microseconds(100));
	} while (hmModuleHandle == NULL);

	return hmModuleHandle;
}

std::pair<DWORD, DWORD> CMemory::GetModuleSize(const std::string& pszModuleName)
{
	HMODULE hmModule = GetModuleHandleSafe(pszModuleName);

	PIMAGE_DOS_HEADER pDOSHeader = (PIMAGE_DOS_HEADER)hmModule;
	PIMAGE_NT_HEADERS pNTHeaders = (PIMAGE_NT_HEADERS)(((DWORD)hmModule) + pDOSHeader->e_lfanew);

	DWORD dwAddress = ((DWORD)hmModule) + pNTHeaders->OptionalHeader.BaseOfCode;
	DWORD dwLength = ((DWORD)hmModule) + pNTHeaders->OptionalHeader.SizeOfCode;

	return { dwAddress, dwLength };
}

PVOID CMemory::GetVirtualFunction(PVOID inst, DWORD index)
{
	if (inst == nullptr)
		return nullptr;

	PDWORD table = *(reinterpret_cast<PDWORD*>(inst));

	if (table == nullptr)
		return nullptr;

	return reinterpret_cast<PVOID>(table[index]);
}

DWORD CMemory::CalcInstAddress(DWORD inst)
{
	if (!inst)
		return 0;

	switch (*reinterpret_cast<BYTE*>(inst))
	{
	// call ?? ?? ?? ??
	case 0xE8:
	{
		DWORD target = *reinterpret_cast<DWORD*>(inst + 1);
		DWORD next = inst + 5;
		return target + next;
	}

	// jmp ?? ?? ?? ??
	case 0xE9:
	{
		DWORD target = *reinterpret_cast<DWORD*>(inst + 1);
		DWORD next = inst + 5;
		return target + next;
	}

	// mov ?? ?? ?? ?? ??
	case 0x8B:
	{
		switch (*reinterpret_cast<BYTE*>(inst + 1))
		{
		// mov ?? ?? ?? ?? ??
		case 0x0D:
		{
			DWORD target = *reinterpret_cast<DWORD*>(inst + 2);
			return target;
		}
		}

		break;
	}

	// mov ?? ?? ?? ??
	case 0xB9:
	{
		DWORD target = *reinterpret_cast<DWORD*>(inst + 1);
		return target;
	}
	}

	return inst;
}

BOOL CMemory::RemoveEntryList(PLIST_ENTRY Entry)
{
	PLIST_ENTRY Blink, Flink;

	Flink = Entry->Flink; Blink = Entry->Blink;
	Blink->Flink = Flink; Flink->Blink = Blink;

	return (BOOL)(Flink == Blink);
}

BOOL CMemory::ClearPEHeader(HINSTANCE hModule)
{
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;

	if (pDosHeader->e_magic == IMAGE_DOS_SIGNATURE)
	{
		PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((DWORD)pDosHeader + (DWORD)pDosHeader->e_lfanew);

		if (pNtHeader->Signature == IMAGE_NT_SIGNATURE)
		{
			DWORD dwOld, dwOld2, dwSize = pNtHeader->OptionalHeader.SizeOfHeaders;

			if (VirtualProtect((LPVOID)pDosHeader, dwSize, PAGE_EXECUTE_READWRITE, &dwOld))
			{
				ZeroMemory(pDosHeader, dwSize);
				VirtualProtect((LPVOID)pDosHeader, dwSize, dwOld, &dwOld2);
				return TRUE;
			}
		}
	}

	return FALSE;
}

bool CMemory::RemoveHeader(HINSTANCE hinstDLL)
{
	DWORD ERSize = 0;
	DWORD Protect = 0;
	DWORD dwStartOffset = (DWORD)hinstDLL;

	IMAGE_DOS_HEADER* pDosHeader = (PIMAGE_DOS_HEADER)dwStartOffset;
	IMAGE_NT_HEADERS* pNtHeader = (PIMAGE_NT_HEADERS)(dwStartOffset + pDosHeader->e_lfanew);

	ERSize = sizeof(IMAGE_NT_HEADERS);

	if (VirtualProtect(pDosHeader, ERSize, PAGE_EXECUTE_READWRITE, &Protect))
	{
		for (DWORD i = 0; i < ERSize - 1; i++)
			*(PBYTE)((DWORD)pDosHeader + i) = 0x00;
	}
	else
		return false;

	VirtualProtect(pDosHeader, ERSize, Protect, 0);

	ERSize = sizeof(IMAGE_DOS_HEADER);

	if ((pNtHeader != 0) && VirtualProtect(pNtHeader, ERSize, PAGE_EXECUTE_READWRITE, &Protect))
	{
		for (DWORD i = 0; i < ERSize - 1; i++)
			*(PBYTE)((DWORD)pNtHeader + i) = 0x00;
	}
	else
		return false;

	VirtualProtect(pNtHeader, ERSize, Protect, 0);
	return true;
}

bool CMemory::HideThread(HANDLE hThread)
{
	typedef NTSTATUS(NTAPI* pNtSetInformationThread)(HANDLE, UINT, PVOID, ULONG);
	NTSTATUS Status;

	pNtSetInformationThread NtSIT = (pNtSetInformationThread)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetInformationThread");

	if (NtSIT == NULL)
	{
		return false;
	}

	if (hThread == NULL)
		Status = NtSIT(GetCurrentThread(), 0x11, 0, 0);
	else
		Status = NtSIT(hThread, 0x11, 0, 0);

	if (Status != 0x00000000)
		return false;
	else
		return true;
}