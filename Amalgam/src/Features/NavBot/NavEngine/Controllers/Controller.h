#pragma once
#include "../../../../SDK/SDK.h"

class CGameObjectiveController
{
public:
	ETFGameType m_eGameMode = TF_GAMETYPE_UNDEFINED;
	bool m_bDoomsday = false;
	void Update( );
	void Reset( );
};

ADD_FEATURE( CGameObjectiveController, GameObjectiveController );