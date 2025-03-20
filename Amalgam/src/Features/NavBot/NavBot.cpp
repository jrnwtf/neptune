#include "NavBot.h"
#include "NavEngine/Controllers/CPController/CPController.h"
#include "NavEngine/Controllers/FlagController/FlagController.h"
#include "NavEngine/Controllers/PLController/PLController.h"
#include "NavEngine/Controllers/Controller.h"
#include "../Players/PlayerUtils.h"
#include "../Aimbot/AimbotGlobal/AimbotGlobal.h"
#include "../TickHandler/TickHandler.h"
#include "../PacketManip/FakeLag/FakeLag.h"
#include "../Misc/Misc.h"
#include "../Simulation/MovementSimulation/MovementSimulation.h"
#include "../CritHack/CritHack.h"
#include <unordered_set>
#include <random>

bool IsWeaponAimable( CTFWeaponBase* pWeapon )
{
	if ( !pWeapon )
		return false;

	switch ( pWeapon->m_iItemDefinitionIndex( ) ) {
	case Soldier_s_TheBuffBanner:
	case Soldier_s_FestiveBuffBanner:
	case Soldier_s_TheBattalionsBackup:
	case Soldier_s_TheConcheror:
	case Demoman_s_TheTideTurner:
	case Demoman_s_TheCharginTarge:
	case Demoman_s_TheSplendidScreen:
	case Demoman_s_FestiveTarge:
	case Demoman_m_TheBootlegger:
	case Demoman_m_AliBabasWeeBooties:
	case Pyro_s_ThermalThruster: return false;
	default: {
		switch ( pWeapon->GetWeaponID( ) ) {
		case TF_WEAPON_MEDIGUN:
		case TF_WEAPON_PDA:
		case TF_WEAPON_PDA_ENGINEER_BUILD:
		case TF_WEAPON_PDA_ENGINEER_DESTROY:
		case TF_WEAPON_PDA_SPY:
		case TF_WEAPON_PDA_SPY_BUILD:
		case TF_WEAPON_BUILDER:
		case TF_WEAPON_GRAPPLINGHOOK: return false;
		default: return true;
		}
		break;
	}
	}
}

bool CNavBot::ShouldSearchHealth( CTFPlayer* pLocal, bool low_priority )
{
	if ( !( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::SearchHealth ) )
        return false;

    // Priority too high
    if (F::NavEngine.current_priority > health)
        return false;

    // Check if being gradually healed in any way
    if (pLocal->m_nPlayerCond() & (1 << 21)/*TFCond_Healing*/)
        return false;

    float health_percent = static_cast<float>(pLocal->m_iHealth()) / pLocal->GetMaxHealth();
    // Get health when below 65%, or below 80% and just patrolling
    return health_percent < 0.64f || low_priority && (F::NavEngine.current_priority <= patrol || F::NavEngine.current_priority == lowprio_health) && health_percent <= 0.80f;
}

bool CNavBot::ShouldSearchAmmo( CTFPlayer* pLocal )
{
	if ( !( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::SearchAmmo ) )
		return false;

	// Priority too high
	if ( F::NavEngine.current_priority > ammo )
		return false;

	for ( int i = 0; i < 2; i++ )
	{
		const auto& pWeapon = pLocal->GetWeaponFromSlot( i );

		if ( !pWeapon || SDK::WeaponDoesNotUseAmmo( pWeapon ) )
			continue;

		const int WepID = pWeapon->GetWeaponID( );
		if ( ( WepID == TF_WEAPON_SNIPERRIFLE ||
			WepID == TF_WEAPON_SNIPERRIFLE_CLASSIC ||
			WepID == TF_WEAPON_SNIPERRIFLE_DECAP ) &&
			pLocal->GetAmmoCount( pWeapon->m_iPrimaryAmmoType( ) ) <= 5 )
			return true;

		if ( !pWeapon->HasAmmo( ) )
			return true;

		int MaxAmmo = SDK::GetWeaponMaxReserveAmmo( WepID, pWeapon->m_iItemDefinitionIndex( ) );
		if ( !MaxAmmo )
			continue;

		// Reserve ammo
		int ResAmmo = pLocal->GetAmmoCount( pWeapon->m_iPrimaryAmmoType( ) );
		if ( ResAmmo <= MaxAmmo / 3 )
			return true;
	}

	return false;
}

int CNavBot::ShouldTarget( int iPlayerIdx, CTFPlayer* pLocal, CTFWeaponBase* pWeapon )
{
	auto pEntity = I::ClientEntityList->GetClientEntity( iPlayerIdx );
	if ( !pEntity || !pEntity->IsPlayer())
		return -1;

	const auto& pPlayer = pEntity->As<CTFPlayer>();
	if ( !pPlayer->IsAlive() || pPlayer == pLocal )
		return -1;

	if (F::PlayerUtils.IsIgnored(iPlayerIdx))
		return 0;

	if ( Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Friends && H::Entities.IsFriend(iPlayerIdx) 
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Party && H::Entities.InParty(iPlayerIdx) 
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Invulnerable && pPlayer->IsInvulnerable() && G::SavedDefIndexes[SLOT_MELEE] != Heavy_t_TheHolidayPunch 
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Cloaked && pPlayer->IsInvisible() && pPlayer->GetInvisPercentage() >= Vars::Aimbot::General::IgnoreCloakPercentage.Value
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::DeadRinger && pPlayer->m_bFeignDeathReady()
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Taunting && pPlayer->IsTaunting()
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Disguised && pPlayer->InCond(TF_COND_DISGUISED))
		return 0;

	if ( pPlayer->m_iTeamNum( ) == pLocal->m_iTeamNum( ) )
		return 0;

	if ( Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Vaccinator )
	{
		switch ( SDK::GetWeaponType( pWeapon ) )
		{
		case EWeaponType::HITSCAN:
			if ( pPlayer->InCond( TF_COND_MEDIGUN_UBER_BULLET_RESIST ) && SDK::AttribHookValue(0, "mod_pierce_resists_absorbs", pWeapon) != 0)
				return 0;
			break;
		case EWeaponType::PROJECTILE:
			if ( pPlayer->InCond( TF_COND_MEDIGUN_UBER_FIRE_RESIST ) && ( G::SavedWepIds[SLOT_PRIMARY] == TF_WEAPON_FLAMETHROWER || G::SavedWepIds[SLOT_SECONDARY] == TF_WEAPON_FLAREGUN ) )
				return 0;
			else if ( pPlayer->InCond( TF_COND_MEDIGUN_UBER_BULLET_RESIST ) && G::SavedWepIds[SLOT_PRIMARY] == TF_WEAPON_COMPOUND_BOW )
				return 0;
			else if ( pPlayer->InCond( TF_COND_MEDIGUN_UBER_BLAST_RESIST ) )
				return 0;
		}
	}

	return 1;
}

bool CNavBot::ShouldAssist( CTFPlayer* pLocal, int iTargetIdx )
{
	auto pEntity = I::ClientEntityList->GetClientEntity( iTargetIdx );
	if ( !pEntity || pEntity->As<CBaseEntity>()->m_iTeamNum( ) != pLocal->m_iTeamNum( ) )
		return false;

	if ( !( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::HelpFriendlyCaptureObjectives ) )
		return true;

	if ( F::PlayerUtils.IsIgnored( iTargetIdx ) 
		|| H::Entities.InParty( iTargetIdx ) 
		|| H::Entities.IsFriend( iTargetIdx ) )
		return true;

	return false;
}

std::vector<CObjectDispenser*> CNavBot::GetDispensers( CTFPlayer* pLocal )
{
	std::vector<CObjectDispenser*> vDispensers;

	for ( const auto& pEntity : H::Entities.GetGroup( EGroupType::BUILDINGS_TEAMMATES ) )
	{
		if ( !pEntity || pEntity->GetClassID( ) != ETFClassID::CObjectDispenser )
			continue;

		const auto& pDispenser = pEntity->As<CObjectDispenser>( );
		if ( pDispenser->m_bCarryDeploy( ) || pDispenser->m_bHasSapper( ) || pDispenser->m_bBuilding( ) )
			continue;

		// This fixes the fact that players can just place dispensers in unreachable locations
		const auto Origin = F::NavParser.GetDormantOrigin(pDispenser->entindex( ));
		if ( !Origin )
			continue;

		const Vec2 Origin2D = Vec2( Origin->x, Origin->y );
		const auto local_nav = F::NavEngine.findClosestNavSquare( *Origin );
		if ( local_nav->getNearestPoint( Origin2D ).DistToSqr( *Origin ) > pow( 300.0f, 2 ) || local_nav->getNearestPoint( Origin2D ).z - Origin->z > PLAYER_JUMP_HEIGHT )
			continue;

		vDispensers.push_back( pDispenser );
	}


	// Sort by distance, closer is better
	const auto vLocalOrigin = pLocal->GetAbsOrigin( );
	std::sort( vDispensers.begin( ), vDispensers.end( ), [&]( CBaseEntity* a, CBaseEntity* b ) -> bool
		{
			return F::NavParser.GetDormantOrigin(a->entindex( ))->DistToSqr( vLocalOrigin ) < F::NavParser.GetDormantOrigin(b->entindex( ))->DistToSqr( vLocalOrigin );
		} );
	return vDispensers;
}

std::vector<CBaseEntity*> CNavBot::GetEntities( CTFPlayer* pLocal, bool find_health )
{
	std::vector<CBaseEntity*> vEntities;

	const EGroupType groupType = find_health ? EGroupType::PICKUPS_HEALTH : EGroupType::PICKUPS_AMMO;
	for ( const auto& pEntity : H::Entities.GetGroup( groupType ) )
	{
		if ( pEntity && !pEntity->IsDormant( ) )
			vEntities.push_back( pEntity );
	}

	// Sort by distance, closer is better
	const auto vLocalOrigin = pLocal->GetAbsOrigin( );
	std::sort( vEntities.begin( ), vEntities.end( ), [&]( CBaseEntity* a, CBaseEntity* b ) -> bool
		{
			return a->GetAbsOrigin( ).DistToSqr( vLocalOrigin ) < b->GetAbsOrigin( ).DistToSqr( vLocalOrigin );
		} );
	return vEntities;
}

bool CNavBot::GetHealth( CUserCmd* pCmd, CTFPlayer* pLocal, bool low_priority )
{
	const Priority_list priority = low_priority ? lowprio_health : health;
	static Timer health_cooldown{};
	static Timer repath_timer;
	if ( !health_cooldown.Check( 1.f ) )
		return F::NavEngine.current_priority == priority;

	// This should also check if pLocal is valid
	if ( ShouldSearchHealth( pLocal, low_priority ) )
	{
		// Already pathing, only try to repath every 2s
		if ( F::NavEngine.current_priority == priority && !repath_timer.Run( 2.f ))
			return true;

		const auto healthpacks = GetEntities( pLocal, true );
		const auto dispensers = GetDispensers( pLocal );
		auto total_ents = healthpacks;

		// Add dispensers and sort list again
		const auto vLocalOrigin = pLocal->GetAbsOrigin( );
		if ( !dispensers.empty( ) )
		{
			total_ents.reserve( healthpacks.size( ) + dispensers.size( ) );
			total_ents.insert( total_ents.end( ), dispensers.begin( ), dispensers.end( ) );
			std::sort( total_ents.begin( ), total_ents.end( ), [&]( CBaseEntity* a, CBaseEntity* b ) -> bool
				{
					return a->GetAbsOrigin( ).DistToSqr( vLocalOrigin ) < b->GetAbsOrigin( ).DistToSqr( vLocalOrigin );
				} );
		}

		CBaseEntity* best_ent = nullptr;
		if ( !total_ents.empty( ) )
			best_ent = total_ents.front( );

		if ( total_ents.size() > 1 )
		{
			F::NavEngine.navTo( best_ent->GetAbsOrigin( ), priority, true, best_ent->GetAbsOrigin( ).DistToSqr( vLocalOrigin ) > pow( 200.0f, 2 ) );
			auto iFirstTargetPoints = F::NavEngine.crumbs.size( );
			F::NavEngine.cancelPath( );

			F::NavEngine.navTo( total_ents.at( 1 )->GetAbsOrigin( ), priority, true, total_ents.at( 1 )->GetAbsOrigin( ).DistToSqr( vLocalOrigin ) > pow( 200.0f, 2 ) );
			auto iSecondTargetPoints = F::NavEngine.crumbs.size( );
			F::NavEngine.cancelPath( );

			best_ent = iSecondTargetPoints < iFirstTargetPoints ? total_ents.at( 1 ) : best_ent;
		}

		if ( best_ent )
		{
			if ( F::NavEngine.navTo( best_ent->GetAbsOrigin( ), priority, true, best_ent->GetAbsOrigin( ).DistToSqr( vLocalOrigin ) > pow( 200.0f, 2 ) ) )
			{
				// Check if we are close enough to the health pack to pick it up (unless its not a health pack)
				if ( best_ent->GetAbsOrigin( ).DistToSqr( pLocal->GetAbsOrigin( ) ) < pow(75.0f, 2) && !best_ent->IsDispenser())
				{
					// Try to touch the health pack
					auto local_nav = F::NavEngine.findClosestNavSquare( pLocal->GetAbsOrigin( ) );
					if ( local_nav )
					{
						Vector2D to = { best_ent->GetAbsOrigin( ).x, best_ent->GetAbsOrigin( ).y };
						Vector path_point = local_nav->getNearestPoint( to );
						path_point.z = best_ent->GetAbsOrigin( ).z;

						// Walk towards the health pack
						SDK::WalkTo( pCmd, pLocal, path_point );
					}
				}
				return true;
			}
		}

		health_cooldown.Update( );
	}
	else if ( F::NavEngine.current_priority == priority )
		F::NavEngine.cancelPath( );

	return false;
}

