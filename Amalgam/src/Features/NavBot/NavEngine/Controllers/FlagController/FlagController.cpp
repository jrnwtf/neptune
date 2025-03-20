#include "FlagController.h"

FlagInfo CFlagController::GetFlag( int iTeam )
{
	for ( auto& flag : m_vFlags )
	{
		if ( !flag.pFlag )
			continue;

		if ( flag.iTeam == iTeam )
			return flag;
	}

	return {};
}

Vector CFlagController::GetPosition( CCaptureFlag* pFlag )
{
	return pFlag->GetAbsOrigin( );
}

std::optional<Vector> CFlagController::GetPosition( int iTeam )
{
	const auto& flag = GetFlag( iTeam );
	if ( flag.pFlag )
		return GetPosition( flag.pFlag );

	return std::nullopt;
}

std::optional<Vector> CFlagController::GetSpawnPosition( int iTeam )
{
	const auto& flag = GetFlag( iTeam );
	if ( flag.pFlag && m_mSpawnPositions.contains( flag.pFlag->entindex() ) )
		return m_mSpawnPositions[ flag.pFlag->entindex() ];

	return std::nullopt;
}

int CFlagController::GetCarrier( CCaptureFlag* pFlag )
{
	if ( !pFlag )
		return -1;

	auto OwnerEnt = pFlag->m_hOwnerEntity( ).Get();
	if ( !OwnerEnt )
		return -1;

	auto carrier = OwnerEnt->As<CTFPlayer>();
	if ( !carrier || carrier->IsDormant() || !carrier->IsPlayer( ) || !carrier->IsAlive( ) )
		return -1;

	return carrier->entindex();
}

int CFlagController::GetCarrier( int iTeam )
{
	const auto& flag = GetFlag( iTeam );

	if ( flag.pFlag )
		return GetCarrier( flag.pFlag );

	SDK::Output( "CFlagController", std::format( "GetCarrier: No flag entity" ).c_str( ), { 255, 131, 131, 255 }, Vars::Debug::Logging.Value );

	return -1;
}

int CFlagController::GetStatus( CCaptureFlag* pFlag )
{
	return pFlag->m_nFlagStatus( );
}

int CFlagController::GetStatus( int iTeam )
{
	const auto& flag = GetFlag( iTeam );
	if ( flag.pFlag )
		return GetStatus( flag.pFlag );

	// Mark as home if nothing is found
	return TF_FLAGINFO_HOME;
}

void CFlagController::Init( )
{
	// Reset everything
	m_vFlags.clear( );
	m_mSpawnPositions.clear( );
}

void CFlagController::Update( )
{
	m_vFlags.clear( );

	// Find flags and get info
	for ( auto pEntity : H::Entities.GetGroup( EGroupType::WORLD_OBJECTIVE ) )
	{
		if ( !pEntity || pEntity->GetClassID() != ETFClassID::CCaptureFlag )
			continue;

		auto pFlag = pEntity->As<CCaptureFlag>( );

		// Cannot use dormant flag, but it is still potentially valid
		if ( pFlag->IsDormant( ) )
			continue;
		
		// Only CTF support for now (and sd_doomsday)
		if ( pFlag->m_nType( ) != TF_FLAGTYPE_CTF && pFlag->m_nType( ) != TF_FLAGTYPE_RESOURCE_CONTROL )
			continue;
		
		FlagInfo info{};
		info.pFlag = pFlag;
		info.iTeam = pFlag->m_iTeamNum( );

		if ( pFlag->m_nFlagStatus( ) == TF_FLAGINFO_HOME )
			m_mSpawnPositions[pFlag->entindex()] = pFlag->GetAbsOrigin( );

		m_vFlags.push_back(info);
	}
}