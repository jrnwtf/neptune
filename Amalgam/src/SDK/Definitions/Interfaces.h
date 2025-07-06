#pragma once

#include "../../Utils/Feature/Feature.h"

#include "Interfaces/CClientModeShared.h"
#include "Interfaces/CClientState.h"
#include "Interfaces/CGlobalVarsBase.h"
#include "Interfaces/CTFGameRules.h"
#include "Interfaces/CTFGCClientSystem.h"
#include "Interfaces/CTFPartyClient.h"
#include "Interfaces/IBaseClientDLL.h"
#include "Interfaces/IClientEntityList.h"
#include "Interfaces/ICVar.h"
#include "Interfaces/IEngineTrace.h"
#include "Interfaces/IEngineVGui.h"
#include "Interfaces/IGameEvents.h"
#include "Interfaces/IGameMovement.h"
#include "Interfaces/IInput.h"
#include "Interfaces/IInputSystem.h"
#include "Interfaces/IKeyValuesSystem.h"
#ifndef TEXTMODE
#include "Interfaces/IMaterialSystem.h"
#include "Interfaces/IMatSystemSurface.h"
#endif
#include "Interfaces/IMoveHelper.h"
#include "Interfaces/IPanel.h"
#ifndef TEXTMODE
#include "Interfaces/IStudioRender.h"
#endif
#include "Interfaces/IUniformRandomStream.h"
#include "Interfaces/IVEngineClient.h"
#ifndef TEXTMODE
#include "Interfaces/IViewRender.h"
#endif
#include "Interfaces/IVModelInfo.h"
#ifndef TEXTMODE
#include "Interfaces/IVModelRender.h"
#include "Interfaces/IVRenderView.h"
#endif
#include "Interfaces/Prediction.h"
#include "Interfaces/SteamInterfaces.h"
#include "Interfaces/ViewRenderBeams.h"
#include "Interfaces/VPhysics.h"
#include "Interfaces/CServerTools.h"
#include "Interfaces/CTFInventoryManager.h"

#ifndef TEXTMODE
#include <d3d9.h>
MAKE_INTERFACE_SIGNATURE_SEARCH(IDirect3DDevice9, DirectXDevice, "shaderapi", "48 8B 0D ? ? ? ? 48 8B 01 FF 50 ? 8B F8", 0x0, 1)
#endif

class CNullInterfaces
{
private:
	bool m_bFailed = false;

public:
	bool Initialize();
};

ADD_FEATURE_CUSTOM(CNullInterfaces, Interfaces, H);