bool CNavBot::GetAmmo( CUserCmd* pCmd, CTFPlayer* pLocal, bool force )
{
	static Timer repath_timer;
	static Timer ammo_cooldown{};
	static bool was_force = false;

	if ( !force && !ammo_cooldown.Check( 1.f ) )
		return F::NavEngine.current_priority == ammo;

	if ( force || ShouldSearchAmmo( pLocal ) )
	{
		// Already pathing, only try to repath every 2s
		if ( F::NavEngine.current_priority == ammo && !repath_timer.Run( 2.f ))
			return true;
		else
			was_force = false;

		auto ammopacks = GetEntities( pLocal );
		auto dispensers = GetDispensers( pLocal );
		auto total_ents = ammopacks;

		// Add dispensers and sort list again
		const auto vLocalOrigin = pLocal->GetAbsOrigin( );
		if ( !dispensers.empty( ) )
		{
			total_ents.reserve( ammopacks.size( ) + dispensers.size( ) );
			total_ents.insert( total_ents.end( ), dispensers.begin( ), dispensers.end( ) );
			std::sort( total_ents.begin( ), total_ents.end( ), [&]( CBaseEntity* a, CBaseEntity* b ) -> bool
				{
					return a->GetAbsOrigin( ).DistToSqr( vLocalOrigin ) < b->GetAbsOrigin( ).DistToSqr( vLocalOrigin );
				} );
		}

		CBaseEntity* best_ent = nullptr;
		if ( !total_ents.empty( ) )
			best_ent = total_ents.front( );

		if ( total_ents.size( ) > 1 )
		{
			F::NavEngine.navTo( best_ent->GetAbsOrigin( ), ammo, true, best_ent->GetAbsOrigin( ).DistToSqr( vLocalOrigin ) > pow( 200.0f, 2 ) );
			const auto iFirstTargetPoints = F::NavEngine.crumbs.size( );
			F::NavEngine.cancelPath( );

			F::NavEngine.navTo( total_ents.at( 1 )->GetAbsOrigin( ), ammo, true, total_ents.at( 1 )->GetAbsOrigin( ).DistToSqr( vLocalOrigin ) > pow( 200.0f, 2 ) );
			const auto iSecondTargetPoints = F::NavEngine.crumbs.size( );
			F::NavEngine.cancelPath( );
			best_ent = iSecondTargetPoints < iFirstTargetPoints ? total_ents.at( 1 ) : best_ent;
		}
			
		if ( best_ent )
		{
			if ( F::NavEngine.navTo( best_ent->GetAbsOrigin( ), ammo, true, best_ent->GetAbsOrigin( ).DistToSqr( vLocalOrigin ) > pow( 200.0f, 2 ) ) )
			{
				// Check if we are close enough to the ammo pack to pick it up (unless its not an ammo pack)
				if (best_ent->GetAbsOrigin().DistToSqr(pLocal->GetAbsOrigin()) < pow(75.0f, 2) && !best_ent->IsDispenser())
				{
					// Try to touch the ammo pack
					auto local_nav = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());
					if (local_nav)
					{
						Vector2D to = { best_ent->GetAbsOrigin( ).x, best_ent->GetAbsOrigin( ).y };
						Vector path_point = local_nav->getNearestPoint(to);
						path_point.z = best_ent->GetAbsOrigin().z;
                        
						// Walk towards the ammo pack
						SDK::WalkTo(pCmd, pLocal, path_point);
					}
				}
				was_force = force;
				return true;
			}
		}

		ammo_cooldown.Update( );
	}
	else if ( F::NavEngine.current_priority == ammo && !was_force )
		F::NavEngine.cancelPath( );

	return false;
}

static Timer refresh_sniperspots_timer{};
void CNavBot::RefreshSniperSpots( )
{
	if ( !refresh_sniperspots_timer.Run( 60.f ) )
		return;

	sniper_spots.clear( );

	// Vector of exposed spots to nav to in case we find no sniper spots
	std::vector<Vector> exposed_spots;

	// Search all nav areas for valid sniper spots
	for ( const auto& area : F::NavEngine.getNavFile( )->m_areas )
	{
		// Dont use spawn as a snipe spot
		if ( area.m_TFattributeFlags & ( TF_NAV_SPAWN_ROOM_BLUE | TF_NAV_SPAWN_ROOM_RED | TF_NAV_SPAWN_ROOM_EXIT ) )
			continue;

		for ( const auto& hiding_spot : area.m_hidingSpots )
		{
			// Spots actually marked for sniping
			if ( hiding_spot.IsGoodSniperSpot( ) || hiding_spot.IsIdealSniperSpot( ) || hiding_spot.HasGoodCover( ) )
			{
				sniper_spots.emplace_back( hiding_spot.m_pos );
				continue;
			}

			if ( hiding_spot.IsExposed( ) )
				exposed_spots.emplace_back( hiding_spot.m_pos );
		}
	}

	// If we have no sniper spots, just use nav areas marked as exposed. They're good enough for sniping.
	if ( sniper_spots.empty( ) && !exposed_spots.empty( ) )
		sniper_spots = exposed_spots;
}

std::pair<int, float> CNavBot::GetNearestPlayerDistance( CTFPlayer* pLocal, CTFWeaponBase* pWeapon )
{
	float flMinDist = FLT_MAX;
	int iBest = -1;

	if ( pLocal && pWeapon )
	{
		for ( auto pEntity : H::Entities.GetGroup( EGroupType::PLAYERS_ENEMIES ) )
		{
			if ( !pEntity )
				continue;

			auto pPlayer = pEntity->As<CTFPlayer>( );

			if ( !ShouldTarget( pPlayer->entindex(), pLocal, pWeapon ) )
				continue;

			auto vOrigin = F::NavParser.GetDormantOrigin( pPlayer->entindex( ) );
			if ( !vOrigin )
				continue;

			auto flDist = pLocal->GetAbsOrigin( ).DistToSqr( *vOrigin );

			if ( flDist >= flMinDist )
				continue;

			flMinDist = flDist;
			iBest = pPlayer->entindex();
		}
	}
	return { iBest, flMinDist };
}

bool CNavBot::IsEngieMode( CTFPlayer* pLocal )
{
	return Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::AutoEngie && ( Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value || Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value ) && pLocal && pLocal->IsAlive( ) && pLocal->m_iClass( ) == TF_CLASS_ENGINEER;
}

bool CNavBot::BlacklistedFromBuilding( CNavArea* area )
{
	// FIXME: Better way of doing this ?
	for ( const auto& blacklisted_area : *F::NavEngine.getFreeBlacklist( ) )
	{
		if ( blacklisted_area.first == area && blacklisted_area.second.value == BlacklistReason_enum::BR_BAD_BUILDING_SPOT )
			return true;
	}
	return false;
}

void CNavBot::RefreshBuildingSpots( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, std::pair<int, float> nearest, bool force )
{
	static Timer refresh_buildingspots_timer;
	if ( !IsEngieMode( pLocal ) || !pWeapon )
		return;

	const bool bHasGunslinger = pWeapon->m_iItemDefinitionIndex( ) == Engi_t_TheGunslinger;

	if ( force || refresh_buildingspots_timer.Run( bHasGunslinger ? 1.f : 5.f ) )
	{
		building_spots.clear( );
		std::optional<Vector> target;

		target = F::FlagController.GetSpawnPosition(pLocal->m_iTeamNum( ));
		if ( !target )
		{
			if ( nearest.first )
				target = F::NavParser.GetDormantOrigin( nearest.first );
			if ( !target )
				target = pLocal->GetAbsOrigin( );
		}
		if ( target )
		{
			// Search all nav areas for valid spots
			for ( auto& area : F::NavEngine.getNavFile( )->m_areas )
			{
				if ( BlacklistedFromBuilding( &area ) )
					continue;

				if ( area.m_TFattributeFlags & (TF_NAV_SPAWN_ROOM_RED | TF_NAV_SPAWN_ROOM_BLUE | TF_NAV_SPAWN_ROOM_EXIT ) )
					continue;

				if ( area.m_TFattributeFlags & TF_NAV_SENTRY_SPOT)
				{
					building_spots.emplace_back( area.m_center );
				}
				else
				{
					for ( auto& hiding_spot : area.m_hidingSpots )
						if ( hiding_spot.HasGoodCover( ) )
							building_spots.emplace_back( hiding_spot.m_pos );
				}
			}
			// Sort by distance to nearest, lower is better
			// TODO: This isn't really optimal, need a dif way to where it is a good distance from enemies but also bots dont build in the same spot
			std::sort( building_spots.begin( ), building_spots.end( ),
				[&]( Vector a, Vector b ) -> bool
				{
					if ( bHasGunslinger )
					{
						auto a_dist = a.DistTo( *target );
						auto b_dist = b.DistTo( *target );

						// Penalty for being in danger ranges
						if ( a_dist + 100.0f < 300.0f )
							a_dist += 4000.0f;
						if ( b_dist + 100.0f < 300.0f )
							b_dist += 4000.0f;

						if ( a_dist + 1000.0f < 500.0f )
							a_dist += 1500.0f;
						if ( b_dist + 1000.0f < 500.0f )
							b_dist += 1500.0f;

						return a_dist < b_dist;
					}
					else
						return a.DistTo( *target ) < b.DistTo( *target );
				} );
		}
	}
}

void CNavBot::RefreshLocalBuildings( CTFPlayer* pLocal )
{
	if ( IsEngieMode( pLocal ) )
	{
		mySentryIdx = -1;
		myDispenserIdx = -1;

		for ( auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_TEAMMATES) )
		{
			if ( !pEntity )
				continue;

			auto ClassID = pEntity->GetClassID( );
			if ( ClassID != ETFClassID::CObjectSentrygun && ClassID != ETFClassID::CObjectDispenser )
				continue;

			auto pBuilding = pEntity->As<CBaseObject>( );
			auto pBuilder = pBuilding->m_hBuilder( ).Get( );
			if ( !pBuilder )
				continue;

			if ( pBuilding->m_bPlacing( ) )
				continue;

			if ( pBuilder->entindex() != pLocal->entindex() )
				continue;

			if ( ClassID == ETFClassID::CObjectSentrygun )
				mySentryIdx = pBuilding->entindex();
			else if ( ClassID == ETFClassID::CObjectDispenser )
				myDispenserIdx = pBuilding->entindex();
		}
	}
}

bool CNavBot::NavToSentrySpot( )
{
	static Timer wait_until_path_sentry;

	// Wait a bit before pathing again
	if ( !wait_until_path_sentry.Run( 0.3f ) )
		return false;

	// Try to nav to our existing sentry spot
	if ( auto pSentry = I::ClientEntityList->GetClientEntity(mySentryIdx) )
	{
		// Don't overwrite current nav
		if ( F::NavEngine.current_priority == engineer )
			return true;

		auto vSentryOrigin = F::NavParser.GetDormantOrigin( pSentry->entindex( ) );
		if ( !vSentryOrigin )
			return false;

		if ( F::NavEngine.navTo( *vSentryOrigin, engineer ) )
			return true;
	}
	else
		mySentryIdx = -1;

	if ( building_spots.empty( ) )
		return false;

	// Don't overwrite current nav
	if ( F::NavEngine.current_priority == engineer )
		return false;

	// Max 10 attempts
	for ( int attempts = 0; attempts < 10 && attempts < building_spots.size( ); ++attempts )
	{
		// Get a semi-random building spot to still keep distance preferrance
		auto random_offset = SDK::RandomInt( 0, std::min( 3, ( int )building_spots.size( ) ) );

		Vector random;

		// Wrap around
		if ( attempts - random_offset < 0 )
			random = building_spots[ building_spots.size( ) + ( attempts - random_offset ) ];
		else
			random = building_spots[ attempts - random_offset ];

		// Try to nav there
		if ( F::NavEngine.navTo( random, engineer ) )
		{
			current_building_spot = random;
			return true;
		}
	}
	return false;
}


