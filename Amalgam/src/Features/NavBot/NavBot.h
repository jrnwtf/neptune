#pragma once
#include "NavEngine/NavEngine.h"

enum slots
{
	primary = 1,
	secondary,
	melee,
	pda1,
	pda2
};

enum building
{
    dispenser = 0,
    sentry    = 2
};

class CNavBot
{
public:
	std::vector<Vector> slight_danger_drawlist_normal;
	std::vector<Vector> slight_danger_drawlist_dormant;
	bool Defending = false;
	std::wstring FollowTargetName{};
	std::wstring EngineerTask{};
	std::pair<int, float> GetNearestPlayerDistance( CTFPlayer* pLocal, CTFWeaponBase* pWeapon );
private:
	// Controls the bot parameters like distance from enemy
	struct bot_class_config
	{
		float min_full_danger;
		float min_slight_danger;
		float max;
		bool prefer_far;
	};

	const bot_class_config CONFIG_SHORT_RANGE = { 140.0f, 400.0f, 600.0f, false };
	const bot_class_config CONFIG_MID_RANGE = { 200.0f, 500.0f, 3000.0f, true };
	const bot_class_config CONFIG_LONG_RANGE = { 300.0f, 500.0f, 4000.0f, true };
	const bot_class_config CONFIG_ENGINEER = { 200.0f, 500.0f, 3000.0f, false };
	const bot_class_config CONFIG_GUNSLINGER_ENGINEER = { 50.0f, 300.0f, 2000.0f, false };
	bot_class_config selected_config;

	bool ShouldSearchHealth( CTFPlayer* pLocal, bool low_priority = false );
	bool ShouldSearchAmmo( CTFPlayer* pLocal );
	int ShouldTarget( int iPlayerIdx, CTFPlayer* pLocal, CTFWeaponBase* pWeapon );
	bool ShouldAssist( CTFPlayer* pLocal, int iTargetIdx );

	// Get Valid Dispensers (Used for health/ammo)
	std::vector<CObjectDispenser*> GetDispensers( CTFPlayer* pLocal );

	// Get entities of given itemtypes (Used for health/ammo)
	// Use true for health packs, use false for ammo packs
	std::vector<CBaseEntity*> GetEntities( CTFPlayer* pLocal, bool find_health = false );

	// Find health if needed
	bool GetHealth( CUserCmd* pCmd, CTFPlayer* pLocal, bool low_priority = false );

	// Find ammo if needed
	bool GetAmmo( CUserCmd* pCmd, CTFPlayer* pLocal, bool force = false );

	// Vector of sniper spot positions we can nav to
	std::vector<Vector> sniper_spots;
	void RefreshSniperSpots( );

	bool IsEngieMode( CTFPlayer* pLocal );

	bool BlacklistedFromBuilding( CNavArea* area );

	std::vector<Vector> building_spots;
	void RefreshBuildingSpots( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, std::pair<int, float> nearest, bool force = false );

	int mySentryIdx = -1;
	int myDispenserIdx = -1;

	void RefreshLocalBuildings( CTFPlayer* pLocal );

	std::optional<Vector> current_building_spot;
	bool NavToSentrySpot( );

	void UpdateEnemyBlacklist( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int slot );

	// Check if an area is valid for stay near. the Third parameter is to save some performance.
	bool IsAreaValidForStayNear( Vector ent_origin, CNavArea* area, bool fix_local_z = true );

	// Actual logic, used to de-duplicate code
	bool StayNearTarget( int entindex );

	// A bunch of basic checks to ensure we don't try to target an invalid entity
	int IsStayNearTargetValid( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int entindex );

	// Recursive function to find hiding spot
	std::optional<std::pair<CNavArea*, int>> FindClosestHidingSpot( CNavArea* area, std::optional<Vector> vischeck_point, int recursion_count, int index = 0 );
	bool RunReload( CTFPlayer* pLocal, CTFWeaponBase* pWeapon );
	
	slots GetReloadWeaponSlot( CTFPlayer* pLocal, std::pair<int, float> nearest );
	bool RunSafeReload( CTFPlayer* pLocal, CTFWeaponBase* pWeapon );

	// Try to stay near enemies and stalk them (or in case of sniper, try to stay far from them
	// and snipe them)
	bool StayNear( CTFPlayer* pLocal, CTFWeaponBase* pWeapon );

	// if melee aimbot/navbot crashes, this is where the problem is.
	bool MeleeAttack( CUserCmd* pCmd, CTFPlayer* pLocal, int slot, std::pair<int, float> nearest ); // also known as "melee AI"

	// Basically the same as isAreaValidForStayNear, but some restrictions lifted.
	bool IsAreaValidForSnipe( Vector ent_origin, Vector area_origin, bool fix_sentry_z = true );

	// Try to snipe the sentry
	bool TryToSnipe( CBaseObject* pBulding );

	// Is our target valid?
	bool IsSnipeTargetValid( int iBuildingIdx );

	CBaseObject* snipe_target = nullptr;
	// Try to Snipe sentries
	bool SnipeSentries( CTFPlayer* pLocal );

	int build_attempts = 0;
	bool BuildBuilding( CUserCmd* pCmd, CTFPlayer* pLocal, std::pair<int, float> nearest, int building );

	bool BuildingNeedsToBeSmacked( CBaseObject* pBulding );
	bool SmackBuilding( CUserCmd* pCmd, CTFPlayer* pLocal, CBaseObject* pBuilding );
	bool EngineerLogic( CUserCmd* pCmd, CTFPlayer* pLocal, std::pair<int, float> nearest );

	// Overwrite to return true for payload carts as an example
	bool overwrite_capture = false;

	std::optional<Vector> GetCtfGoal( CTFPlayer* pLocal, int our_team, int enemy_team );
	std::optional<Vector> GetPayloadGoal( const Vector vLocalOrigin, int our_team );
	std::optional<Vector> GetControlPointGoal( const Vector vLocalOrigin, int our_team );
	std::optional<Vector> GetDoomsdayGoal( CTFPlayer* pLocal, int our_team, int enemy_team );

	// Try to capture objectives
	bool CaptureObjectives( CTFPlayer* pLocal, CTFWeaponBase* pWeapon );

	// Roam around map
	bool Roam( CTFPlayer* pLocal );

	// Find safe position to escape to
	bool EscapeProjectiles( CTFPlayer* pLocal );

	// Run away from dangerous areas
	bool EscapeDanger( CTFPlayer* pLocal );
	bool EscapeSpawn( CTFPlayer* pLocal );

	int slot = primary;
	slots m_eLastReloadSlot = slots();

	slots GetBestSlot( CTFPlayer* pLocal, slots active_slot, std::pair<int, float> nearest );
	void UpdateSlot( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, std::pair<int, float> nearest );
	void AutoScope( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd );
public:
	void CreateMove( CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd );

	void OnLevelInit( );
};

ADD_FEATURE( CNavBot, NavBot );