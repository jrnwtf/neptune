#include <Windows.h>
#include "Core/Core.h"
#include "Utils/CrashLog/CrashLog.h"
#include "Utils/Memory/Memory.h"

DWORD WINAPI MainThread(LPVOID lpParam)
{
    while (!GetModuleHandleA("client.dll") || !GetModuleHandleA("engine.dll"))
        Sleep(100);
    U::Core.Load();
    U::Core.Loop();
    
    CrashLog::Unload();
    U::Core.Unload();

    FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), EXIT_SUCCESS);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);

        CrashLog::Initialize();

        if (const auto hMainThread = CreateThread(nullptr, 0, MainThread, hinstDLL, 0, nullptr))
        {
            U::Memory.HideThread(hMainThread);
            CloseHandle(hMainThread);
        }
    }

    return TRUE;
}