void CNavBot::UpdateEnemyBlacklist( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int slot )
{
	if ( !pLocal || !pLocal->IsAlive( ) || !pWeapon || !( Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::Players ) )
		return;

	if ( !( Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::DormantThreats ) )
		F::NavEngine.clearFreeBlacklist( BlacklistReason( BR_ENEMY_DORMANT ) );

	if ( !( Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::NormalThreats ) )
	{
		F::NavEngine.clearFreeBlacklist( BlacklistReason( BR_ENEMY_NORMAL ) );
		return;
	}

	static Timer blacklist_update_timer{};
	static Timer dormant_update_timer{};
	static int last_slot_blacklist = primary;
	bool should_run_normal = blacklist_update_timer.Run( 0.5f ) || last_slot_blacklist != slot;
	bool should_run_dormant = Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::DormantThreats && ( dormant_update_timer.Run( 1.f ) || last_slot_blacklist != slot );
	// Don't run since we do not care here
	if ( !should_run_dormant && !should_run_normal )
		return;

	// Clear blacklist for normal entities
	if ( should_run_normal )
		F::NavEngine.clearFreeBlacklist( BlacklistReason( BR_ENEMY_NORMAL ) );

	// Clear blacklist for dormant entities
	if ( should_run_dormant )
		F::NavEngine.clearFreeBlacklist( BlacklistReason( BR_ENEMY_DORMANT ) );

	if ( const auto& pGameRules = I::TFGameRules( ) )
	{
		if ( pGameRules->m_iRoundState( ) == GR_STATE_TEAM_WIN )
			return;
	}

	// #NoFear
	if ( slot == melee )
		return;

	// Store the danger of the individual nav areas
	std::unordered_map<CNavArea*, int> dormant_slight_danger;
	std::unordered_map<CNavArea*, int> normal_slight_danger;

	// This is used to cache Dangerous areas between ents
	std::unordered_map<CTFPlayer*, std::vector<CNavArea*>> ent_marked_dormant_slight_danger;
	std::unordered_map<CTFPlayer*, std::vector<CNavArea*>> ent_marked_normal_slight_danger;

	std::vector<std::pair<CTFPlayer*, Vector>> checked_origins;
	for ( auto pEntity : H::Entities.GetGroup( EGroupType::PLAYERS_ENEMIES ) )
	{
		// Entity is generally invalid, ignore
		if ( !pEntity )
			continue;

		auto pPlayer = pEntity->As<CTFPlayer>( );

		if ( !pPlayer->IsAlive() )
			continue;

		bool is_dormant = pPlayer->IsDormant( );
		// Should not run on dormant and entity is dormant, ignore.
		// Should not run on normal entity and entity is not dormant, ignore
		if ( !should_run_dormant && is_dormant )
			continue;
		else if ( !should_run_normal && !is_dormant )
			continue;

		// Avoid excessive calls by ignoring new checks if people are too close to each other
		auto origin = F::NavParser.GetDormantOrigin( pPlayer->entindex( ) );
		if ( !origin )
			continue;

		origin->z += PLAYER_JUMP_HEIGHT;

		bool should_check = true;

		// Find already dangerous marked areas by other entities
		auto to_loop = is_dormant ? &ent_marked_dormant_slight_danger : &ent_marked_normal_slight_danger;

		// Add new danger entries
		auto to_mark = is_dormant ? &dormant_slight_danger : &normal_slight_danger;

		for ( const auto& checked_origin : checked_origins )
		{
			// If this origin is closer than a quarter of the min HU (or less than 100 HU) to a cached one, don't go through
			// all nav areas again DistToSqr is much faster than DistTo which is why we use it here
			auto distance = selected_config.min_slight_danger;

			distance *= 0.25f;
			distance = std::max( 100.0f, distance );

			// Square the distance
			distance *= distance;

			if ( origin->DistToSqr( checked_origin.second ) < distance )
			{
				should_check = false;

				bool is_absolute_danger = distance < pow(selected_config.min_full_danger, 2);
				if ( !is_absolute_danger && ( false/*slight danger when capping*/|| F::NavEngine.current_priority != capture ) )
				{	
					// The area is not visible by the player
					if ( !F::NavParser.IsVectorVisibleNavigation( *origin, checked_origin.second, MASK_SHOT ) )
						continue;

					for ( auto& area : ( *to_loop )[ checked_origin.first ] )
					{
						( *to_mark )[ area ]++;
						if ( ( *to_mark )[ area ] >= Vars::Misc::Movement::NavBot::BlacklistSlightDangerLimit.Value )
							( *F::NavEngine.getFreeBlacklist( ) )[ area ] = is_dormant ? BlacklistReason_enum::BR_ENEMY_DORMANT : BlacklistReason_enum::BR_ENEMY_NORMAL;
						// pointers scare me..
					}
				}
				break;
			}
		}
		if ( !should_check )
			continue;

		// Now check which areas they are close to
		for ( auto& nav_area : F::NavEngine.getNavFile( )->m_areas )
		{
			float distance = nav_area.m_center.DistToSqr( *origin );
			float slight_danger_dist = selected_config.min_slight_danger;
			float absolute_danger_dist = selected_config.min_full_danger;

			// Not dangerous, Still don't bump
			if ( !ShouldTarget( pPlayer->entindex( ), pLocal, pWeapon ) )
			{
				slight_danger_dist = PLAYER_WIDTH * 1.2f;
				absolute_danger_dist = PLAYER_WIDTH * 1.2f;
			}

			// Too close to count as slight danger
			bool is_absolute_danger = distance < pow(absolute_danger_dist, 2);

			if ( distance < pow(slight_danger_dist, 2) )
			{
				Vector vNavAreaPos = nav_area.m_center;
				vNavAreaPos.z += PLAYER_JUMP_HEIGHT;
				// The area is not visible by the player
				if ( !F::NavParser.IsVectorVisibleNavigation( *origin, vNavAreaPos, MASK_SHOT ) )
					continue;

				// Add as marked area
				( *to_loop )[ pPlayer ].push_back( &nav_area );

				// Just slightly dangerous, only mark as such if it's clear
				if ( !is_absolute_danger && ( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::SafeCapping || F::NavEngine.current_priority != capture ) )
				{
					( *to_mark )[ &nav_area ]++;
					if ( ( *to_mark )[ &nav_area ] < Vars::Misc::Movement::NavBot::BlacklistSlightDangerLimit.Value )
						continue;
				}
				( *F::NavEngine.getFreeBlacklist( ) )[ &nav_area ] = is_dormant ? BlacklistReason_enum::BR_ENEMY_DORMANT : BlacklistReason_enum::BR_ENEMY_NORMAL;
			}
		}
		checked_origins.emplace_back( pPlayer, *origin );
	}
}

bool CNavBot::IsAreaValidForStayNear( Vector ent_origin, CNavArea* area, bool fix_local_z )
{
	if ( fix_local_z )
		ent_origin.z += PLAYER_JUMP_HEIGHT;
	auto area_origin = area->m_center;
	area_origin.z += PLAYER_JUMP_HEIGHT;

	// Do all the distance checks
	float distance = ent_origin.DistToSqr( area_origin );

	// Too close
	if ( distance < pow( selected_config.min_full_danger, 2 ) )
		return false;

	// Blacklisted
	if ( F::NavEngine.getFreeBlacklist( )->find( area ) != F::NavEngine.getFreeBlacklist( )->end( ) )
		return false;

	// Too far away
	if ( distance > pow( selected_config.max, 2 ) )
		return false;

	CGameTrace trace = {};
	CTraceFilterWorldAndPropsOnly filter = {}; 
	
	// Attempt to vischeck
	SDK::Trace( ent_origin, area_origin, MASK_SHOT | CONTENTS_GRATE, &filter, &trace );
	return trace.fraction == 1.f;
}

bool CNavBot::StayNearTarget( int entindex )
{
	auto pEntity = I::ClientEntityList->GetClientEntity( entindex );
	if ( !pEntity )
		return false;
	
	auto vOrigin = F::NavParser.GetDormantOrigin( entindex );
	// No origin recorded, don't bother
	if ( !vOrigin )
		return false;

	// Add the vischeck height
	vOrigin->z += PLAYER_JUMP_HEIGHT;

	// Use std::pair to avoid using the distance functions more than once
	std::vector<std::pair<CNavArea*, float>> good_areas{};

	for ( auto& area : F::NavEngine.getNavFile( )->m_areas )
	{
		auto area_origin = area.m_center;

		// Is this area valid for stay near purposes?
		if ( !IsAreaValidForStayNear( *vOrigin, &area, false ) )
			continue;

		float distance = ( *vOrigin ).DistToSqr( area_origin );
		// Good area found
		good_areas.emplace_back( &area, distance );
	}
	// Sort based on distance
	if ( selected_config.prefer_far )
		std::sort( good_areas.begin( ), good_areas.end( ), []( std::pair<CNavArea*, float> a, std::pair<CNavArea*, float> b ) { return a.second > b.second; } );
	else
		std::sort( good_areas.begin( ), good_areas.end( ), []( std::pair<CNavArea*, float> a, std::pair<CNavArea*, float> b ) { return a.second < b.second; } );

	// Try to path to all the good areas, based on distance
	if ( std::ranges::any_of( good_areas, [&]( std::pair<CNavArea*, float> area ) -> bool
		{
			return F::NavEngine.navTo( area.first->m_center, staynear, true, !F::NavEngine.isPathing( ) );
		} )
		)
	{
		G::NavbotTargetIdx = pEntity->entindex( );
		const auto& pPlayerResource = H::Entities.GetPR( );
		if ( pPlayerResource )
			FollowTargetName = SDK::ConvertUtf8ToWide( pPlayerResource->GetPlayerName( pEntity->entindex( ) ) );
		return true;
	}

	return false;
}

int CNavBot::IsStayNearTargetValid( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int entindex )
{
	int iShouldTarget = ShouldTarget( entindex, pLocal, pWeapon );
	int iResult = entindex ? iShouldTarget == -1 ? -1 : iShouldTarget == 0 ? 0 : 1 : 0;
	return iResult;
}

std::optional<std::pair<CNavArea*, int>> CNavBot::FindClosestHidingSpot( CNavArea* area, std::optional<Vector> vischeck_point, int recursion_count, int index )
{
	static std::vector<CNavArea*> already_recursed;
	if ( index == 0 )
		already_recursed.clear( );
	Vector area_origin = area->m_center;
	area_origin.z += PLAYER_JUMP_HEIGHT;

	// Increment recursion index
	index++;

	// If the area works, return it
	if ( vischeck_point && !F::NavParser.IsVectorVisibleNavigation( area_origin, *vischeck_point ) )
		return std::pair<CNavArea*, int>{ area, index - 1 };
	// Termination condition not hit yet
	else if ( index < recursion_count )
	{
		// Store the nearest area
		std::optional<std::pair<CNavArea*, int>> best_area = std::nullopt;

		for ( auto& connection : area->m_connections )
		{
			if ( std::find( already_recursed.begin( ), already_recursed.end( ), connection.area ) != already_recursed.end( ) )
				continue;
			already_recursed.push_back( connection.area );
			auto area = FindClosestHidingSpot( connection.area, vischeck_point, recursion_count, index );
			if ( area && ( !best_area || area->second < best_area->second ) )
				best_area = { area->first, area->second };
		}
		return best_area;
	}
	return std::nullopt;
}

bool CNavBot::RunReload( CTFPlayer* pLocal, CTFWeaponBase* pWeapon )
{
	static Timer reloadrun_cooldown{};

	// Not reloading, do not run
	if ( !G::Reloading && pWeapon->m_iClip1( ) )
		return false;

	if ( !( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::StalkEnemies ) )
		return false;

	// Too high priority, so don't try
	if ( F::NavEngine.current_priority > run_reload )
		return false;

	// Re-calc only every once in a while
	if ( !reloadrun_cooldown.Run( 1.f ) )
		return F::NavEngine.current_priority == run_reload;

	// Get our area and start recursing the neighbours
	auto local_area = F::NavEngine.findClosestNavSquare( pLocal->GetAbsOrigin( ) );
	if ( !local_area )
		return false;

	// Get closest enemy to vicheck
	CBaseEntity* closest_visible_enemy = nullptr;
	float best_distance = FLT_MAX;
	for ( auto pEntity : H::Entities.GetGroup( EGroupType::PLAYERS_ENEMIES ) )
	{
		if ( !pEntity )
			continue;

		if ( !ShouldTarget( pEntity->entindex( ), pLocal, pWeapon ) )
			continue;

		float flDist = pEntity->GetAbsOrigin( ).DistToSqr( pLocal->GetAbsOrigin( ) );
		if ( flDist > pow( best_distance, 2 ) )
			continue;

		best_distance = flDist;
		closest_visible_enemy = pEntity;
	}

	if ( !closest_visible_enemy )
		return false;

	Vector vischeck_point = closest_visible_enemy->GetAbsOrigin( );
	vischeck_point.z += PLAYER_JUMP_HEIGHT;

	// Get the best non visible area
	auto best_area = FindClosestHidingSpot( local_area, vischeck_point, 5 );
	if ( !best_area )
		return false;

	// If we can, path
	if ( F::NavEngine.navTo( ( *best_area ).first->m_center, run_reload, true, !F::NavEngine.isPathing() ) )
		return true;
	return false;
}

slots CNavBot::GetReloadWeaponSlot( CTFPlayer* pLocal, std::pair<int, float> nearest )
{
	if ( !( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::ReloadWeapons ) )
		return slots( );

	// Priority too high
	if ( F::NavEngine.current_priority > capture )
		return slots( );

	// Dont try to reload in combat
	if ( F::NavEngine.current_priority == staynear && nearest.second <= pow( 500, 2 )
		|| nearest.second <= pow( 200, 2 ) )
		return slots( );

	auto pPrimaryWeapon = pLocal->GetWeaponFromSlot( SLOT_PRIMARY );
	auto pSecondaryWeapon = pLocal->GetWeaponFromSlot( SLOT_SECONDARY );
	bool bCheckPrimary = !SDK::WeaponDoesNotUseAmmo(G::SavedWepIds[SLOT_PRIMARY], G::SavedDefIndexes[SLOT_PRIMARY], false);
	bool bCheckSecondary = !SDK::WeaponDoesNotUseAmmo(G::SavedWepIds[SLOT_SECONDARY], G::SavedDefIndexes[SLOT_SECONDARY], false);

	float flDivider = F::NavEngine.current_priority < staynear && nearest.second > pow( 500, 2 ) ? 1.f : 3.f;

	CTFWeaponInfo* pWeaponInfo = nullptr;
	bool bWeaponCantReload = false;
	if ( bCheckPrimary && pPrimaryWeapon )
	{
		pWeaponInfo = pPrimaryWeapon->GetWeaponInfo( );
		bWeaponCantReload = ( !pWeaponInfo || pWeaponInfo->iMaxClip1 < 0 || !pLocal->GetAmmoCount( pPrimaryWeapon->m_iPrimaryAmmoType( ) ) ) && G::SavedWepIds[ SLOT_PRIMARY ] != TF_WEAPON_PARTICLE_CANNON && G::SavedWepIds[ SLOT_PRIMARY ] != TF_WEAPON_DRG_POMSON;
		if ( pWeaponInfo && !bWeaponCantReload && G::AmmoInSlot[ SLOT_PRIMARY ] < ( pWeaponInfo->iMaxClip1 / flDivider ) )
			return primary;
	}

	bool bFoundPrimaryWepInfo = pWeaponInfo;
	if ( bCheckSecondary && pSecondaryWeapon && ( bFoundPrimaryWepInfo || !bCheckPrimary ) )
	{
		pWeaponInfo = pSecondaryWeapon->GetWeaponInfo( );
		bWeaponCantReload = ( !pWeaponInfo || pWeaponInfo->iMaxClip1 < 0 || !pLocal->GetAmmoCount( pSecondaryWeapon->m_iPrimaryAmmoType( ) ) ) && G::SavedWepIds[ SLOT_SECONDARY ] != TF_WEAPON_RAYGUN;
		if ( pWeaponInfo && !bWeaponCantReload && G::AmmoInSlot[ SLOT_SECONDARY ] < ( pWeaponInfo->iMaxClip1 / flDivider ) )
			return secondary;
	}

	// We succeeded in finding a reload slot previously
	// return our current slot to avoid getting stuck switching between our best weapon slot and reload weapon slot
	/*if ( m_eLastReloadSlot && ( ( bCheckPrimary && ( !pPrimaryWeapon || !bFoundPrimaryWepInfo ) )
		|| ( bCheckSecondary && ( !pSecondaryWeapon || !pWeaponInfo ) ) ) )
	{
		return m_eLastReloadSlot;
	}*/

	return slots( );
}

