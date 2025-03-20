#pragma once
#include "../../../../../SDK/SDK.h"

#define MAX_CONTROL_POINTS 8
#define MAX_PREVIOUS_POINTS 3

struct CPInfo
{
	// Index in the ObjectiveResource
	int Idx{ -1 };
	std::optional<Vector> vPos{};
	// For BLU and RED to show who can and cannot cap
	std::array<bool, 2> bCanCap{};
	CPInfo( ) = default;
};

class CCPController
{
private:
	std::array<CPInfo, MAX_CONTROL_POINTS + 1> ControlPointData;
	CBaseTeamObjectiveResource* pObjectiveResource = nullptr;
	
	//Update
	void UpdateObjectiveResource( );
	void UpdateControlPoints( );

	struct PointIgnore
	{
		std::string szMapName;
		int PointIdx;
		PointIgnore( std::string str, int idx ) : szMapName{ std::move( str ) }, PointIdx{ idx } {};
	};

	// TODO: Find a way to fix these edge-cases.
	// clang-format off
	std::array<PointIgnore, 1> IgnorePoints
	{
		PointIgnore( "cp_steel", 4 )
	};
	// clang-format on

	bool TeamCanCapPoint( int index, int team );
	int GetPreviousPointForPoint( int index, int team, int previndex );
	int GetFarthestOwnedControlPoint( int team );

	// Can we cap this point?
	bool IsPointUseable( int index, int team );

public:
	// Get the closest Control Point we can cap
	std::optional<Vector> GetClosestControlPoint( Vector source, int team );

	void Init( );
	void Update( );
};

ADD_FEATURE( CCPController, CPController );