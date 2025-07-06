#include "../SDK/SDK.h"

MAKE_SIGNATURE(GenerateEquipRegionConflictMask, "client.dll", "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 33 FF 41 8B E8", 0x0);
MAKE_SIGNATURE(CTFInventoryManager_GetItemInLoadoutForClass, "client.dll", "48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 57 48 83 EC ? 81 60", 0x0);
MAKE_SIGNATURE(CTFPlayerInventory_VerifyChangedLoadoutsAreValid, "client.dll", "41 56 48 83 EC ? 48 8B 05 ? ? ? ? 48 8D 54 24", 0x0);
MAKE_SIGNATURE(CEquipSlotItemSelectionPanel_UpdateModelPanelsForSelection_GetItemInLoadoutForClass_Call, "client.dll", "48 85 C0 74 ? 48 8D 48 ? 48 8B 40 ? FF 50 ? 44 0B A0", 0x0);

MAKE_HOOK(GenerateEquipRegionConflictMask, S::GenerateEquipRegionConflictMask(), uint32_t,
	int iClass, int iUpToSlot, int iIgnoreSlot)
{
    HOOK_TRY
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::GenerateEquipRegionConflictMask[DEFAULT_BIND])
		return CALL_ORIGINAL(iClass, iUpToSlot, iIgnoreSlot);
#endif

	return Vars::Misc::Exploits::EquipRegionUnlock.Value ? 0 : CALL_ORIGINAL(iClass, iUpToSlot, iIgnoreSlot);
    HOOK_CATCH("GenerateEquipRegionConflictMask", uint32_t)
}

MAKE_HOOK(CTFInventoryManager_GetItemInLoadoutForClass, S::CTFInventoryManager_GetItemInLoadoutForClass(), void*,
	void* rcx, int iClass, int iSlot, CSteamID* pID)
{
    HOOK_TRY
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::GenerateEquipRegionConflictMask[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, iClass, iSlot, pID);
#endif

	static const auto dwDesired = S::CEquipSlotItemSelectionPanel_UpdateModelPanelsForSelection_GetItemInLoadoutForClass_Call();
	const auto dwRetAddr = uintptr_t(_ReturnAddress());

	return dwRetAddr == dwDesired && Vars::Misc::Exploits::EquipRegionUnlock.Value ? nullptr : CALL_ORIGINAL(rcx, iClass, iSlot, pID);
    HOOK_CATCH("CTFInventoryManager_GetItemInLoadoutForClass", void*)
}

MAKE_HOOK(CTFPlayerInventory_VerifyChangedLoadoutsAreValid, S::CTFPlayerInventory_VerifyChangedLoadoutsAreValid(), void,
	void* rcx)
{
    HOOK_TRY
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::GenerateEquipRegionConflictMask[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx);
#endif

	if (!Vars::Misc::Exploits::EquipRegionUnlock.Value)
		CALL_ORIGINAL(rcx);
    HOOK_CATCH("CTFPlayerInventory_VerifyChangedLoadoutsAreValid", void)
}