bool CNavBot::RunSafeReload( CTFPlayer* pLocal, CTFWeaponBase* pWeapon )
{
	static Timer reloadrun_cooldown{};

	if ( !( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::ReloadWeapons ) || !m_eLastReloadSlot && !G::Reloading)
	{
		if ( F::NavEngine.current_priority == run_safe_reload )
			F::NavEngine.cancelPath( );
		return false;
	}

	// Re-calc only every once in a while
	if ( !reloadrun_cooldown.Run( 1.f ) )
		return F::NavEngine.current_priority == run_safe_reload;

	// Get our area and start recursing the neighbours
	auto local_area = F::NavEngine.findClosestNavSquare( pLocal->GetAbsOrigin( ) );
	if ( !local_area )
		return false;

	// If pathing try to avoid going to our current destination until we fully reload
	std::optional<Vector> vCurrentDestination;
	auto crumbs = F::NavEngine.getCrumbs();
	if ( F::NavEngine.current_priority != run_safe_reload && crumbs->size( ) > 4 )
		vCurrentDestination = crumbs->at(4).vec;

	if( vCurrentDestination )
		vCurrentDestination->z += PLAYER_JUMP_HEIGHT;
	else
	{
		// Get closest enemy to vicheck
		CBaseEntity* closest_visible_enemy = nullptr;
		float best_distance = FLT_MAX;
		for ( auto pEntity : H::Entities.GetGroup( EGroupType::PLAYERS_ENEMIES ) )
		{
			if ( !pEntity || pEntity->IsDormant( ) )
				continue;

			if ( !ShouldTarget( pEntity->entindex( ), pLocal, pWeapon ) )
				continue;

			float flDist = pEntity->GetAbsOrigin( ).DistToSqr( pLocal->GetAbsOrigin( ) );
			if ( flDist > pow( best_distance, 2 ) )
				continue;

			best_distance = flDist;
			closest_visible_enemy = pEntity;
		}

		if ( closest_visible_enemy )
		{
			*vCurrentDestination = closest_visible_enemy->GetAbsOrigin( );
			vCurrentDestination->z += PLAYER_JUMP_HEIGHT;
		}
	}
	// Get the best non visible area
	auto best_area = FindClosestHidingSpot( local_area, vCurrentDestination, 5 );
	if ( best_area )
	{
		// If we can, path
		if ( F::NavEngine.navTo( ( *best_area ).first->m_center, run_safe_reload, true, !F::NavEngine.isPathing() ) )
			return true;
	}

	return false;
}


bool CNavBot::StayNear( CTFPlayer* pLocal, CTFWeaponBase* pWeapon )
{
	static Timer staynear_cooldown{};
	static Timer invalidTargetTimer{};
	static int iStayNearTargetIdx = -1;

	// Stay near is expensive so we have to cache. We achieve this by only checking a pre-determined amount of players every
	// CreateMove
	constexpr int MAX_STAYNEAR_CHECKS_RANGE = 3;
	constexpr int MAX_STAYNEAR_CHECKS_CLOSE = 2;

	// Stay near is off
	if ( !( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::StalkEnemies ) )
	{
		iStayNearTargetIdx = -1;
		return false;
	}

	// Don't constantly path, it's slow.
	// Far range classes do not need to repath nearly as often as close range ones.
	if ( !staynear_cooldown.Run( selected_config.prefer_far ? 2.f : 0.5f ) )
		return F::NavEngine.current_priority == staynear;

	// Too high priority, so don't try
	if ( F::NavEngine.current_priority > staynear )
	{
		iStayNearTargetIdx = -1;
		return false;
	}

	int iPreviousTargetValid = IsStayNearTargetValid( pLocal, pWeapon, iStayNearTargetIdx );
	// Check and use our previous target if available
	if ( iPreviousTargetValid )
	{
		invalidTargetTimer.Update( );

		// Check if target is RAGE status - if so, always keep targeting them
		int iPriority = H::Entities.GetPriority( iStayNearTargetIdx );
		if ( iPriority > F::PlayerUtils.m_vTags[ F::PlayerUtils.TagToIndex( DEFAULT_TAG ) ].Priority )
		{
			 if (StayNearTarget(iStayNearTargetIdx))
                return true;
		}

		auto ent_origin = F::NavParser.GetDormantOrigin( iStayNearTargetIdx );
		if ( ent_origin )
		{
			// Check if current target area is valid
			if ( F::NavEngine.isPathing( ) )
			{
				auto crumbs = F::NavEngine.getCrumbs( );
				// We cannot just use the last crumb, as it is always nullptr
				if ( crumbs->size( ) > 2 )
				{
					auto last_crumb = ( *crumbs )[ crumbs->size( ) - 2 ];
					// Area is still valid, stay on it
					if ( IsAreaValidForStayNear( *ent_origin, last_crumb.navarea ) )
						return true;
				}
			}
			// Else Check our origin for validity (Only for ranged classes)
			else if ( selected_config.prefer_far && IsAreaValidForStayNear( *ent_origin, F::NavEngine.findClosestNavSquare( pLocal->GetAbsOrigin( ) ) ) )
				return true;
		}
		// Else we try to path again
		if ( StayNearTarget( iStayNearTargetIdx ) )
			return true;

	}
	// Our previous target wasn't properly checked, try again unless
	else if( iPreviousTargetValid == -1 && !invalidTargetTimer.Check(0.3f))
		return F::NavEngine.current_priority == staynear;

	// Failed, invalidate previous target and try others
	iStayNearTargetIdx = -1;
	invalidTargetTimer.Update( );

	// Cancel path so that we dont follow old target
	if ( F::NavEngine.current_priority == staynear )
		F::NavEngine.cancelPath( );

	std::vector<std::pair<int,int>> vPriorityPlayers{};
	std::unordered_set<int> sHasPriority{};
	for ( const auto& pEntity : H::Entities.GetGroup( EGroupType::PLAYERS_ENEMIES ) )
	{
		int iPriority = H::Entities.GetPriority( pEntity->entindex( ) );
		if ( iPriority > F::PlayerUtils.m_vTags[ F::PlayerUtils.TagToIndex( DEFAULT_TAG ) ].Priority )
		{
			vPriorityPlayers.push_back( {pEntity->entindex( ), iPriority} );
			sHasPriority.insert( pEntity->entindex( ) );
		}
	}
	std::sort( vPriorityPlayers.begin( ), vPriorityPlayers.end( ), []( std::pair<int, int> a, std::pair<int, int> b ) { return a.second > b.second; } );
	
	// First check for RAGE players - they get highest priority
	for ( auto [playeridx, _] : vPriorityPlayers )
	{
		if (!IsStayNearTargetValid(pLocal, pWeapon, playeridx))
			continue;

		if (StayNearTarget(playeridx))
		{
			iStayNearTargetIdx = playeridx;
			return true;
		}
	}

	// Then check other players
	int calls = 0;
	auto advance_count = selected_config.prefer_far ? MAX_STAYNEAR_CHECKS_RANGE : MAX_STAYNEAR_CHECKS_CLOSE;
	std::vector<std::pair<int, float>> vSortedPlayers{};
	for ( auto pEntity : H::Entities.GetGroup( EGroupType::PLAYERS_ENEMIES ) )
	{
		if ( calls >= advance_count )
			break;
		calls++;

		// Skip RAGE players as we already checked them
		if ( !pEntity || sHasPriority.contains(pEntity->entindex()) )
			continue;

		auto playeridx = pEntity->entindex();
		if ( !IsStayNearTargetValid( pLocal, pWeapon, playeridx ) )
		{
			calls--;
			continue;
		}

		auto vOrigin = F::NavParser.GetDormantOrigin( playeridx );
		if ( !vOrigin )
			continue;

		vSortedPlayers.push_back( { playeridx, vOrigin->DistToSqr( pLocal->GetAbsOrigin( ) ) } );
	}
	if ( !vSortedPlayers.empty() )
	{
		std::sort( vSortedPlayers.begin( ), vSortedPlayers.end( ), []( std::pair<int, float> a, std::pair<int, float> b ) { return a.second < b.second; } );

		for ( auto [idx, _] : vSortedPlayers )
		{
			// Succeeded pathing
			if ( StayNearTarget( idx ) )
			{
				iStayNearTargetIdx = idx;
				return true;
			}
		}
	}

	// Stay near failed to find any good targets, add extra delay
	staynear_cooldown += 3.f;
	return false;
}

bool CNavBot::MeleeAttack( CUserCmd* pCmd, CTFPlayer* pLocal, int slot, std::pair<int, float> nearest )
{	
	static bool bIsVisible = false;

	auto pEntity = I::ClientEntityList->GetClientEntity( nearest.first );
	if ( !pEntity || pEntity->IsDormant( ) )
		return F::NavEngine.current_priority == prio_melee;

	if ( slot != melee || m_eLastReloadSlot )
	{
		if ( F::NavEngine.current_priority == prio_melee )
			F::NavEngine.cancelPath( );
		return false;
	}

	auto pPlayer = pEntity->As<CTFPlayer>( );
	if ( pPlayer->IsInvulnerable( ) && G::SavedDefIndexes[ SLOT_MELEE ] != Heavy_t_TheHolidayPunch )
		return false;

	// Too high priority, so don't try
	if ( F::NavEngine.current_priority > prio_melee )
		return false;

	static Timer melee_cooldown{};
	{
		trace_t trace;
		CTraceFilterWorldAndPropsOnly filter{};

		const auto hb = pPlayer->GetHitboxCenter( HITBOX_THORAX );
		if ( !hb.IsZero( ) )
		{
			SDK::TraceHull( pLocal->GetShootPos( ), hb, pLocal->m_vecMins( ), pLocal->m_vecMaxs( ), MASK_PLAYERSOLID, &filter, &trace );
			bIsVisible = trace.fraction == 1.f;
		}
		else
			bIsVisible = false;
	}

	Vector vTargetOrigin = pPlayer->GetAbsOrigin( );

	// If we are close enough, don't even bother with using the navparser to get there
	if ( nearest.second < pow(100.0f, 2) && bIsVisible )
	{	
		Vector vLocalOrigin = pLocal->GetAbsOrigin( );

		// Should fix getting stuck at trying to walk to players in unreachable areas
		if ( pow( vTargetOrigin.z, 2 ) < pow( vLocalOrigin.z - PLAYER_JUMP_HEIGHT, 2 ) || pow( vTargetOrigin.z, 2 ) > pow( vLocalOrigin.z + PLAYER_JUMP_HEIGHT, 2 ) )
			return false;

		SDK::WalkTo( pCmd, pLocal, vTargetOrigin );
		F::NavEngine.cancelPath( );
		F::NavEngine.current_priority = prio_melee;
		return true;
	}

	// Don't constantly path, it's slow.
	// The closer we are, the more we should try to path
	if ( !melee_cooldown.Run( nearest.second < pow(100.0f, 2) ? 0.2f : nearest.second < pow(1000.0f, 2) ? 0.5f : 2.f ) && F::NavEngine.isPathing( ) )
		return F::NavEngine.current_priority == prio_melee;

	// Just walk at the enemy l0l
	if ( F::NavEngine.navTo( vTargetOrigin, prio_melee, true, !F::NavEngine.isPathing( ) ) )
		return true;
	return false;
}

bool CNavBot::IsAreaValidForSnipe( Vector ent_origin, Vector area_origin, bool fix_sentry_z )
{
	if (fix_sentry_z)
		ent_origin.z += 40.0f;
	area_origin.z += PLAYER_JUMP_HEIGHT;

	float distance = ent_origin.DistToSqr(area_origin);
	// Too close to be valid
	if (distance <= pow(1100.0f + HALF_PLAYER_WIDTH, 2))
		return false;
	// Fails vischeck, bad
	if (!F::NavParser.IsVectorVisibleNavigation(area_origin, ent_origin))
		return false;
	return true;
}

