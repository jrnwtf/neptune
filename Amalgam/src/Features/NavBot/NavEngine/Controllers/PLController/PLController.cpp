#include "PLController.h"

void CPLController::Init( )
{
	// Reset entries
	for ( auto& entry : Payloads )
		entry.clear( );
}

void CPLController::Update( )
{
	// We should update the payload list
	{
		// Reset entries
		for ( auto& entry : Payloads )
			entry.clear( );

		for ( const auto& pPayload : H::Entities.GetGroup(EGroupType::WORLD_OBJECTIVE) )
		{
			if ( !pPayload || pPayload->GetClassID( ) != ETFClassID::CObjectCartDispenser)
				continue;

			const int iTeam = pPayload->m_iTeamNum( );

			// Not the object we need
			if ( iTeam < TF_TEAM_RED || iTeam > TF_TEAM_BLUE )
				continue;

			// Add new entry for the team
			Payloads.at( iTeam - TF_TEAM_RED ).push_back( pPayload );
		}
	}
}

std::optional<Vector> CPLController::GetClosestPayload( Vector source, int team )
{
	// Invalid team
	if ( team < TF_TEAM_RED || team > TF_TEAM_BLUE )
		return std::nullopt;

	// Convert to index
	const int index = team - TF_TEAM_RED;
	const auto& entry = Payloads[ index ];

	float flBestDist = FLT_MAX;
	std::optional<Vector> vBestPos;

	// Find best payload
	for ( const auto& payload : entry )
	{
		if ( !payload )
			continue;

		if ( payload->GetClassID( ) != ETFClassID::CObjectCartDispenser || payload->IsDormant( ) )
			continue;

		const auto vOrigin = payload->GetAbsOrigin( );
		const auto flDist = vOrigin.DistToSqr( source );
		if ( flDist < flBestDist )
		{
			vBestPos = vOrigin;
			flBestDist = flDist;
		}
	}
	return vBestPos;
}