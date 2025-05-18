#pragma once
#include "Interface.h"
#include "../../../Utils/Memory/Memory.h"

MAKE_SIGNATURE(TFInventoryManager, "client.dll", "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 48 83 EC ? 4C 8D 0D", 0x0);
MAKE_SIGNATURE(CTFPlayerInventory_GetFirstItemOfItemDef, "client.dll", "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B FA 0F B7 E9", 0x0)

class CEconItemView
{
public:
	inline uint16 GetDefinitionIndex()
	{
		return *reinterpret_cast<uint16*>(this + 48);
	}

	inline uint64_t UUID()
	{
		uint64_t value = *reinterpret_cast<uint64_t*>(this + 96);
		auto a = value >> 32;
		auto b = value << 32;
		return b | a;
	}
};

#define SIZE_OF_ITEMVIEW 344
class CTFPlayerInventory
{
public:
	inline CEconItemView* GetFirstItemOfItemDef(uint16 iItemDef)
	{
		return S::CTFPlayerInventory_GetFirstItemOfItemDef.Call<CEconItemView*>(iItemDef, this);
	}

	inline int GetItemCount() 
	{
		return *reinterpret_cast<int*>(this + 112);
	}

	inline CEconItemView* GetItem(int idx)
	{
		uintptr_t uStart = *(uintptr_t*)(this + 96);
		uintptr_t nOffset = idx * SIZE_OF_ITEMVIEW;
		return reinterpret_cast<CEconItemView*>(uStart + nOffset);
	}

	inline std::vector<uint64_t> GetItemsOfItemDef(uint16 iItemDef)
	{
		std::vector<uint64_t> vUUIDs;
		for (int i = 0; i < GetItemCount(); i++)
		{
			auto pItem = GetItem(i);
			if (pItem->GetDefinitionIndex() == iItemDef)
				vUUIDs.push_back(pItem->UUID());
		}
		return vUUIDs;
	}
};

class CTFInventoryManager
{
public:
	inline bool EquipItemInLoadout(int iClass, int iSlot, uint64_t uniqueid)
	{
		return reinterpret_cast<bool (*)(void*, int, int, uint64_t)>(U::Memory.GetVFunc(this, 19))(this, iClass, iSlot, uniqueid);
	}

	inline CTFPlayerInventory* GetLocalInventory()
	{
		return reinterpret_cast<CTFPlayerInventory* (*)(void*)>(U::Memory.GetVFunc(this, 23))(this);
	}
};

namespace I
{
	inline CTFInventoryManager* TFInventoryManager()
	{
		return S::TFInventoryManager.Call<CTFInventoryManager*>();
	}
};