bool CNavBot::TryToSnipe( CBaseObject* pBulding )
{
	auto vOrigin = F::NavParser.GetDormantOrigin(pBulding->entindex());
	if ( !vOrigin )
		return false;
	// Add some z to dormant sentries as it only returns origin
	//if (ent->IsDormant())
		vOrigin->z += 40.0f;

	std::vector<std::pair<CNavArea *, float>> good_areas;
	for (auto &area : F::NavEngine.getNavFile()->m_areas)
	{
		// Not usable
		if (!IsAreaValidForSnipe(*vOrigin, area.m_center, false))
			continue;
		good_areas.push_back(std::pair<CNavArea *, float>(&area, area.m_center.DistToSqr(*vOrigin)));
	}

	// Sort based on distance
	if (selected_config.prefer_far)
		std::sort(good_areas.begin(), good_areas.end(), [](std::pair<CNavArea *, float> a, std::pair<CNavArea *, float> b) { return a.second > b.second; });
	else
		std::sort(good_areas.begin(), good_areas.end(), [](std::pair<CNavArea *, float> a, std::pair<CNavArea *, float> b) { return a.second < b.second; });

	if (std::ranges::any_of(good_areas, [](std::pair<CNavArea *, float> area) { return F::NavEngine.navTo(area.first->m_center, snipe_sentry); }))
		return true;
	return false;
}

bool CNavBot::IsSnipeTargetValid( int iBuildingIdx )
{
	if ( !( Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Sentry ) )
		return false;

	auto pEntity = I::ClientEntityList->GetClientEntity( iBuildingIdx );
	if ( !pEntity || !pEntity->IsSentrygun())
		return false;

	auto pOwner = pEntity->As<CBaseObject>()->m_hBuilder().Get();
	if (pOwner && F::PlayerUtils.IsIgnored(pOwner->entindex()))
		return false;

	return true;
}

bool CNavBot::SnipeSentries( CTFPlayer* pLocal )
{
	static Timer sentry_snipe_cooldown;
	static int previous_target = -1;

	if (!(Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::TargetSentries))
		return false;

	// Sentries don't move often, so we can use a slightly longer timer
	if (!sentry_snipe_cooldown.Run(2.f))
		return F::NavEngine.current_priority == snipe_sentry || IsSnipeTargetValid(previous_target);

	if (IsSnipeTargetValid(previous_target))
	{
		auto crumbs = F::NavEngine.getCrumbs();
		auto pPrevTarget = I::ClientEntityList->GetClientEntity( previous_target )->As<CBaseObject>();
		// We cannot just use the last crumb, as it is always nullptr
		if (crumbs->size() > 2)
		{
			auto last_crumb = (*crumbs)[crumbs->size() - 2];
			// Area is still valid, stay on it
			if (IsAreaValidForSnipe(pPrevTarget->GetAbsOrigin(), last_crumb.navarea->m_center ) )
				return true;
		}
		if (TryToSnipe(pPrevTarget))
			return true;
	}

	// Make sure we don't try to do it on shortrange classes unless specified
	if (!(Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::TargetSentriesLowRange)
		&& (pLocal->m_iClass() == TF_CLASS_SCOUT || pLocal->m_iClass() == TF_CLASS_PYRO ) )
		return false;

	for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_ENEMIES))
	{
		// Invalid sentry
		if (IsSnipeTargetValid(pEntity->entindex()))
			continue;

		// Succeeded in trying to snipe it
		if (TryToSnipe(pEntity->As<CBaseObject>()))
		{
			previous_target = pEntity->entindex();
			return true;
		}
	}
	previous_target = -1;
	return false;
}

static CBaseObject* GetCarriedBuilding( CTFPlayer* pLocal )
{
	if ( auto pEntity = pLocal->m_hCarriedObject( ).Get() )
		return pEntity->As<CBaseObject>( );
	return nullptr;
}

bool CNavBot::BuildBuilding( CUserCmd* pCmd, CTFPlayer* pLocal, std::pair<int, float> nearest, int building )
{
	CTFWeaponBase* pMeleeWeapon = pLocal->GetWeaponFromSlot( SLOT_MELEE );
	if ( !pMeleeWeapon )
		return false;

	// Blacklist this spot and refresh the building spots
	if (build_attempts >= 15)
	{
		(*F::NavEngine.getFreeBlacklist())[F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin())] = BlacklistReason_enum::BR_BAD_BUILDING_SPOT;
		RefreshBuildingSpots(pLocal, pMeleeWeapon, nearest, true);
		current_building_spot = std::nullopt;
		build_attempts = 0;
		return false;
	}

	// Make sure we have right amount of ammo
	int required = (pMeleeWeapon->m_iItemDefinitionIndex() == Engi_t_TheGunslinger || building == dispenser ) ? 100 : 130;
	if (pLocal->m_iMetalCount() < required)
		return GetAmmo(pCmd, pLocal, true);

	EngineerTask = std::format( L"Build {}", building == dispenser ? L"dispenser" : L"sentry" );
	static float flPrevYaw = 0.0f;
	// Try to build! we are close enough
	if (current_building_spot && current_building_spot->DistToSqr(pLocal->GetAbsOrigin()) <= pow(building == dispenser ? 500.0f : 200.0f, 2))
	{
		// TODO: Rotate our angle to a valid building spot ? also rotate building itself to face enemies ?
		pCmd->viewangles.x = 20.0f;
		pCmd->viewangles.y = flPrevYaw += 2.0f;

		// Gives us 4 1/2 seconds to build
		static Timer attempt_timer;
		if (attempt_timer.Run(0.3f))
			build_attempts++;

		//auto pCarriedBuilding = GetCarriedBuilding( pLocal );
		if (!pLocal->m_bCarryingObject())
		{
			static Timer command_timer;
			if (command_timer.Run(0.1f))
				I::EngineClient->ClientCmd_Unrestricted( std::format( "build {}", building ).c_str( ) );
		}
		//else if (pCarriedBuilding->m_bServerOverridePlacement()) // Can place
		pCmd->buttons |= IN_ATTACK;
		return true;
	}
	else
	{
		flPrevYaw = 0.0f;
		return NavToSentrySpot( );
	}

	return false;
}

bool CNavBot::BuildingNeedsToBeSmacked( CBaseObject* pBuilding )
{
	if (!pBuilding || pBuilding->IsDormant())
		return false;

	if (pBuilding->m_iUpgradeLevel() != 3 || pBuilding->m_iHealth() / pBuilding->m_iMaxHealth() <= 0.80f)
		return true;

	if (pBuilding->GetClassID() == ETFClassID::CObjectSentrygun)
	{
		int max_ammo = 0;
		switch (pBuilding->m_iUpgradeLevel( ) )
		{
		case 1:
			max_ammo = 150;
			break;
		case 2:
		case 3:
			max_ammo = 200;
			break;
		}

		return pBuilding->As<CObjectSentrygun>()->m_iAmmoShells() / max_ammo <= 0.50f;
	}
	return false;
}

bool CNavBot::SmackBuilding( CUserCmd* pCmd, CTFPlayer* pLocal, CBaseObject* pBuilding )
{
	if ( !pBuilding || pBuilding->IsDormant( ) )
		return false;

	if ( !pLocal->m_iMetalCount( ) )
		return GetAmmo( pCmd, pLocal, true );

	CTFWeaponBase* pWeapon = H::Entities.GetWeapon( );
	if ( !pWeapon )
		return false;

	EngineerTask = std::format( L"Smack {}", pBuilding->GetClassID() == ETFClassID::CObjectDispenser ? L"dispenser" : L"sentry" );

	if ( pBuilding->GetAbsOrigin( ).DistToSqr( pLocal->GetAbsOrigin( ) ) <= pow(100.0f, 2) && pWeapon->GetSlot( ) == SLOT_MELEE )
	{
		pCmd->buttons |= IN_ATTACK;

		auto vAngTo = Math::CalcAngle( pLocal->GetEyePosition( ), pBuilding->GetCenter( ) );
		G::Attacking = SDK::IsAttacking( pLocal, pWeapon, pCmd, true );
		if ( G::Attacking == 1 )
			pCmd->viewangles = vAngTo;
	}
	else if ( F::NavEngine.current_priority != engineer )
		return F::NavEngine.navTo( pBuilding->GetAbsOrigin(), engineer );

	return true;
}

bool CNavBot::EngineerLogic( CUserCmd* pCmd, CTFPlayer* pLocal, std::pair<int, float> nearest )
{
	if (!IsEngieMode(pLocal))
	{
		EngineerTask.clear( );
		return false;
	}

	CTFWeaponBase* pMeleeWeapon = pLocal->GetWeaponFromSlot( SLOT_MELEE );
	if ( !pMeleeWeapon )
		return false;

	auto pSentry = I::ClientEntityList->GetClientEntity( mySentryIdx );
	// Already have a sentry
	if (pSentry && pSentry->GetClassID() == ETFClassID::CObjectSentrygun)
	{
		if (pMeleeWeapon->m_iItemDefinitionIndex() == Engi_t_TheGunslinger)
		{
			auto vSentryOrigin = F::NavParser.GetDormantOrigin( mySentryIdx );

			// Too far away, destroy it
			// BUG Ahead, building isnt destroyed lol
			if (!vSentryOrigin || vSentryOrigin->DistToSqr( pLocal->GetAbsOrigin() ) >= pow(1800.0f,2) )
			{
				// If we have a valid building
				I::EngineClient->ClientCmd_Unrestricted("destroy 2");
			}
			// Return false so we run another task
			return false;
		}
		else
		{
			// Try to smack sentry first
			if (BuildingNeedsToBeSmacked(pSentry->As<CBaseObject>()))
				return SmackBuilding(pCmd, pLocal, pSentry->As<CBaseObject>());
			else
			{
				auto pDispenser = I::ClientEntityList->GetClientEntity( myDispenserIdx );
				// We put dispenser by sentry
				if (!pDispenser)
					return BuildBuilding(pCmd, pLocal, nearest, dispenser);
				else
				{
					// We already have a dispenser, see if it needs to be smacked
					if (BuildingNeedsToBeSmacked(pDispenser->As<CBaseObject>()))
						return SmackBuilding(pCmd, pLocal, pDispenser->As<CBaseObject>());
					
					// Return false so we run another task
					return false;
				}
			}

		}
	}
	// Try to build a sentry
	return BuildBuilding( pCmd, pLocal, nearest, sentry );
}

std::optional<Vector> CNavBot::GetCtfGoal( CTFPlayer* pLocal, int our_team, int enemy_team )
{
	// Get Flag related information
	auto status = F::FlagController.GetStatus( enemy_team );
	auto position = F::FlagController.GetPosition( enemy_team );
	auto carrier = F::FlagController.GetCarrier( enemy_team );

	// CTF is the current capture type
	if ( status == TF_FLAGINFO_STOLEN )
	{
		if ( carrier == pLocal->entindex( ) )
		{
			// Return our capture point location
			return F::FlagController.GetSpawnPosition(our_team);
		}
		// Assist with capturing
		else if ( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::HelpCaptureObjectives )
		{
			if ( ShouldAssist( pLocal, carrier ) )
			{
				// Stay slightly behind and to the side to avoid blocking
				if ( position )
				{
					Vector offset( 40.0f, 40.0f, 0.0f );
					*position -= offset;
				}
				return position;
			}
		}
		return std::nullopt;
	}

	// Get the flag if not taken by us already
	return position;
}

std::optional<Vector> CNavBot::GetPayloadGoal( const Vector vLocalOrigin, int our_team )
{
	auto position = F::PLController.GetClosestPayload( vLocalOrigin, our_team );
	if ( !position )
		return std::nullopt;

	// Get number of teammates near cart to coordinate positioning
    int teammates_near_cart = 0;
    float cart_radius = 150.0f; // Approx cart capture radius

	for ( auto pEntity : H::Entities.GetGroup( EGroupType::PLAYERS_TEAMMATES ) )
	{
		if ( !pEntity || pEntity->entindex() == I::EngineClient->GetLocalPlayer( ) )
			continue;

		auto pTeammate = pEntity->As<CTFPlayer>( );
		if ( !pTeammate->IsAlive( ) )
			continue;

		if ( pTeammate->GetAbsOrigin().DistToSqr(*position) <= pow(cart_radius, 2))
			teammates_near_cart++;
	}

	// Adjust position based on number of teammates to avoid crowding
    Vector adjusted_pos = *position;
    if (teammates_near_cart > 0)
    {
        // Create a ring formation around cart
        float angle = PI * 2 * (float)(I::EngineClient->GetLocalPlayer( ) % (teammates_near_cart + 1)) / (teammates_near_cart + 1);
        Vector offset(cos(angle) * 75.0f, sin(angle) * 75.0f, 0.0f);
        adjusted_pos += offset;
    }

	// Adjust position, so it's not floating high up, provided the local player is close.
	if ( vLocalOrigin.DistToSqr( adjusted_pos ) <= pow( 150.0f, 2 ) )
		adjusted_pos.z = vLocalOrigin.z;

	// If close enough, don't move (mostly due to lifts)
	if ( adjusted_pos.DistToSqr( vLocalOrigin ) <= pow( 15.0f, 2 ) )
	{
		overwrite_capture = true;
		return std::nullopt;
	}
	
	return adjusted_pos;
}

