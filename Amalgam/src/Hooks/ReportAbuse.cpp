#include "../SDK/SDK.h"

MAKE_SIGNATURE(AbuseReportManager, "client.dll", "48 8B 0D ? ? ? ? 48 8B 01 48 83 C4 ? 48 FF A0 ? ? ? ? 48 8B 0D", 0x0);
MAKE_SIGNATURE(ReportAbuse, "client.dll", "48 8B 0D ? ? ? ? 48 85 C9 75 ? 48 8D 0D ? ? ? ? 48 FF 25", 0x0);

MAKE_HOOK(ReportAbuse, S::ReportAbuse(), void*)
{
	*reinterpret_cast<void**>(U::Memory.RelToAbs(S::AbuseReportManager())) = nullptr;
	return CALL_ORIGINAL();
}