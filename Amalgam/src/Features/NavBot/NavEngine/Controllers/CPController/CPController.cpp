#include "CPController.h"

//TODO: fix

void CCPController::UpdateObjectiveResource( )
{
	// Get ObjectiveResource
	pObjectiveResource = H::Entities.GetOR( );
}

// Don't constantly update the cap status
static Timer capstatus_update{};
void CCPController::UpdateControlPoints( )
{
	// No objective resource, can't run
	if ( !pObjectiveResource )
		return;

	const int num_cp = pObjectiveResource->m_iNumControlPoints( );

	// No control points
	if ( !num_cp )
		return;

	// Clear the invalid controlpoints
	if ( num_cp <= MAX_CONTROL_POINTS )
	{
		for ( int i = num_cp; i < MAX_CONTROL_POINTS; ++i )
			ControlPointData[ i ] = CPInfo( );
	}

	for ( int i = 0; i < num_cp; ++i )
	{
		auto& data = ControlPointData[ i ];
		data.Idx = i;

		// Update position
		data.vPos = pObjectiveResource->CPPositions( i );
	}

	if ( capstatus_update.Run( 1.f ) )
	{
		for ( int i = 0; i < num_cp; ++i )
		{
			auto& data = ControlPointData[ i ];
			
			// Check accessibility for both teams, requires alot of checks
			const bool CanCapRED = IsPointUseable( i, TF_TEAM_RED );
			const bool CanCapBLU = IsPointUseable( i, TF_TEAM_BLUE );
			
			data.bCanCap.at( 0 ) = CanCapRED;
			data.bCanCap.at( 1 ) = CanCapBLU;
		}
	}
}

bool CCPController::TeamCanCapPoint( int index, int team )
{
	const int arr_index = index + team * MAX_CONTROL_POINTS;
	return pObjectiveResource->TeamCanCap( arr_index );
}

int CCPController::GetPreviousPointForPoint( int index, int team, int previndex )
{
	const int iIntIndex = previndex + ( index * MAX_PREVIOUS_POINTS ) + ( team * MAX_CONTROL_POINTS * MAX_PREVIOUS_POINTS );
	return pObjectiveResource->PreviousPoints( iIntIndex );
}

int CCPController::GetFarthestOwnedControlPoint( int team )
{
	const int iOwnedEnd = pObjectiveResource->BaseControlPoints( team );
	if ( iOwnedEnd == -1 )
		return -1;

	const int iNumControlPoints = pObjectiveResource->m_iNumControlPoints( );
	int iWalk = 1;
	int iEnemyEnd = iNumControlPoints - 1;
	if ( iOwnedEnd != 0 )
	{
		iWalk = -1;
		iEnemyEnd = 0;
	}

	// Walk towards the other side, and find the farthest owned point
	int iFarthestPoint = iOwnedEnd;
	for ( int iPoint = iOwnedEnd; iPoint != iEnemyEnd; iPoint += iWalk )
	{
		// If we've hit a point we don't own, we're done
		if ( pObjectiveResource->OwningTeam( iPoint ) != team )
			break;

		iFarthestPoint = iPoint;
	}

	return iFarthestPoint;
}

bool CCPController::IsPointUseable( int index, int team )
{
	// We Own it, can't cap it
	if ( pObjectiveResource->OwningTeam( index ) == team )
		return false;

	// Can we cap the point?
	if ( !TeamCanCapPoint( index, team ) )
		return false;

	// We are playing a sectioned map, check if the CP is in it
	if ( pObjectiveResource->m_bPlayingMiniRounds( ) && !pObjectiveResource->InMiniRound( index) )
		return false;

	// Is the point locked?
	if ( pObjectiveResource->CPLocked( index ) )
		return false;

	// Linear cap means that it won't require previous points, bail
	static auto tf_caplinear = U::ConVars.FindVar( "tf_caplinear" );
	if ( tf_caplinear && !tf_caplinear->GetBool( ) )
		return true;

	// Any previous points necessary?
	int iPointNeeded = GetPreviousPointForPoint( index, team, 0 );

	// Points set to require themselves are always cappable
	if ( iPointNeeded == index )
		return true;

	// No required points specified? Require all previous points.
	if ( iPointNeeded == -1 )
	{
		// No Mini rounds
		if ( !pObjectiveResource->m_bPlayingMiniRounds( ) )
		{
			// No custom previous point, team must own all previous points
			const int iFarthestPoint = GetFarthestOwnedControlPoint( team );
			return ( abs( iFarthestPoint - index ) <= 1 );
		}
		// We got a section map
		else
			// Tf2 itself does not seem to have any more code for this, so here goes
			return true;
	}

	// Loop through each previous point and see if the team owns it
	for ( int iPrevPoint = 0; iPrevPoint < MAX_PREVIOUS_POINTS; iPrevPoint++ )
	{
		iPointNeeded = GetPreviousPointForPoint( index, team, iPrevPoint );
		if ( iPointNeeded != -1 )
		{
			// We don't own the needed points
			if ( pObjectiveResource->OwningTeam( iPointNeeded ) != team )
				return false;
		}
	}
	return true;
}

std::optional<Vector> CCPController::GetClosestControlPoint( Vector source, int team )
{
	// No resource for it
	if ( !pObjectiveResource )
		return std::nullopt;

	// Check if it's a cp map
	//static auto tf_gamemode_cp = U::ConVars.FindVar( "tf_gamemode_cp" );
	//if ( !tf_gamemode_cp )
	//{
	//	tf_gamemode_cp = U::ConVars.FindVar( "tf_gamemode_cp" );
	//	return std::nullopt;
	//}

	//if ( !tf_gamemode_cp->GetBool( ) )
	//	return std::nullopt;

	// Map team to 0-1 and check If Valid
	int team_idx = team - TF_TEAM_RED;
	if ( team_idx < 0 || team_idx > 1 )
		return std::nullopt;

	// No controlpoints
	if ( !pObjectiveResource->m_iNumControlPoints( ) )
		return std::nullopt;

	int IgnoreIdx = -1;
	// Do the points need checking because of the map?
	const auto& levelname = SDK::GetLevelName( );
	for ( const auto& ignore : IgnorePoints )
	{
		// Try to find map name in bad point array
		if ( levelname.find( ignore.szMapName ) != std::string::npos )
			IgnoreIdx = ignore.PointIdx;
	}

	// Find the best and closest control point
	std::optional<Vector> BestControlPoint;
	float flBestDist = FLT_MAX;
	for ( const auto& ControlPoint : ControlPointData )
	{
		// Ignore this point
		if ( ControlPoint.Idx == IgnoreIdx )
			continue;

		// They can cap
		if ( ControlPoint.bCanCap.at( team_idx ) )
		{
			const auto flDist = ( *ControlPoint.vPos ).DistToSqr( source );
			// Is it closer?
			if ( ControlPoint.vPos && flDist < flBestDist )
			{
				flBestDist = flDist;
				BestControlPoint = ControlPoint.vPos;
			}
		}
	}

	return BestControlPoint;
}

void CCPController::Init( )
{
	for ( auto& cp : ControlPointData )
		cp = CPInfo( );

	pObjectiveResource = nullptr;
}

void CCPController::Update( )
{
	UpdateObjectiveResource( );
	UpdateControlPoints( );
}