std::optional<Vector> CNavBot::GetControlPointGoal( const Vector vLocalOrigin, int our_team )
{
	static Vector previous_position;
	static Vector randomized_position;

	auto position = F::CPController.GetClosestControlPoint( vLocalOrigin, our_team );
	if ( !position )
		return std::nullopt;

	// Get number of teammates on point
    int teammates_on_point = 0;
    float cap_radius = 100.0f; // Approximate capture radius
	
	for ( auto pEntity : H::Entities.GetGroup( EGroupType::PLAYERS_TEAMMATES ) )
	{
		if ( !pEntity || pEntity->IsDormant() || pEntity->entindex( ) == I::EngineClient->GetLocalPlayer( ) )
			continue;

		auto pTeammate = pEntity->As<CTFPlayer>( );
		if ( !pTeammate->IsAlive( ) )
			continue;

		if ( pTeammate->GetAbsOrigin().DistToSqr(*position) <= pow(cap_radius, 2))
			teammates_on_point++;
	}

	// Check for enemies near point
    bool enemies_near = false;
    float threat_radius = 800.0f; // Distance to check for enemies
	
	for ( auto pEntity : H::Entities.GetGroup( EGroupType::PLAYERS_ENEMIES ) )
	{
		if ( !pEntity || pEntity->IsDormant())
			continue;

		auto pEnemy = pEntity->As<CTFPlayer>( );
		if ( !pEnemy->IsAlive( ) )
			continue;

		if ( pEnemy->GetAbsOrigin().DistToSqr(*position) <= pow(threat_radius, 2))
		{
			enemies_near = true;
			break;
		}
	}

	Vector adjusted_pos = *position;

	 // If enemies are near, take defensive positions
    if (enemies_near)
    {
		// Find nearby cover points
		for (auto &area : F::NavEngine.getNavFile()->m_areas)
		{
			for (auto &hiding_spot : area.m_hidingSpots)
			{
				if (hiding_spot.HasGoodCover() && hiding_spot.m_pos.DistTo(*position) <= cap_radius)
				{
					adjusted_pos = hiding_spot.m_pos;
					break;
				}
			}
		}
    }
	else
	{
		// Only update position when needed
		if (previous_position != *position || !F::NavEngine.isPathing())
		{
			previous_position = *position;
            
			// Create spread out formation based on player index and class
			float base_radius = 120.0f;
            
			// Add some randomization but keep formation
			float angle = PI * 2 * (float)(I::EngineClient->GetLocalPlayer( ) % (teammates_on_point + 1)) / (teammates_on_point + 1);
			float radius = base_radius + SDK::RandomFloat(-10.0f, 10.0f);
			Vector offset(cos(angle) * radius, sin(angle) * radius, 0.0f);
            
			adjusted_pos += offset;
		}
	}
	// If close enough, don't move
	if ( ( adjusted_pos.DistToSqr( vLocalOrigin ) <= pow( 50.0f, 2 ) ) )
	{
		overwrite_capture = true;
		return std::nullopt;
	}

	return adjusted_pos;
}

std::optional<Vector> CNavBot::GetDoomsdayGoal( CTFPlayer* pLocal, int our_team, int enemy_team )
{
	int iTeam = TEAM_UNASSIGNED;
	while( iTeam != -1 )
	{
		auto pFlag = F::FlagController.GetFlag(iTeam);
		if ( pFlag.pFlag )
			break;

		iTeam = iTeam != our_team ? our_team : -1;
	}

	// No australium found
	if ( iTeam == -1 )
		 return std::nullopt;

    // Get Australium related information
    auto status = F::FlagController.GetStatus(iTeam);
    auto position = F::FlagController.GetPosition(iTeam);
    auto carrier = F::FlagController.GetCarrier(iTeam);

    if (status == TF_FLAGINFO_STOLEN )
    { 
		// We have the australium
		if ( carrier == pLocal->entindex() )
		{
			// Get rocket position - in Doomsday it's marked as a cap point
			auto rocket = F::CPController.GetClosestControlPoint( pLocal->GetAbsOrigin( ), our_team );
			if ( rocket )
			{
				// If close enough, don't move
				if ( rocket->DistToSqr( pLocal->GetAbsOrigin( ) ) <= pow( 50.0f, 2 ) )
				{
					overwrite_capture = true;
					return std::nullopt;
				}
				return rocket;
			}
		}
		// Help friendly carrier
		else if ( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::HelpCaptureObjectives )
		{
			if (ShouldAssist(pLocal, carrier))
			{
				// Stay slightly behind and to the side to avoid blocking
				if ( position )
				{
					Vector offset( 40.0f, 40.0f, 0.0f );
					*position -= offset;
				}
				return position;
			}
		}
    }

    // Get the australium if not taken
    return position;
}

bool CNavBot::CaptureObjectives( CTFPlayer* pLocal, CTFWeaponBase* pWeapon )
{
	static Timer capture_timer;
	static Vector previous_target;

	if ( !( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::CaptureObjectives ) )
		return false;

	if ( const auto& pGameRules = I::TFGameRules( ) )
	{
		if ( !( ( pGameRules->m_iRoundState( ) == GR_STATE_RND_RUNNING || pGameRules->m_iRoundState( ) == GR_STATE_STALEMATE ) && !pGameRules->m_bInWaitingForPlayers( ) ) 
			|| pGameRules->m_iRoundState( ) == GR_STATE_TEAM_WIN 
			|| pGameRules->m_bPlayingSpecialDeliveryMode( ) )
			return false;
	}

	if ( !capture_timer.Check( 2.f ) )
		return F::NavEngine.current_priority == capture;

	// Priority too high, don't try
	if ( F::NavEngine.current_priority > capture )
		return false;

	// Where we want to go
	std::optional<Vector> target;

	int our_team = pLocal->m_iTeamNum( );
	int enemy_team = our_team == TF_TEAM_BLUE ? TF_TEAM_RED : TF_TEAM_BLUE;

	overwrite_capture = false;

	const auto vLocalOrigin = pLocal->GetAbsOrigin( );

	// Run logic
	switch ( F::GameObjectiveController.m_eGameMode )
	{
	case TF_GAMETYPE_CTF:
		target = GetCtfGoal( pLocal, our_team, enemy_team );
		break;
	case TF_GAMETYPE_CP:
		target = GetControlPointGoal( vLocalOrigin, our_team );
		break;
	case TF_GAMETYPE_ESCORT:
		target = GetPayloadGoal( vLocalOrigin, our_team );
		break;
	default:
		if ( F::GameObjectiveController.m_bDoomsday )
			target = GetDoomsdayGoal( pLocal, our_team, enemy_team );
		break;
	}

	// Overwritten, for example because we are currently on the payload, cancel any sort of pathing and return true
	if ( overwrite_capture )
	{
		F::NavEngine.cancelPath( );
		return true;
	}
	// No target, bail and set on cooldown
	else if ( !target )
	{
		capture_timer.Update( );
		return F::NavEngine.current_priority == capture;
	}
	// If priority is not capturing, or we have a new target, try to path there
	else if ( F::NavEngine.current_priority != capture || *target != previous_target )
	{
		if ( F::NavEngine.navTo( *target, capture, true, !F::NavEngine.isPathing( ) ) )
		{
			previous_target = *target;
			return true;
		}
		capture_timer.Update( );
	}
	return false;
}

template <typename Iter, typename RandomGenerator> Iter select_randomly(Iter start, Iter end, RandomGenerator &g)
{
	std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

template <typename Iter> Iter select_randomly(Iter start, Iter end)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return select_randomly(start, end, gen);
}

bool CNavBot::Roam( CTFPlayer* pLocal )
{
	static Timer roam_timer;
	static std::vector<CNavArea*> visited_areas;
	static Timer visited_areas_clear_timer;
	static CNavArea* current_target = nullptr;
	static int consecutive_fails = 0;

	// Don't path constantly
	/*if ( !roam_timer.Run( 2000 ) )
	{
		return false;
	}*/

	/*
	// No sniper spots :shrug:
	if ( sniper_spots.empty( ) )
		return false;

	// Don't overwrite current roam
	if ( F::NavEngine.current_priority == patrol )
		return false;

	// Max 10 attempts
	for ( int attempts = 0; attempts < 10; ++attempts )
	{
		// Get a random sniper spot
		auto random = select_randomly( sniper_spots.begin( ), sniper_spots.end( ) );
		// Try to nav there
		if ( F::NavEngine.navTo( *random, patrol ) )
			return true;
	}
	return false;
	*/

	// Clear visited areas every 60 seconds to allow revisiting
	if (visited_areas_clear_timer.Run(60.f))
	{
		visited_areas.clear();
		consecutive_fails = 0;
	}

	// Don't path constantly
	if (!roam_timer.Run(2.f))
		return false;

	if (F::NavEngine.current_priority > patrol)
		return false;

	 // Defend our objective if possible
	if ( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::DefendObjectives )
	{
		int enemy_team = pLocal->m_iTeamNum( ) == TF_TEAM_BLUE ? TF_TEAM_RED : TF_TEAM_BLUE;

		std::optional<Vector> target;
		const auto vLocalOrigin = pLocal->GetAbsOrigin( );

		switch ( F::GameObjectiveController.m_eGameMode )
		{
		case TF_GAMETYPE_CP:
			target = GetControlPointGoal( vLocalOrigin, enemy_team );
			break;
		case TF_GAMETYPE_ESCORT:
			target = GetPayloadGoal( vLocalOrigin, enemy_team );
			break;
		default:
			break;
		}
		if ( target )
		{
			if ( target->DistToSqr( vLocalOrigin ) <= pow( 250.0f, 2 ) )
			{
				F::NavEngine.cancelPath( );
				return true;
			}
			if ( F::NavEngine.navTo( *target, patrol, true, !F::NavEngine.isPathing() ) )
			{
				Defending = true;
				return true;
			}
		}
	}
	Defending = false;

	// If we have a current target and are pathing, continue
	if (current_target && F::NavEngine.current_priority == patrol)
		return true;

	// Reset current target
	current_target = nullptr;

	// Get our current nav area
	auto current_area = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());
	if (!current_area)
		return false;

	std::vector<CNavArea*> valid_areas;

	// Get all nav areas
	for (auto& area : F::NavEngine.getNavFile()->m_areas)
	{
		// Skip if area is blacklisted
		if (F::NavEngine.getFreeBlacklist()->find(&area) != F::NavEngine.getFreeBlacklist()->end())
			continue;
            
		// Dont run in spawn bitch
		if ( area.m_TFattributeFlags & ( TF_NAV_SPAWN_ROOM_BLUE | TF_NAV_SPAWN_ROOM_RED | TF_NAV_SPAWN_ROOM_EXIT ) )
			continue;

		// Skip if we recently visited this area
		if (std::find(visited_areas.begin(), visited_areas.end(), &area) != visited_areas.end())
			continue;

		// Skip areas that are too close
		float dist = area.m_center.DistToSqr(pLocal->GetAbsOrigin());
		if (dist < pow(500.0f,2))
			continue;
            
		valid_areas.push_back(&area);
	}

	// No valid areas found
	if (valid_areas.empty())
	{
		// If we failed too many times in a row, clear visited areas
		if (++consecutive_fails >= 3)
		{
			visited_areas.clear();
			consecutive_fails = 0;
		}
		return false;
	}

	// Reset fail counter since we found valid areas
	consecutive_fails = 0;

	// Different strategies for area selection
	std::vector<CNavArea*> potential_targets;
    
	// Strategy 1: Try to find areas that are far from current position
	for (auto area : valid_areas)
	{
		float dist = area->m_center.DistToSqr(pLocal->GetAbsOrigin());
		if (dist > pow(2000.0f,2))
			potential_targets.push_back(area);
	}
    
	// Strategy 2: If no far areas found, try areas that are at medium distance
	if (potential_targets.empty())
	{
		for (auto area : valid_areas)
		{
			float dist = area->m_center.DistToSqr(pLocal->GetAbsOrigin());
			if (dist > pow(1000.0f,2) && dist <= pow(2000.0f,2))
				potential_targets.push_back(area);
		}
	}

	// Strategy 3: If still no areas found, use any valid area
	if (potential_targets.empty())
		potential_targets = valid_areas;

	// Shuffle the potential targets to add randomness
	for (size_t i = potential_targets.size() - 1; i > 0; i--)
	{
		int j = rand() % (i + 1);
		std::swap(potential_targets[i], potential_targets[j]);
	}

	// Try to path to potential targets
	for (auto area : potential_targets)
	{
		if (F::NavEngine.navTo(area->m_center, patrol))
		{
			current_target = area;
			visited_areas.push_back(area);
			return true;
		}
	}

	return false;
}

// Check if a position is safe from stickies and projectiles
bool IsPositionSafe(Vector pos, int iLocalTeam)
{
    if (!(Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::Stickies ) &&
		!(Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::Projectiles ))
        return true;

    for (auto ent : H::Entities.GetGroup(EGroupType::WORLD_PROJECTILES))
    {
        if (!ent || ent->m_iTeamNum() == iLocalTeam)
            continue;

		auto ClassId = ent->GetClassID( );

        // Check for stickies
        if (Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::Stickies && ClassId == ETFClassID::CTFGrenadePipebombProjectile)
        {
            // Skip non-sticky projectiles
            if (ent->As<CTFGrenadePipebombProjectile>()->m_iType() != TF_GL_MODE_REMOTE_DETONATE)
                continue;

            float dist = ent->m_vecOrigin().DistToSqr(pos);
            if (dist < pow(Vars::Misc::Movement::NavBot::StickyDangerRange.Value, 2))
                return false;
        }
        
        // Check for rockets and pipes
        if (Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::Projectiles)
        {
            if (ClassId == ETFClassID::CTFProjectile_Rocket || 
                (ClassId == ETFClassID::CTFGrenadePipebombProjectile && ent->As<CTFGrenadePipebombProjectile>()->m_iType() == TF_GL_MODE_REGULAR ) )
            {
                float dist = ent->m_vecOrigin().DistToSqr(pos);
                if (dist < pow(Vars::Misc::Movement::NavBot::ProjectileDangerRange.Value, 2))
                    return false;
            }
        }
    }
    return true;
}

