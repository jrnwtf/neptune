#pragma once
#include "../../../../../SDK/SDK.h"

struct PLInfo
{
	CBaseEntity* pEntity{};
	std::optional<Vector> vPos;
	PLInfo( ) = default;
};

class CPLController
{
private:
	// Valid_ents that controls all the payloads for each team. Red team is first, then comes blue team.
	std::array<std::vector<CBaseEntity*>, 2> Payloads;

public:
	// Get the closest Control Payload
	std::optional<Vector> GetClosestPayload( Vector source, int team );

	void Init( );
	void Update( );
};

ADD_FEATURE( CPLController, PLController );