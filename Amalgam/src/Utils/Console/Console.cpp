#include "Console.h"

#include <Windows.h>
#include <memory>
#include <iostream>

void CConsole::Initialize(const char* title)
{
	AllocConsole();

	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(0)), "conin$", "r", static_cast<_iobuf*>(__acrt_iob_func(0)));
	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(1)), "conout$", "w", static_cast<_iobuf*>(__acrt_iob_func(1)));
	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(2)), "conout$", "w", static_cast<_iobuf*>(__acrt_iob_func(2)));

	SetConsoleTitleA(title);
}

void CConsole::Unload()
{
	fclose(static_cast<_iobuf*>(__acrt_iob_func(0)));
	fclose(static_cast<_iobuf*>(__acrt_iob_func(1)));
	fclose(static_cast<_iobuf*>(__acrt_iob_func(2)));

	FreeConsole();
}

void CConsole::Log(const char* text, ...)
{
	va_list ap;
	va_start(ap, text);

	char buffer[1024];
	vsprintf_s(buffer, text, ap);
	va_end(ap);

	std::cout << buffer << "\n" << std::endl;
}