bool CNavBot::EscapeProjectiles( CTFPlayer* pLocal )
{
    if (!(Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::Stickies ) &&
		!(Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::Projectiles ) )
        return false;

    // Don't interrupt higher priority tasks
    if (F::NavEngine.current_priority > danger)
        return false;

    // Check if current position is unsafe
    if (IsPositionSafe(pLocal->GetAbsOrigin(), pLocal->m_iTeamNum()))
    {
        if (F::NavEngine.current_priority == danger)
            F::NavEngine.cancelPath();
        return false;
    }

    // Find safe nav areas sorted by distance
    std::vector<std::pair<CNavArea*, float>> safe_areas;
    auto *local_nav = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());
    
    if (!local_nav)
        return false;

    for (auto &area : F::NavEngine.getNavFile()->m_areas)
    {
        // Skip current area
        if (&area == local_nav)
            continue;
            
        // Skip if area is blacklisted
        if (F::NavEngine.getFreeBlacklist()->find(&area) != F::NavEngine.getFreeBlacklist()->end())
            continue;

        if (IsPositionSafe(area.m_center, pLocal->m_iTeamNum()))
        {
            float dist = area.m_center.DistTo(pLocal->GetAbsOrigin());
            safe_areas.push_back({&area, dist});
        }
    }

    // Sort by distance
    std::sort(safe_areas.begin(), safe_areas.end(),
        [](const std::pair<CNavArea*, float> &a, const std::pair<CNavArea*, float> &b) {
            return a.second < b.second;
        });

    // Try to path to closest safe area
    for (auto &area : safe_areas)
    {
        if (F::NavEngine.navTo(area.first->m_center, danger))
            return true;
    }

    return false;
}

bool CNavBot::EscapeDanger( CTFPlayer* pLocal )
{
	if ( !( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::EscapeDanger ) )
		return false;

	// Don't escape while we have the intel
	if ( Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::DontEscapeDangerIntel && F::GameObjectiveController.m_eGameMode == TF_GAMETYPE_CTF)
	{
		const auto flag_carrier = F::FlagController.GetCarrier( pLocal->m_iTeamNum( ) );
		if ( flag_carrier == pLocal->entindex() )
			return false;
	}

	// Priority too high
	if ( F::NavEngine.current_priority > danger || F::NavEngine.current_priority == prio_melee || F::NavEngine.current_priority == run_safe_reload )
		return false;

	auto local_nav = F::NavEngine.findClosestNavSquare( pLocal->GetAbsOrigin( ) );
	const auto blacklist = F::NavEngine.getFreeBlacklist( );

	// Check if we're in spawn - if so, ignore danger and focus on getting out
	if (local_nav && (local_nav->m_TFattributeFlags & TF_NAV_SPAWN_ROOM_RED || local_nav->m_TFattributeFlags & TF_NAV_SPAWN_ROOM_BLUE))
		return false;

	// In danger, try to run (besides if it's a building spot, don't run away from that)
	if ( blacklist->contains( local_nav ) )
	{
		if ( ( *blacklist )[ local_nav ].value == BR_BAD_BUILDING_SPOT )
			return false;

		static CNavArea* target_area = nullptr;
		// Already running and our target is still valid
		if ( F::NavEngine.current_priority == danger && !blacklist->contains( target_area ) )
			return true;

		std::vector<CNavArea*> nav_areas_ptr;
		// Copy a ptr list (sadly cat_nav_init exists so this cannot be only done once)
		for ( auto& nav_area : F::NavEngine.getNavFile( )->m_areas )
			nav_areas_ptr.push_back( &nav_area );

		// Sort by distance
		std::sort( nav_areas_ptr.begin( ), nav_areas_ptr.end( ), [&]( CNavArea* a, CNavArea* b ) -> bool
			{
				return a->m_center.DistToSqr( pLocal->GetAbsOrigin( ) ) < b->m_center.DistToSqr( pLocal->GetAbsOrigin( ) ); 
			} );

		int calls = 0;
		// Try to path away
		for ( const auto& area : nav_areas_ptr )
		{
			if ( !blacklist->contains( area ) )
			{
				// only try the 5 closest valid areas though, something is wrong if this fails
				calls++;
				if ( calls > 5 )
					break;
				if ( F::NavEngine.navTo( area->m_center, danger ) )
				{
					target_area = area;
					return true;
				}
			}
		}
	}
	// No longer in danger
	else if ( F::NavEngine.current_priority == danger )
		F::NavEngine.cancelPath( );

	return false;
}

bool CNavBot::EscapeSpawn( CTFPlayer* pLocal )
{
	static Timer spawn_escape_cooldown{};
    
	// Don't try too often
	if (!spawn_escape_cooldown.Run(0.5f))
		return F::NavEngine.current_priority == escape_spawn;
        
	auto local_nav = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());
	if (!local_nav)
		return false;
        
	// Check if we're in spawn
	bool in_spawn = local_nav->m_TFattributeFlags & (TF_NAV_SPAWN_ROOM_RED | TF_NAV_SPAWN_ROOM_BLUE | TF_NAV_SPAWN_ROOM_EXIT);
    
	// Cancel if we're not in spawn and this is running
	if (!in_spawn && F::NavEngine.current_priority == escape_spawn)
	{
		F::NavEngine.cancelPath();
		return false;
	}
    
	// Not in spawn, don't try
	if (!in_spawn)
		return false;
        
	// Try to find an exit
	for (auto &nav_area : F::NavEngine.getNavFile()->m_areas)
	{
		// Skip spawn areas
		if (nav_area.m_TFattributeFlags & (TF_NAV_SPAWN_ROOM_RED | TF_NAV_SPAWN_ROOM_BLUE | TF_NAV_SPAWN_ROOM_EXIT))
			continue;
            
		// Try to get there
		if (F::NavEngine.navTo(nav_area.m_center, escape_spawn))
			return true;
	}
    
	return false;
}

slots CNavBot::GetBestSlot( CTFPlayer* pLocal, slots active_slot, std::pair<int, float> nearest )
{
	if ( Vars::Misc::Movement::NavBot::WeaponSlot.Value != Vars::Misc::Movement::NavBot::WeaponSlotEnum::Best )
		return static_cast<slots>( Vars::Misc::Movement::NavBot::WeaponSlot.Value - 1 );

	if ( pLocal && pLocal->IsAlive( ) )
	{
		auto pPlayer = I::ClientEntityList->GetClientEntity( nearest.first )->As<CTFPlayer>( );

		auto pPrimaryWeapon = pLocal->GetWeaponFromSlot( SLOT_PRIMARY );
		auto pSecondaryWeapon = pLocal->GetWeaponFromSlot( SLOT_SECONDARY );
		if (pPrimaryWeapon && pSecondaryWeapon)
		{
			int iPrimaryResAmmo = SDK::WeaponDoesNotUseAmmo(pPrimaryWeapon) ? -1 : pLocal->GetAmmoCount( pPrimaryWeapon->m_iPrimaryAmmoType( ) );
			int iSecondaryResAmmo = SDK::WeaponDoesNotUseAmmo(pSecondaryWeapon) ? -1 : pLocal->GetAmmoCount( pSecondaryWeapon->m_iPrimaryAmmoType( ) );
			switch ( pLocal->m_iClass( ) )
			{
			case TF_CLASS_SCOUT:
			{
				if ( ( !G::AmmoInSlot[ SLOT_PRIMARY ] && ( !G::AmmoInSlot[ SLOT_SECONDARY ] || ( iSecondaryResAmmo != -1 &&
					iSecondaryResAmmo <= SDK::GetWeaponMaxReserveAmmo( G::SavedWepIds[ SLOT_SECONDARY ], G::SavedDefIndexes[ SLOT_SECONDARY ] ) / 4 ) ) ) && nearest.second <= pow( 200.f, 2 ) )
					return melee;

				if ( G::AmmoInSlot[ SLOT_PRIMARY ] && nearest.second <= pow( 800.f, 2 ) )
					return primary;

				else if ( G::AmmoInSlot[ SLOT_SECONDARY ] )
					return secondary;

				break;
			}
			case TF_CLASS_HEAVY:
			{
				if ( !G::AmmoInSlot[ SLOT_PRIMARY ] && !G::AmmoInSlot[ SLOT_SECONDARY ] ||
					( G::SavedDefIndexes[ SLOT_MELEE ] == Heavy_t_TheHolidayPunch && ( pPlayer && !pPlayer->IsTaunting( ) && pPlayer->IsInvulnerable( ) ) && nearest.second < pow( 400.f, 2 ) ) )
					return melee;

				else if ( ( !pPlayer || nearest.second <= pow( 900.f, 2 ) ) && G::AmmoInSlot[ SLOT_PRIMARY ] )
					return primary;

				else if ( G::AmmoInSlot[ SLOT_SECONDARY ] && G::SavedWepIds[ SLOT_SECONDARY ] == TF_WEAPON_SHOTGUN_HWG )
					return secondary;

				break;
			}
			case TF_CLASS_MEDIC:
			{
				/*if ( AutoMedic->FoundTarget )
				{
					return secondary;
				}*/

				if ( !G::AmmoInSlot[ SLOT_PRIMARY ] ||
					( nearest.second <= pow( 400.f, 2 ) && pPlayer/* && !pPlayer->IsInvulnerable( )*/ ) )
					return melee;

				return primary;
			}
			case TF_CLASS_SPY:
			{
				if ( nearest.second <= pow( 250.0f, 2 ) && pPlayer/* && !pPlayer->IsInvulnerable( )*/ )
					return melee;
				else if ( G::AmmoInSlot[ SLOT_PRIMARY ] )
					return primary;

				break;
			}
			case TF_CLASS_SNIPER:
			{
				// We have already skipped the targets which are invulnerable
				// bool bPlayerInvuln = pPlayer && pPlayer->IsInvulnerable( );
				int iPlayerLowHp = pPlayer ? ( pPlayer->m_iHealth( ) < pPlayer->GetMaxHealth( ) * 0.35f ? 2 : pPlayer->m_iHealth( ) < pPlayer->GetMaxHealth( ) * 0.75f ) : -1;

				if ( !G::AmmoInSlot[ SLOT_PRIMARY ] && !G::AmmoInSlot[ SLOT_SECONDARY ] ||
					( nearest.second <= pow( 200.0f, 2 ) && pPlayer/*&& !bPlayerInvuln*/ ) )
					return melee;

				if ( G::AmmoInSlot[ SLOT_SECONDARY ] &&
					( nearest.second <= pow( 300.0f, 2 ) && iPlayerLowHp > 1 /*&& !bPlayerInvuln*/ ) )
					return secondary;

				// Keep the smg if the target we previosly tried shooting is running away
				else if ( active_slot && active_slot < 3 && G::AmmoInSlot[ active_slot - 1 ] &&
					( nearest.second <= pow( 800.0f, 2 ) && iPlayerLowHp > 1 /*&& !bPlayerInvuln*/ ) )
					break;

				else if ( G::AmmoInSlot[ SLOT_PRIMARY ] )
					return primary;

				break;
			}
			case TF_CLASS_PYRO:
			{
				if ( !G::AmmoInSlot[ SLOT_PRIMARY ] && !G::AmmoInSlot[ SLOT_SECONDARY ] && ( pPlayer && nearest.second <= pow( 300.0f, 2 ) ) )
					return melee;

				if ( G::AmmoInSlot[ SLOT_PRIMARY ] && ( nearest.second <= pow( 550.0f, 2 ) || !pPlayer ) )
					return primary;
				else if ( G::AmmoInSlot[ SLOT_SECONDARY ] )
					return secondary;

				break;
			}
			case TF_CLASS_SOLDIER:
			{
				auto pEnemyWeapon = pPlayer ? pPlayer->m_hActiveWeapon( ).Get( )->As<CTFWeaponBase>() : nullptr;
				bool bEnemyCanAirblast = pEnemyWeapon && pEnemyWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER && pEnemyWeapon->m_iItemDefinitionIndex() != Pyro_m_ThePhlogistinator;
				bool bEnemyClose = pPlayer && nearest.second <= pow( 250.0f, 2 );
				if ( bEnemyClose && ( pPlayer->m_iHealth( ) < 80 ? !G::AmmoInSlot[ SLOT_SECONDARY ] : pPlayer->m_iHealth( ) >= 150 || G::AmmoInSlot[ SLOT_SECONDARY ] < 2 ) )
					return melee;

				if ( G::AmmoInSlot[ SLOT_SECONDARY ] && ( bEnemyCanAirblast || ( nearest.second <= pow( 350.0f, 2 ) && pPlayer && pPlayer->m_iHealth() < 125 ) ) )
					return secondary;
				else if ( G::AmmoInSlot[ SLOT_PRIMARY ] )
					return primary;

				break;
			}
			case TF_CLASS_DEMOMAN:
			{
				if (!G::AmmoInSlot[ SLOT_PRIMARY ] && !G::AmmoInSlot[ SLOT_SECONDARY ] && ( pPlayer && nearest.second <= pow( 200.0f, 2 ) ) )
					return melee;

				if ( G::AmmoInSlot[ SLOT_PRIMARY ] && ( nearest.second <= pow( 800.0f, 2 ) ) )
					return primary;
				else if ( G::AmmoInSlot[ SLOT_SECONDARY ] || iSecondaryResAmmo >= SDK::GetWeaponMaxReserveAmmo( G::SavedWepIds[ SLOT_SECONDARY ], G::SavedDefIndexes[ SLOT_SECONDARY ] ) / 2 )
					return secondary;

				break;
			}
			case TF_CLASS_ENGINEER:
			{
				auto pSentry = I::ClientEntityList->GetClientEntity( mySentryIdx );
				auto pDispenser = I::ClientEntityList->GetClientEntity( mySentryIdx );
				if ( ( ( pSentry && pSentry->GetAbsOrigin( ).DistToSqr( pLocal->GetAbsOrigin( ) ) <= pow( 300.0f, 2 ) ) || ( pDispenser && pDispenser->GetAbsOrigin( ).DistToSqr( pLocal->GetAbsOrigin( ) ) <= pow( 500.0f, 2 ) ) ) || ( current_building_spot && current_building_spot->DistToSqr( pLocal->GetAbsOrigin( ) ) <= pow( 500.0f, 2 ) ) )
				{
					if ( active_slot >= melee && F::NavEngine.current_priority != prio_melee )
						return active_slot;
					else
						return melee;
				}	
				else if ( !G::AmmoInSlot[ SLOT_PRIMARY ] && !G::AmmoInSlot[ SLOT_SECONDARY ] && ( pPlayer && nearest.second <= pow( 200.0f, 2 ) ) )
					return melee;
				else if ( G::AmmoInSlot[ SLOT_PRIMARY ] && ( pPlayer && nearest.second <= pow( 500.0f, 2 ) ) )
					return primary;
				else if ( G::AmmoInSlot[ SLOT_SECONDARY ] )
					return secondary;

				break;
			}
			default:
				break;
			}
		}
	}
	return active_slot;
}

