#include "../SDK/SDK.h"

MAKE_SIGNATURE(Host_Error, "engine.dll", "48 89 4C 24 ? 48 89 54 24 ? 4C 89 44 24 ? 4C 89 4C 24 ? 48 81 EC 28 04 00 00", 0x0);

MAKE_HOOK(Host_Error, S::Host_Error(), void,
	const char* pInMessage, ...)
{
	// dont send host errors
}