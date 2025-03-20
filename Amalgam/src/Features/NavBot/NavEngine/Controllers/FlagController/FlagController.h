#pragma once
#include "../../../../../SDK/SDK.h"

struct FlagInfo
{
	CCaptureFlag* pFlag{ nullptr };
	int iTeam{ TEAM_UNASSIGNED };
	FlagInfo( ) = default;
	FlagInfo( CCaptureFlag* pFlag, int iTeam )
	{
		this->pFlag	 = pFlag;
		this->iTeam		 = iTeam;
	}
};

class CFlagController
{
private:
	std::vector<FlagInfo> m_vFlags;
	std::unordered_map<int, Vector> m_mSpawnPositions;
	//bool IsCTF = true;
	//int& GetFlagType( CBaseEntity* flag );
	//int& GetFlagStatus( CBaseEntity* flag );
public:
	// Use incase you don't get the needed information from the functions below
	FlagInfo GetFlag( int team );

	Vector GetPosition( CCaptureFlag* pFlag );
	std::optional<Vector> GetPosition( int iTeam );
	std::optional<Vector> GetSpawnPosition( int iTeam );
	int GetCarrier( CCaptureFlag* pFlag );
	int GetCarrier( int iTeam );
	int GetStatus( CCaptureFlag* pFlag );
	int GetStatus( int iTeam );

	void Init( );
	void Update( );

};

ADD_FEATURE( CFlagController, FlagController );