void CNavBot::UpdateSlot( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, std::pair<int, float> nearest )
{
	static Timer slot_timer{};
	if ( !slot_timer.Run( 0.3f ) )
		return;

	slot = pWeapon->GetSlot( ) + 1;

	// Prioritize reloading
	int iReloadSlot = GetReloadWeaponSlot( pLocal, nearest );
	m_eLastReloadSlot = static_cast< slots >( iReloadSlot );

	int newslot = iReloadSlot ? iReloadSlot : ( Vars::Misc::Movement::NavBot::WeaponSlot.Value ? GetBestSlot( pLocal, static_cast< slots >( slot ), nearest ) : -1 );
	if ( newslot > -1 )
	{
		auto command = "slot" + std::to_string( newslot );
		if ( slot != newslot )
			I::EngineClient->ClientCmd_Unrestricted( command.c_str( ) );
	}
}

void CNavBot::AutoScope( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd )
{
	static bool bKeep = false;
	static Timer keepTimer{};
	static Timer scopeTimer{};
	bool bIsClassic = pWeapon->GetWeaponID( ) == TF_WEAPON_SNIPERRIFLE_CLASSIC;
	if ( !Vars::Misc::Movement::NavBot::AutoScope.Value || pWeapon->GetWeaponID( ) != TF_WEAPON_SNIPERRIFLE && !bIsClassic && pWeapon->GetWeaponID( ) != TF_WEAPON_SNIPERRIFLE_DECAP )
	{
		bKeep = false;
		return;
	}

	if ( bIsClassic )
	{
		if ( bKeep )
		{
			if ( !( pCmd->buttons & IN_ATTACK ) )
				pCmd->buttons |= IN_ATTACK;
			if ( keepTimer.Check( Vars::Misc::Movement::NavBot::AutoScopeCancelTime.Value ) ) // cancel classic charge
				pCmd->buttons |= IN_JUMP;
		}
		if ( !pLocal->OnSolid( ) && !( pCmd->buttons & IN_ATTACK ) )
			bKeep = false;
	}
	else
	{
		if ( bKeep )
		{
			if ( pLocal->InCond(TF_COND_ZOOMED) )
			{
				if ( scopeTimer.Check( Vars::Misc::Movement::NavBot::AutoScopeCancelTime.Value ) )
				{
					bKeep = false;
					pCmd->buttons |= IN_ATTACK2;
				}
				return;
			}
		}
	}

	auto vLocalOrigin = pLocal->GetAbsOrigin( );

	CNavArea* pCurrentDestinationArea = nullptr;
	auto crumbs = F::NavEngine.getCrumbs();
	if ( crumbs->size( ) > 4 )
		pCurrentDestinationArea = crumbs->at(4).navarea;

	auto pLocalNav = pCurrentDestinationArea ? pCurrentDestinationArea : F::NavEngine.findClosestNavSquare(vLocalOrigin);
	if (!pLocalNav)
		return;

	Vector vFrom = pLocalNav->m_center;
	vFrom.z += PLAYER_JUMP_HEIGHT;

	std::vector<std::pair<CBaseEntity*, float>> vEnemiesSorted;
	for ( auto pEnemy : H::Entities.GetGroup( EGroupType::PLAYERS_ENEMIES ) )
	{
		if (!pEnemy || pEnemy->IsDormant())
			continue;

		if (!ShouldTarget(pEnemy->entindex(), pLocal, pWeapon) )
			continue;
		
		vEnemiesSorted.emplace_back( pEnemy, pEnemy->GetAbsOrigin( ).DistToSqr( vLocalOrigin ) );
	}

	if ( vEnemiesSorted.empty( ) )
		return;

	std::sort( vEnemiesSorted.begin( ), vEnemiesSorted.end( ), [&]( std::pair<CBaseEntity*, float> a, std::pair<CBaseEntity*, float> b ) -> bool { return a.second < b.second; } );

	auto CheckVisibility = [&](const Vec3& vTo) -> bool
	{
		CGameTrace trace = {};
		CTraceFilterWorldAndPropsOnly filter = {}; 
		
		// Trace from local pos first
		SDK::Trace( Vector( vLocalOrigin.x, vLocalOrigin.y, vLocalOrigin.z + PLAYER_JUMP_HEIGHT ), vTo, MASK_SHOT | CONTENTS_GRATE, &filter, &trace );
		bool bHit = trace.fraction == 1.0f;
		if ( !bHit )
		{
			// Try to trace from our destination pos
			SDK::Trace( vFrom, vTo, MASK_SHOT | CONTENTS_GRATE, &filter, &trace );
			bHit = trace.fraction == 1.0f;
		}

		if ( bHit )
		{
			if ( bIsClassic )
			{
				keepTimer.Update( );
				pCmd->buttons |= IN_ATTACK;
			}
			else if ( !( pCmd->buttons & IN_ATTACK2 ) )
			{
				pCmd->buttons |= IN_ATTACK2;
				scopeTimer.Update( );
			}
			return bKeep = true;
		}
		return false;
	};

	PlayerStorage storage;
	for ( auto [pEnemy, _] : vEnemiesSorted )
	{
		Vector vNonPredictedPos = pEnemy->GetAbsOrigin( );
		vNonPredictedPos.z += PLAYER_JUMP_HEIGHT;
		if ( CheckVisibility( vNonPredictedPos ) )
			return;

		F::MoveSim.Initialize( pEnemy, storage, false );
		if ( storage.m_bFailed )
		{
			F::MoveSim.Restore( storage );
			continue;
		}

		for ( int i = 0; i < TIME_TO_TICKS( 0.5f ); i++ )
			F::MoveSim.RunTick( storage );

		auto pTargetNav = F::NavEngine.findClosestNavSquare(storage.m_vPredictedOrigin);
		if ( pTargetNav )
		{
			Vector vTo = pTargetNav->m_center;

			// If player is in the air dont try to vischeck nav areas below him, check the predicted position instead
			if ( !pEnemy->As<CBasePlayer>( )->OnSolid( ) && vTo.DistToSqr(storage.m_vPredictedOrigin) >= 1000000.f)
				vTo = storage.m_vPredictedOrigin;
			
			vTo.z += PLAYER_JUMP_HEIGHT;
			if ( CheckVisibility( vTo ) )
			{
				F::MoveSim.Restore( storage );
				return;
			}
		}
		F::MoveSim.Restore( storage );
	}
}

bool IsWeaponValidForDT( CTFWeaponBase* pWeapon )
{
	if ( !pWeapon )
		return false;

	const auto WepID = pWeapon->GetWeaponID( );
	if ( WepID == TF_WEAPON_SNIPERRIFLE || WepID == TF_WEAPON_SNIPERRIFLE_CLASSIC || WepID == TF_WEAPON_SNIPERRIFLE_DECAP )
		return false;

	return IsWeaponAimable( pWeapon );
}

void CNavBot::CreateMove( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd )
{	
	static Timer DoubletapRecharge{};
	if ( !Vars::Misc::Movement::NavBot::Enabled.Value || !F::NavEngine.isReady( ) )
	{
		G::NavbotTargetIdx = -1;
		return;
	}

	if ( F::NavEngine.current_priority != staynear )
		G::NavbotTargetIdx = -1;

	if ( F::Ticks.m_bWarp || F::Ticks.m_bDoubletap )
		return;

	if ( !pLocal || !pLocal->IsAlive( ) || !pWeapon || !pCmd  )
		return;

	if ( pCmd->buttons & ( IN_FORWARD | IN_BACK | IN_MOVERIGHT | IN_MOVELEFT ) && !F::Misc.m_bAntiAFK )
		return;

	AutoScope( pLocal, pWeapon, pCmd );

	//Recharge doubletap every n seconds
	if ( Vars::Misc::Movement::NavBot::RechargeDT.Value && IsWeaponValidForDT(pWeapon))
	{
		if ( !F::Ticks.m_bRechargeQueue &&
			( Vars::Misc::Movement::NavBot::RechargeDT.Value != Vars::Misc::Movement::NavBot::RechargeDTEnum::WaitForFL || !Vars::CL_Move::Fakelag::Fakelag.Value || !F::FakeLag.m_iGoal ) &&
			G::Attacking != 1 &&
			( F::Ticks.m_iShiftedTicks < F::Ticks.m_iMaxShift) && DoubletapRecharge.Check( Vars::Misc::Movement::NavBot::RechargeDTDelay.Value ) )
			F::Ticks.m_bRechargeQueue = true;
		else if ( F::Ticks.m_iShiftedTicks >= F::Ticks.m_iMaxShift )
			DoubletapRecharge.Update( );
	}

	RefreshSniperSpots( );
	RefreshLocalBuildings( pLocal );
	auto nearest = GetNearestPlayerDistance(pLocal, pWeapon);
	RefreshBuildingSpots( pLocal, pLocal->GetWeaponFromSlot(SLOT_MELEE), nearest );

	// Update the distance config
	switch ( pLocal->m_iClass() )
	{
	case TF_CLASS_SCOUT:
	case TF_CLASS_HEAVY:
		selected_config = CONFIG_SHORT_RANGE;
		break;
	case TF_CLASS_ENGINEER:
		selected_config = IsEngieMode( pLocal ) ? pWeapon->m_iItemDefinitionIndex( ) == Engi_t_TheGunslinger ? CONFIG_GUNSLINGER_ENGINEER : CONFIG_ENGINEER : CONFIG_SHORT_RANGE;
		break;
	case TF_CLASS_SNIPER:
		selected_config = pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW ? CONFIG_MID_RANGE : CONFIG_LONG_RANGE;
		break;
	default:
		selected_config = CONFIG_MID_RANGE;
	}
	
	// Spin up the minigun if there are enemies nearby or if we had an active aimbot target 
	if ( pWeapon->GetWeaponID( ) == TF_WEAPON_MINIGUN )
	{
		static Timer spinupTimer{};
		const auto& pEntity = I::ClientEntityList->GetClientEntity( nearest.first );
		if ( pEntity && pEntity->As<CTFPlayer>()->IsAlive( ) && !pEntity->As<CTFPlayer>()->IsInvulnerable( ) && pWeapon->HasAmmo( ) )
		{
			if ( abs(G::Target.second - I::GlobalVars->tickcount) < 32 || nearest.second <= pow(800.f, 2) )
				spinupTimer.Update( );
			if ( !spinupTimer.Check( 3.f ) ) // 3 seconds until unrev
				pCmd->buttons |= IN_ATTACK2;
		}
	}

	UpdateSlot( pLocal, pWeapon, nearest );
	UpdateEnemyBlacklist( pLocal, pWeapon, slot );

	// TODO:
	// Add engie logic and target sentries logic. (Done)
	// Also maybe add some spy sapper logic? (No.)
	// Fix defend and help capture logic
	// Fix capture logic (control points, sd_doomsday)
	// Fix reload stuff because its really janky
	// Finish auto wewapon stuff
	// Make a better closest enemy lorgic

	if ( EscapeSpawn( pLocal )
		|| EscapeProjectiles( pLocal )
		|| MeleeAttack( pCmd, pLocal, slot, nearest )
		|| EscapeDanger( pLocal )
		|| GetHealth( pCmd, pLocal )
		|| GetAmmo( pCmd, pLocal )
		//|| RunReload( pLocal, pWeapon )
		|| RunSafeReload( pLocal, pWeapon )
		|| CaptureObjectives( pLocal, pWeapon )
		|| EngineerLogic( pCmd, pLocal, nearest )
		|| SnipeSentries( pLocal )
		|| StayNear( pLocal, pWeapon )
		|| GetHealth( pCmd, pLocal, true )
		|| Roam( pLocal ) )
	{
		// Force crithack in dangerous conditions
		// TODO:
		// Maybe add some logic to it (more logic)
		CTFPlayer* pPlayer = nullptr;
		switch( F::NavEngine.current_priority )
		{
		case staynear:
			pPlayer = I::ClientEntityList->GetClientEntity( G::NavbotTargetIdx )->As<CTFPlayer>( );
			if ( pPlayer )
				F::CritHack.m_bForce = !pPlayer->IsDormant() && pPlayer->m_iHealth() >= pWeapon->GetDamage();
			break;
		case prio_melee:
		case health:
		case danger:
			pPlayer = I::ClientEntityList->GetClientEntity( nearest.first )->As<CTFPlayer>( );
			F::CritHack.m_bForce = pPlayer && !pPlayer->IsDormant() && pPlayer->m_iHealth() >= pWeapon->GetDamage();
			break;
		default: 
			F::CritHack.m_bForce = false;
			break;
		}
	}
}

void CNavBot::OnLevelInit( )
{
	// Make it run asap
	refresh_sniperspots_timer -= 60;
	G::NavbotTargetIdx = -1;
	//snipe_target = nullptr;
	//staynear_target = nullptr;
	mySentryIdx = -1;
	myDispenserIdx = -1;
	sniper_spots.clear( );
}
