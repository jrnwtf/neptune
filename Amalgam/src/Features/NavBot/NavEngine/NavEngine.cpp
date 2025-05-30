#include "NavEngine.h"
#include "../../Ticks/Ticks.h"
#include "../../Misc/Misc.h"
#include <direct.h>

std::optional<Vector> CNavParser::GetDormantOrigin(int iIndex)
{
	if (!iIndex)
		return std::nullopt;

	auto pEntity = I::ClientEntityList->GetClientEntity(iIndex);
	if (!pEntity || !pEntity->As<CBasePlayer>()->IsAlive())
		return std::nullopt;

	if (!pEntity->IsPlayer() && !pEntity->IsBuilding())
		return std::nullopt;

	if (!pEntity->IsDormant() || H::Entities.GetDormancy(iIndex))
		return pEntity->GetAbsOrigin();

	return std::nullopt;
}

bool CNavParser::IsSetupTime()
{
	static Timer tCheckTimer{};
	static bool bSetupTime = false;
	if (Vars::NavEng::NavEngine::PathInSetup.Value)
		return false;

	auto pLocal = H::Entities.GetLocal();
	if (pLocal && pLocal->IsAlive())
	{
		std::string sLevelName = SDK::GetLevelName();

		// No need to check the round states that quickly.
		if (tCheckTimer.Run(1.5f))
		{
			// Special case for Pipeline which doesn't use standard setup time
			if (sLevelName == "plr_pipeline")
				return false;

			if (auto pGameRules = I::TFGameRules())
			{
				// The round just started, players cant move.
				if (pGameRules->m_iRoundState() == GR_STATE_PREROUND)
					return bSetupTime = true;

				if (pLocal->m_iTeamNum() == TF_TEAM_BLUE)
				{
					if (pGameRules->m_bInSetup() || (pGameRules->m_bInWaitingForPlayers() && (sLevelName.starts_with("pl_") || sLevelName.starts_with("cp_"))))
						return bSetupTime = true;
				}
				bSetupTime = false;
			}
		}
	}
	return bSetupTime;
}

bool CNavParser::IsVectorVisibleNavigation(Vector from, Vector to, unsigned int mask)
{
	// Validate input vectors to prevent crashes
	if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(from.z) ||
		!std::isfinite(to.x) || !std::isfinite(to.y) || !std::isfinite(to.z))
		return false;
		
	// Skip if vectors are too close
	if (from.DistToSqr(to) < 1.0f)
		return true;
		
	// Skip if vectors are too far (likely to fail anyway)
	if (from.DistToSqr(to) > pow(3000.0f, 2))
		return false;

	try
	{
		Ray_t ray;
		ray.Init(from, to);	
		CGameTrace trace_visible;
		CTraceFilterNavigation Filter{};
		
		I::EngineTrace->TraceRay(ray, mask, &Filter, &trace_visible);
		
		// If we hit something but it's very close to the destination, still consider it visible
		if (trace_visible.fraction < 1.0f && trace_visible.fraction > 0.97f)
		{
			// Check if end point is close enough to destination
			float dist_to_end = trace_visible.endpos.DistToSqr(to);
			if (dist_to_end < pow(20.0f, 2))
				return true;
		}
		
		return trace_visible.fraction == 1.0f;
	}
	catch (...)
	{
		// If trace fails for any reason, assume not visible
		return false;
	}
}

void CNavParser::DoSlowAim(Vector& input_angle, float speed, Vector viewangles)
{
	// Yaw
	if (viewangles.y != input_angle.y)
	{
		Vector slow_delta = input_angle - viewangles;

		while (slow_delta.y > 180)
			slow_delta.y -= 360;
		while (slow_delta.y < -180)
			slow_delta.y += 360;

		slow_delta /= speed;
		input_angle = viewangles + slow_delta;

		// Clamp as we changed angles
		Math::ClampAngles(input_angle);
	}
}

void CNavParser::Map::AdjacentCost(void* main, std::vector<micropather::StateCost>* adjacent)
{
	CNavArea& tArea = *reinterpret_cast<CNavArea*>(main);
	for (NavConnect& tConnection : tArea.m_connections)
	{
		// An area being entered twice means it is blacklisted from entry entirely
		auto connection_key = std::pair<CNavArea*, CNavArea*>(tConnection.area, tConnection.area);

		// Entered and marked bad?
		if (vischeck_cache.count(connection_key) && !vischeck_cache[connection_key].vischeck_state)
			continue;

		// If the extern blacklist is running, ensure we don't try to use a bad area
		if (!free_blacklist_blocked && std::any_of(free_blacklist.begin(), free_blacklist.end(), [&](const auto& entry) { return entry.first == tConnection.area; }))
			continue;

		auto points = F::NavParser.determinePoints(&tArea, tConnection.area);

		// Apply dropdown
		points.center = F::NavParser.handleDropdown(points.center, points.next);

		float height_diff = points.center_next.z - points.center.z;

		// Too high for us to jump!
		if (height_diff > PLAYER_JUMP_HEIGHT)
			continue;

		points.current.z += PLAYER_JUMP_HEIGHT;
		points.center.z += PLAYER_JUMP_HEIGHT;
		points.next.z += PLAYER_JUMP_HEIGHT;

		const auto key = std::pair<CNavArea*, CNavArea*>(&tArea, tConnection.area);
		if (vischeck_cache.count(key))
		{
			if (vischeck_cache[key].vischeck_state)
			{
				const float cost = tConnection.area->m_center.DistToSqr(tArea.m_center);
				adjacent->push_back(micropather::StateCost{ reinterpret_cast<void*>(tConnection.area), cost });
			}
		}
		else
		{
			// Check if there is direct line of sight
			if (F::NavParser.IsPlayerPassableNavigation(points.current, points.center) &&
				F::NavParser.IsPlayerPassableNavigation(points.center, points.next))
			{
				vischeck_cache[key] = CachedConnection{ TICKCOUNT_TIMESTAMP(60), 1 };

				const float cost = points.next.DistToSqr(points.current);
				adjacent->push_back(micropather::StateCost{ reinterpret_cast<void*>(tConnection.area), cost });
			}
			else
				vischeck_cache[key] = CachedConnection{ TICKCOUNT_TIMESTAMP(60), -1 };
		}
	}
}

CNavArea* CNavParser::Map::findClosestNavSquare(const Vector& vec)
{
	const auto& pLocal = H::Entities.GetLocal();
	if (!pLocal || !pLocal->IsAlive())
		return nullptr;

	if (navfile.m_areas.empty())
		return nullptr;

	auto vec_corrected = vec;
	vec_corrected.z += PLAYER_JUMP_HEIGHT;
	float overall_best_dist = FLT_MAX, best_dist = FLT_MAX;
	// If multiple candidates for LocalNav have been found, pick the closest
	CNavArea* overall_best_square = nullptr, * best_square = nullptr;
	
	// Additional safety check for invalid vectors
	if (!std::isfinite(vec.x) || !std::isfinite(vec.y) || !std::isfinite(vec.z))
		return nullptr;
		
	try
	{
		for (auto& i : navfile.m_areas)
		{
			// Marked bad, do not use if local origin
			if (pLocal->GetAbsOrigin() == vec)
			{
				auto key = std::pair<CNavArea*, CNavArea*>(&i, &i);
				if (vischeck_cache.count(key) && vischeck_cache[key].vischeck_state == -1)
					continue;
			}

			// Check for valid center coordinates
			if (!std::isfinite(i.m_center.x) || !std::isfinite(i.m_center.y) || !std::isfinite(i.m_center.z))
				continue;

			float dist = i.m_center.DistToSqr(vec);
			// Check for NaN or other invalid values
			if (!std::isfinite(dist))
				continue;
				
			if (dist < best_dist)
			{
				best_dist = dist;
				best_square = &i;
			}

			if (overall_best_dist < dist)
				continue;

			auto center_corrected = i.m_center;
			center_corrected.z += PLAYER_JUMP_HEIGHT;

			// Check if we are within x and y bounds of an area
			// Add extra validation for IsOverlapping and visibility check
			if (!i.IsOverlapping(vec))
				continue;
				
			// Skip visibility check if already found a better match
			if (overall_best_dist < dist)
				continue;
				
			bool isVisible = false;
			try {
				isVisible = F::NavParser.IsVectorVisibleNavigation(vec_corrected, center_corrected);
			}
			catch (...) {
				// If visibility check fails, skip this area
				continue;
			}
			
			if (!isVisible)
				continue;

			overall_best_dist = dist;
			overall_best_square = &i;

			// Early return if the area is overlapping and visible
			if (overall_best_dist == best_dist)
				return overall_best_square;
		}
	}
	catch (...)
	{
		// If anything goes wrong, return nullptr
		return nullptr;
	}

	return overall_best_square ? overall_best_square : best_square;
}

void CNavParser::Map::updateIgnores()
{
	static Timer tUpdateTime;
	if (!tUpdateTime.Run(1.f))
		return;

	auto pLocal = H::Entities.GetLocal();
	if (!pLocal || !pLocal->IsAlive())
		return;

	// Clear the blacklist
	F::NavEngine.clearFreeBlacklist(BlacklistReason(BR_SENTRY));
	F::NavEngine.clearFreeBlacklist(BlacklistReason(BR_ENEMY_INVULN));
	if (Vars::NavEng::NavBot::Blacklist.Value & Vars::NavEng::NavBot::BlacklistEnum::Players)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
		{
			if (!pEntity->IsPlayer())
				continue;

			auto pPlayer = pEntity->As<CTFPlayer>();
			if (!pPlayer->IsAlive())
				continue;

			if (pPlayer->IsInvulnerable() &&
				// Cant do the funny (We are not heavy or we do not have the holiday punch equipped)
				(pLocal->m_iClass() != TF_CLASS_HEAVY || G::SavedDefIndexes[SLOT_MELEE] != Heavy_t_TheHolidayPunch))
			{
				// Get origin of the player
				auto player_origin = F::NavParser.GetDormantOrigin(pPlayer->entindex());
				if (player_origin)
				{
					player_origin.value().z += PLAYER_JUMP_HEIGHT;

					// Actual player check
					for (auto& i : navfile.m_areas)
					{
						Vector area = i.m_center;
						area.z += PLAYER_JUMP_HEIGHT;

						// Check if player is close to us
						float flDistSqr = player_origin.value().DistToSqr(area);
						if (flDistSqr <= pow(1000.0f, 2))
						{
							// Check if player can hurt us
							if (!F::NavParser.IsVectorVisibleNavigation(player_origin.value(), area, MASK_SHOT))
								continue;

							// Blacklist
							free_blacklist[&i] = BR_ENEMY_INVULN;
						}
					}
				}
			}
		}
	}

	if (Vars::NavEng::NavBot::Blacklist.Value & Vars::NavEng::NavBot::BlacklistEnum::Sentries)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_ENEMIES))
		{
			if (!pEntity->IsBuilding())
				continue;

			auto pBuilding = pEntity->As<CBaseObject>();

			if (pBuilding->GetClassID() == ETFClassID::CObjectSentrygun)
			{
				auto pSentry = pBuilding->As<CObjectSentrygun>();
				// Should we even ignore the sentry?
				if (pSentry->m_iState() != SENTRY_STATE_INACTIVE)
				{
					// Soldier/Heavy do not care about Level 1 or mini sentries
					bool is_strong_class = pLocal->m_iClass() == TF_CLASS_HEAVY || pLocal->m_iClass() == TF_CLASS_SOLDIER;
					int bullet = pSentry->m_iAmmoShells();
					int rocket = pSentry->m_iAmmoRockets();
					if (!is_strong_class || (!pSentry->m_bMiniBuilding() && pSentry->m_iUpgradeLevel() != 1) && bullet != 0 || (pSentry->m_iUpgradeLevel() == 3 && rocket != 0))
					{
						// It's still building/being sapped, ignore.
						// Unless it just was deployed from a carry, then it's dangerous
						if (pSentry->m_bCarryDeploy() || !pSentry->m_bBuilding() && !pSentry->m_bPlacing() && !pSentry->m_bHasSapper())
						{
							// Get origin of the sentry
							auto building_origin = F::NavParser.GetDormantOrigin(pSentry->entindex());
							if (!building_origin)
								continue;

							// For dormant sentries we need to add the jump height to the z
							// if ( pSentry->IsDormant( ) )
							building_origin->z += PLAYER_JUMP_HEIGHT;

							// Define range tiers for sentry danger
							const float flHighDangerRange = 900.0f; // Full blacklist
							const float flMediumDangerRange = 1050.0f; // Caution range (try to avoid)
							const float flLowDangerRange = 1200.0f; // Safe for some classes

							// Actual building check
							for (auto& i : navfile.m_areas)
							{
								Vector area = i.m_center;
								area.z += PLAYER_JUMP_HEIGHT;
								
								float flDistSqr = building_origin->DistToSqr(area);
								
								// High danger - close to sentry
								if (flDistSqr <= pow(flHighDangerRange + HALF_PLAYER_WIDTH, 2))
								{
									// Check if sentry can see us
									if (F::NavParser.IsVectorVisibleNavigation(*building_origin, area, MASK_SHOT))
									{
										// High danger - full blacklist
										free_blacklist[&i] = BR_SENTRY;
									}
								}
								// Medium danger - within sentry range but further away
								else if (flDistSqr <= pow(flMediumDangerRange + HALF_PLAYER_WIDTH, 2))
								{
									// Only blacklist if sentry can see this area
									if (F::NavParser.IsVectorVisibleNavigation(*building_origin, area, MASK_SHOT))
									{
										// Medium sentry danger - can pass through if necessary
										if (free_blacklist.find(&i) == free_blacklist.end())
										{
											// Only set to medium danger if not already high danger
											free_blacklist[&i] = BR_SENTRY_MEDIUM;
										}
									}
								}
								// Low danger - edge of sentry range (only for weak classes)
								else if (!is_strong_class && flDistSqr <= pow(flLowDangerRange + HALF_PLAYER_WIDTH, 2))
								{
									// Only blacklist if sentry can see this area
									if (F::NavParser.IsVectorVisibleNavigation(*building_origin, area, MASK_SHOT))
									{
										// Low sentry danger - can pass through if necessary
										if (free_blacklist.find(&i) == free_blacklist.end())
										{
											// Only set to low danger if not already higher danger
											free_blacklist[&i] = BR_SENTRY_LOW;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	auto stickytimestamp = TICKCOUNT_TIMESTAMP(Vars::NavEng::NavEngine::StickyIgnoreTime.Value);
	if (Vars::NavEng::NavBot::Blacklist.Value & Vars::NavEng::NavBot::BlacklistEnum::Stickies)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_PROJECTILES))
		{
			auto pSticky = pEntity->As<CTFGrenadePipebombProjectile>();
			if (pSticky->GetClassID() != ETFClassID::CTFGrenadePipebombProjectile || pSticky->m_iTeamNum() == pLocal->m_iTeamNum() || pSticky->m_iType() != TF_GL_MODE_REMOTE_DETONATE || pSticky->IsDormant() || !pSticky->m_vecVelocity().IsZero(1.f))
				continue;

			auto sticky_origin = pSticky->GetAbsOrigin();
			// Make sure the sticky doesn't vischeck from inside the floor
			sticky_origin.z += PLAYER_JUMP_HEIGHT / 2.0f;
			for (auto& i : navfile.m_areas)
			{
				Vector area = i.m_center;
				area.z += PLAYER_JUMP_HEIGHT;

				// Out of range
				if (sticky_origin.DistToSqr(area) <= pow(130.0f + HALF_PLAYER_WIDTH, 2))
				{
					CGameTrace trace = {};
					CTraceFilterProjectile filter = {};
					SDK::Trace(sticky_origin, area, MASK_SHOT, &filter, &trace);

					// Check if Sticky can see the reason
					if (trace.fraction == 1.0f)
						free_blacklist[&i] = { BR_STICKY, stickytimestamp };
					// Blacklist because it's in range of the sticky, but stickies make no noise, so blacklist it for a specific timeframe
				}
			}
		}
	}

	static size_t previous_blacklist_size = 0;

	bool erased = previous_blacklist_size != free_blacklist.size();
	previous_blacklist_size = free_blacklist.size();

	std::erase_if(free_blacklist, [](const auto& entry) { return entry.second.time && entry.second.time < I::GlobalVars->tickcount; });
	std::erase_if(vischeck_cache, [](const auto& entry) { return entry.second.expire_tick < I::GlobalVars->tickcount; });
	std::erase_if(connection_stuck_time, [](const auto& entry) { return entry.second.expire_tick < I::GlobalVars->tickcount; });

	if (erased)
		pather.Reset();
}

void CNavParser::Map::UpdateRespawnRooms()
{
	std::vector<CFuncRespawnRoom*> vFoundEnts;
	CServerBaseEntity* pPrevEnt = nullptr;
	while (true)
	{
		auto pEntity = I::ServerTools->FindEntityByClassname(pPrevEnt, "func_respawnroom");
		if (!pEntity)
			break;

		pPrevEnt = pEntity;

		vFoundEnts.push_back(pEntity->As<CFuncRespawnRoom>());
	}

	if (vFoundEnts.empty())
	{
		SDK::Output("CNavParser::Map::UpdateRespawnRooms", std::format("Couldn't find any room entities").c_str(), { 255, 50, 50 }, Vars::Debug::Logging.Value);
		return;
	}

	for (auto pRespawnRoom : vFoundEnts)
	{
		if (!pRespawnRoom)
			continue;

		static Vector vStepHeight(0.0f, 0.0f, 18.0f);
		for (auto& tArea : navfile.m_areas)
		{
			if (pRespawnRoom->PointIsWithin(tArea.m_center + vStepHeight)
				|| pRespawnRoom->PointIsWithin(tArea.m_nwCorner + vStepHeight)
				|| pRespawnRoom->PointIsWithin(tArea.getNeCorner() + vStepHeight)
				|| pRespawnRoom->PointIsWithin(tArea.getSwCorner() + vStepHeight)
				|| pRespawnRoom->PointIsWithin(tArea.m_seCorner + vStepHeight))
			{
				uint32_t uFlags = pRespawnRoom->m_iTeamNum() == TF_TEAM_BLUE ? TF_NAV_SPAWN_ROOM_BLUE : TF_NAV_SPAWN_ROOM_RED;
				if (!(tArea.m_TFattributeFlags & uFlags))
					tArea.m_TFattributeFlags |= uFlags;
			}
		}
	}
}

bool CNavParser::IsPlayerPassableNavigation(Vector origin, Vector target, unsigned int mask)
{
	if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z) ||
		!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z))
		return false;
	
	if (origin.DistToSqr(target) < 1.0f)
		return true;
	
	if (origin.DistToSqr(target) > pow(2000.0f, 2))
		return false;
	
	Vector tr = target - origin;
	float height_diff = tr.z;
	
	if (height_diff < -PLAYER_JUMP_HEIGHT * 1.2f)
	{
		Vector ground_check = target;
		ground_check.z -= 100.0f;
		
		trace_t ground_trace;
		CTraceFilterHitscan ground_filter{};
		SDK::Trace(target, ground_check, mask, &ground_filter, &ground_trace);
		
		if (ground_trace.fraction > 0.7f)
			return false;
	}
	else if (height_diff > PLAYER_JUMP_HEIGHT * 0.95f)
	{
		return false;
	}
	
	Vector angles;
	Math::VectorAngles(tr, angles);

	Vector forward, right;
	Math::AngleVectors(angles, &forward, &right, nullptr);
	right.z = 0;

	float tr_length = tr.Length();
	Vector relative_endpos = forward * tr_length;
	
	float adjusted_half_width = HALF_PLAYER_WIDTH * 0.8f;
	
	trace_t trace;
	CTraceFilterNavigation Filter{};

	Vector left_ray_origin = origin - right * adjusted_half_width;
	Vector left_ray_endpos = left_ray_origin + relative_endpos;
	SDK::Trace(left_ray_origin, left_ray_endpos, mask, &Filter, &trace);

	// Left ray hit something
	if (trace.DidHit())
		return false;

	Vector right_ray_origin = origin + right * adjusted_half_width;
	Vector right_ray_endpos = right_ray_origin + relative_endpos;
	SDK::Trace(right_ray_origin, right_ray_endpos, mask, &Filter, &trace);

	if (trace.DidHit())
		return false;
		
	SDK::Trace(origin, origin + relative_endpos, mask, &Filter, &trace);
	
	if (trace.DidHit())
		return false;
		
	if (tr_length > 100.0f)
	{
		Vector top_ray_origin = origin;
		top_ray_origin.z += PLAYER_HEIGHT * 0.7f;
		Vector top_ray_endpos = top_ray_origin + relative_endpos;
		
		SDK::Trace(top_ray_origin, top_ray_endpos, mask, &Filter, &trace);
		
		if (trace.DidHit())
			return false;
	}
	
	return true;
}

Vector CNavParser::GetForwardVector(Vector origin, Vector viewangles, float distance)
{
	Vector forward;
	float sp, sy, cp, cy;
	const QAngle angle = viewangles;

	Math::SinCos(DEG2RAD(angle[1]), &sy, &cy);
	Math::SinCos(DEG2RAD(angle[0]), &sp, &cp);
	forward.x = cp * cy;
	forward.y = cp * sy;
	forward.z = -sp;
	forward = forward * distance + origin;
	return forward;
}

Vector CNavParser::handleDropdown(Vector current_pos, Vector next_pos)
{
	Vector to_target = (next_pos - current_pos);
	float height_diff = to_target.z;

	// Handle drops more carefully
	if (height_diff < 0)
	{
		float drop_distance = -height_diff;

		// Small drops (less than jump height) - no special handling needed
		if (drop_distance <= PLAYER_JUMP_HEIGHT)
			return current_pos;

		// Medium drops - move out a bit to prevent getting stuck
		if (drop_distance <= PLAYER_HEIGHT)
		{
			to_target.z = 0;
			to_target.Normalize();
			QAngle angles;
			Math::VectorAngles(to_target, angles);
			Vector vec_angles(angles.x, angles.y, angles.z);
			return GetForwardVector(current_pos, vec_angles, PLAYER_WIDTH * 1.5f);
		}
		// Large drops - move out significantly to prevent fall damage
		to_target.z = 0;
		to_target.Normalize();
		QAngle angles;
		Math::VectorAngles(to_target, angles);
		Vector vec_angles(angles.x, angles.y, angles.z);
		return GetForwardVector(current_pos, vec_angles, PLAYER_WIDTH * 2.5f);
	}
	// Handle upward movement
	else if (height_diff > 0)
	{
		// If it's within jump height, move closer to help with the jump
		if (height_diff <= PLAYER_JUMP_HEIGHT)
		{
			to_target.z = 0;
			to_target.Normalize();
			QAngle angles;
			Math::VectorAngles(-to_target, angles);
			Vector vec_angles(angles.x, angles.y, angles.z);
			return GetForwardVector(current_pos, vec_angles, PLAYER_WIDTH * 0.5f);
		}
	}

	return current_pos;
}

NavPoints CNavParser::determinePoints(CNavArea* current, CNavArea* next)
{
	auto area_center = current->m_center;
	auto next_center = next->m_center;
	// Gets a vector on the edge of the current area that is as close as possible to the center of the next area
	auto area_closest = current->getNearestPoint(Vector2D(next_center.x, next_center.y));
	// Do the same for the other area
	auto next_closest = next->getNearestPoint(Vector2D(area_center.x, area_center.y));

	// Use one of them as a center point, the one that is either x or y alligned with a center
	// Of the areas. This will avoid walking into walls.
	auto center_point = area_closest;

	// Determine if alligned, if not, use the other one as the center point
	if (center_point.x != area_center.x && center_point.y != area_center.y && center_point.x != next_center.x && center_point.y != next_center.y)
	{
		center_point = next_closest;
		// Use the point closest to next_closest on the "original" mesh for z
		center_point.z = current->getNearestPoint(Vector2D(next_closest.x, next_closest.y)).z;
	}

	// Nearest point to center on "next", used for height checks
	auto center_next = next->getNearestPoint(Vector2D(center_point.x, center_point.y));

	return NavPoints(area_center, center_point, center_next, next_center);
}

static Timer inactivity;
static Timer time_spent_on_crumb;
bool CNavEngine::navTo(const Vector& destination, int priority, bool should_repath, bool nav_to_local, bool is_repath)
{
	const auto& pLocal = H::Entities.GetLocal();
	if (!pLocal || !pLocal->IsAlive() || F::Ticks.m_bWarp || F::Ticks.m_bDoubletap)
		return false;

	if (!isReady())
		return false;

	// Don't path, priority is too low
	if (priority < current_priority)
		return false;

	CNavArea* start_area = map->findClosestNavSquare(pLocal->GetAbsOrigin());
	CNavArea* dest_area = map->findClosestNavSquare(destination);

	if (!start_area || !dest_area)
		return false;

	auto path = map->findPath(start_area, dest_area);
	if (path.empty())
		return false;

	if (!nav_to_local)
		path.erase(path.begin());

	crumbs.clear();

	for (size_t i = 0; i < path.size(); i++)
	{
		auto area = reinterpret_cast<CNavArea*>(path.at(i));
		if (!area)
			continue;

		// All entries besides the last need an extra crumb
		if (i != path.size() - 1)
		{
			auto next_area = reinterpret_cast<CNavArea*>(path.at(i + 1));

			auto points = F::NavParser.determinePoints(area, next_area);

			points.center = F::NavParser.handleDropdown(points.center, points.next);

			crumbs.push_back({ area, points.current });
			crumbs.push_back({ area, points.center });
		}
		else
			crumbs.push_back({ area, area->m_center });
	}

	crumbs.push_back({ nullptr, destination });
	
	// Smooth the path to avoid zigzags and sharp turns
	smoothPath();
	
	inactivity.Update();

	current_priority = priority;
	current_navtolocal = nav_to_local;
	repath_on_fail = should_repath;
	// Ensure we know where to go
	if (repath_on_fail)
		last_destination = destination;

	return true;
}

// Smooths the path by removing unnecessary zigzags and creating more direct routes
void CNavEngine::smoothPath()
{
	if (crumbs.size() < 3)
		return;
	
	std::vector<CNavParser::Crumb> smoothed_crumbs;
	smoothed_crumbs.push_back(crumbs.front());
	
	const auto pLocal = H::Entities.GetLocal();
	if (!pLocal || !pLocal->IsAlive())
		return;
	
	for (size_t i = 0; i < crumbs.size() - 1; i++)
	{
		Vector current = crumbs[i].vec;
		current.z += PLAYER_JUMP_HEIGHT;
		
		const size_t max_look_ahead = 3;
		size_t furthest_visible = i + 1;
		
		for (size_t j = i + 2; j < crumbs.size() && j <= i + max_look_ahead; j++)
		{
			Vector target = crumbs[j].vec;
			target.z += PLAYER_JUMP_HEIGHT;
			
			// Check if we can directly see this point and path to it
			if (F::NavParser.IsPlayerPassableNavigation(current, target))
			{
				furthest_visible = j;
			}
			else
			{
				// Can't see further, stop looking
				break;
			}
		}
		
		// Only skip points if we can see at least 2 ahead
		if (furthest_visible > i + 1)
		{
			// Add intermediate points for smoother movement, keeping at least
			// one intermediate crumb for every two we skip
			size_t steps = furthest_visible - i;
			size_t points_to_add = steps / 2;
			
			// Always add at least one intermediate point for large jumps
			if (steps >= 3 && points_to_add < 1)
				points_to_add = 1;
				
			// Add intermediate points
			for (size_t k = 1; k <= points_to_add; k++)
			{
				size_t idx = i + (k * steps) / (points_to_add + 1);
				if (idx < furthest_visible && idx > i)
				{
					// Don't add duplicate points
					if (smoothed_crumbs.empty() || smoothed_crumbs.back().vec != crumbs[idx].vec)
						smoothed_crumbs.push_back(crumbs[idx]);
				}
			}
			
			// Skip to the furthest visible point
			i = furthest_visible - 1;
		}
		
		// Add the furthest visible point
		if (furthest_visible < crumbs.size() && (smoothed_crumbs.empty() || smoothed_crumbs.back().vec != crumbs[furthest_visible].vec))
		{
			smoothed_crumbs.push_back(crumbs[furthest_visible]);
		}
	}
	
	// Make sure we keep the final destination
	if (smoothed_crumbs.back().vec != crumbs.back().vec)
	{
		smoothed_crumbs.push_back(crumbs.back());
	}
	
	// Replace the original path with the smoothed one only if it has enough points
	if (smoothed_crumbs.size() > 1 && smoothed_crumbs.size() >= crumbs.size() / 3)
	{
		crumbs = smoothed_crumbs;
	}
}

void CNavEngine::vischeckPath()
{
	static Timer vischeck_timer{};
	// No crumbs to check, or vischeck timer should not run yet, bail.
	if (crumbs.size() < 2 || !vischeck_timer.Run(Vars::NavEng::NavEngine::VischeckTime.Value))
		return;

	const auto timestamp = TICKCOUNT_TIMESTAMP(Vars::NavEng::NavEngine::VischeckCacheTime.Value);
	
	// Use static counter to implement incremental checking (don't check all crumbs at once)
	static size_t check_index = 0;
	static constexpr size_t MAX_CHECKS_PER_FRAME = 3; // Limit checks per frame to reduce performance impact
	
	// Reset check index if it's out of bounds
	if (check_index >= crumbs.size() - 1)
		check_index = 0;
	
	// Get local player for advanced checks
	auto pLocal = H::Entities.GetLocal();
	if (!pLocal || !pLocal->IsAlive())
		return;
	
	Vector vLocalOrigin = pLocal->GetAbsOrigin();
	size_t checks_done = 0;
	bool path_invalid = false;

	// Check up to MAX_CHECKS_PER_FRAME crumbs per frame or until we find an invalid path
	for (size_t i = 0; i < std::min(crumbs.size() - 1, MAX_CHECKS_PER_FRAME) && !path_invalid; i++)
	{
		// Calculate which crumb pair to check
		size_t current_idx = (check_index + i) % (crumbs.size() - 1);
		auto it = crumbs.begin() + current_idx;
		auto next = it + 1;
		
		if (next == crumbs.end())
			continue;
		
		auto current_crumb = *it;
		auto next_crumb = *next;
		
		// Skip checking pairs that are too far from the player
		float dist_to_current = vLocalOrigin.DistToSqr(current_crumb.vec);
		if (dist_to_current > pow(1500.0f, 2))
			continue;
			
		auto key = std::pair<CNavArea*, CNavArea*>(current_crumb.navarea, next_crumb.navarea);

		// Skip if we already have a valid cache entry
		if (map->vischeck_cache.count(key) && 
			map->vischeck_cache[key].expire_tick > I::GlobalVars->tickcount)
		{
			// If it's already known to be invalid, abort pathing
			if (map->vischeck_cache[key].vischeck_state <= 0)
			{
				path_invalid = true;
				break;
			}
			continue;
		}

		auto current_center = current_crumb.vec;
		current_center.z += PLAYER_JUMP_HEIGHT;

		auto next_center = next_crumb.vec;
		next_center.z += PLAYER_JUMP_HEIGHT;
		
		// Check if there's a height difference and adjust checking strategy
		float height_diff = next_center.z - current_center.z;
		bool is_steep = std::abs(height_diff) > PLAYER_JUMP_HEIGHT / 2.0f;
		
		try
		{
			bool is_passable = false;
			
			// For steep paths or long distances, use more thorough checking
			if (is_steep || current_center.DistToSqr(next_center) > pow(200.0f, 2))
			{
				// Use multiple intermediate points for more accurate checking of long or steep paths
				Vector dir = next_center - current_center;
				float dist = dir.Length();
				dir.Normalize();
				
				const int check_points = 3; // Number of intermediate points to check
				bool all_points_passable = true;
				
				for (int j = 1; j < check_points && all_points_passable; j++)
				{
					Vector check_point = current_center + dir * (dist * j / check_points);
					all_points_passable = F::NavParser.IsPlayerPassableNavigation(current_center, check_point) &&
										 F::NavParser.IsPlayerPassableNavigation(check_point, next_center);
				}
				
				is_passable = all_points_passable;
			}
			else
			{
				// Simple direct check for short, flat paths
				is_passable = F::NavParser.IsPlayerPassableNavigation(current_center, next_center);
			}
			
			// Check if we can pass, if not, abort pathing and mark as bad
			if (!is_passable)
			{
				// Mark as invalid for a while
				map->vischeck_cache[key] = CNavParser::CachedConnection{ timestamp, -1 };
				path_invalid = true;
			}
			// Else we can update the cache (if not marked bad before this)
			else if (!map->vischeck_cache.count(key) || map->vischeck_cache[key].vischeck_state != -1)
			{
				map->vischeck_cache[key] = CNavParser::CachedConnection{ timestamp, 1 };
			}
		}
		catch (...)
		{
			// Handle exceptions during visibility check - mark as unknown but don't fail the path
			map->vischeck_cache[key] = CNavParser::CachedConnection{ TICKCOUNT_TIMESTAMP(10), 0 };
			SDK::Output("CNavEngine", "Exception during visibility check", { 255, 100, 100 }, Vars::Debug::Logging.Value);
		}
		
		checks_done++;
	}
	
	// Update the check index for next time
	check_index = (check_index + checks_done) % (crumbs.size() - 1);
	
	// If we found an invalid path segment, abandon the path
	if (path_invalid)
		abandonPath();
}

static Timer blacklist_check_timer{};
// Check if one of the crumbs is suddenly blacklisted
void CNavEngine::checkBlacklist()
{
	// Only check every 500ms
	if (!blacklist_check_timer.Run(0.5f))
		return;

	auto pLocal = H::Entities.GetLocal();
	if (!pLocal || !pLocal->IsAlive())
		return;

	// Local player is ubered and does not care about the blacklist
	// TODO: Only for damage type things
	if (pLocal->IsInvulnerable())
	{
		map->free_blacklist_blocked = true;
		map->pather.Reset();
		return;
	}
	auto origin = pLocal->GetAbsOrigin();

	auto local_area = map->findClosestNavSquare(origin);
	for (const auto& entry : map->free_blacklist)
	{
		// Local player is in a blocked area, so temporarily remove the blacklist as else we would be stuck
		if (entry.first == local_area)
		{
			map->free_blacklist_blocked = true;
			map->pather.Reset();
			return;
		}
	}

	// Local player is not blocking the nav area, so blacklist should not be marked as blocked
	map->free_blacklist_blocked = false;

	bool should_abandon = false;
	for (auto& crumb : crumbs)
	{
		if (should_abandon)
			break;
		// A path Node is blacklisted, abandon pathing
		for (const auto& entry : map->free_blacklist)
			if (entry.first == crumb.navarea)
				should_abandon = true;
	}
	if (should_abandon)
		abandonPath();
}

void CNavEngine::updateStuckTime()
{
	// No crumbs
	if (crumbs.empty())
		return;

	// We're stuck, add time to connection
	if (inactivity.Check(Vars::NavEng::NavEngine::StuckTime.Value / 2))
	{
		std::pair<CNavArea*, CNavArea*> key = last_crumb.navarea ? std::pair<CNavArea *, CNavArea *>(last_crumb.navarea, crumbs[0].navarea) : std::pair<CNavArea *, CNavArea *>(crumbs[0].navarea, crumbs[0].navarea);

		// Expires in 10 seconds
		map->connection_stuck_time[key].expire_tick = TICKCOUNT_TIMESTAMP(Vars::NavEng::NavEngine::StuckExpireTime.Value);
		// Stuck for one tick
		map->connection_stuck_time[key].time_stuck += 1;

		// We are stuck for too long, blastlist node for a while and repath
		if (map->connection_stuck_time[key].time_stuck > TIME_TO_TICKS(Vars::NavEng::NavEngine::StuckDetectTime.Value))
		{
			const auto expire_tick = TICKCOUNT_TIMESTAMP(Vars::NavEng::NavEngine::StuckBlacklistTime.Value);
			SDK::Output("CNavEngine", std::format("Stuck for too long, blacklisting the node (expires on tick: {})", expire_tick).c_str(), { 255, 131, 131 }, Vars::Debug::Logging.Value, Vars::Debug::Logging.Value);
			map->vischeck_cache[key].expire_tick = expire_tick;
			map->vischeck_cache[key].vischeck_state = 0;
			abandonPath();
		}
	}
}

void CNavEngine::Reset(bool bForced)
{
	abandonPath();

	const auto level_name = I::EngineClient->GetLevelName();
	if (level_name)
	{
		if (map)
			map->Reset();

		if (bForced || !map || map->mapname != level_name)
		{
			char* p, cwd[MAX_PATH + 1], lvl_name[256];
			std::string nav_path;
			strncpy_s(lvl_name, level_name, 255);
			lvl_name[255] = 0;
			p = std::strrchr(lvl_name, '.');
			if (!p)
				return;

			*p = 0;

			p = _getcwd(cwd, sizeof(cwd));
			if (!p)
				return;

			nav_path = std::format("{}/tf/{}.nav", cwd, lvl_name);
			SDK::Output("NavEngine", std::format("Nav File location: {}", nav_path).c_str(), { 50, 255, 50 }, Vars::Debug::Logging.Value);
			map = std::make_unique<CNavParser::Map>(nav_path.c_str());
		}
	}
}

bool CNavEngine::isReady(bool bRoundCheck)
{
	static Timer tRestartTimer{};
	if (!Vars::NavEng::NavEngine::Enabled.Value)
	{
		tRestartTimer.Update();
		return false;
	}

	auto pLocal = H::Entities.GetLocal();
	// Too early, the engine might not fully restart yet.
	if (!tRestartTimer.Check(0.5f) || !pLocal || !pLocal->IsAlive())
		return false;

	if (!I::EngineClient->IsInGame())
		return false;

	if (!map || map->state != CNavParser::NavState::Active)
		return false;

	if (!bRoundCheck && F::NavParser.IsSetupTime())
		return false;

	return true;
}

bool CNavEngine::findNearestNavNode(CTFPlayer* pLocal)
{
	if (!pLocal || !pLocal->IsAlive() || !map || !isReady() || !I::EngineClient || !I::EngineClient->IsInGame())
		return false;

	if (isPathing())
		return false;

	Vector vLocalOrigin = pLocal->GetAbsOrigin();

	CNavArea* pClosestNav = map->findClosestNavSquare(vLocalOrigin);
	if (!pClosestNav)
		return false;

	if (pClosestNav->IsOverlapping(vLocalOrigin))
		return false;

	Vector vDestination = pClosestNav->m_center;

	SDK::Output("CNavEngine", "Bot is not on any navmesh, navigating to nearest node", { 255, 200, 100 }, Vars::Debug::Logging.Value, Vars::Debug::Logging.Value);
	return navTo(vDestination, Priority_list::danger, true, true, false);
}

void CNavEngine::Run(CUserCmd* pCmd)
{
	static bool bWasOn = false;
	if (!Vars::NavEng::NavEngine::Enabled.Value)
		bWasOn = false;
	else if (I::EngineClient->IsInGame() && !bWasOn)
	{
		bWasOn = true;
		Reset(true);
	}

	auto pLocal = H::Entities.GetLocal();
	if (!pLocal /*|| F::Ticks.m_bRecharge*/)
		return;

	if (!pLocal->IsAlive())
	{
		cancelPath();
		return;
	}

	if ((current_priority == engineer && ((!Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value && !Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value) || pLocal->m_iClass() != TF_CLASS_ENGINEER)) ||
		(current_priority == capture && !(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::CaptureObjectives)))
	{
		cancelPath();
		return;
	}

	if (!pCmd || (pCmd->buttons & (IN_FORWARD | IN_BACK | IN_MOVERIGHT | IN_MOVELEFT) && !F::Misc.m_bAntiAFK)
		|| !isReady(true))
		return;

	// Still in setup. If on fitting team and map, do not path yet.
	if (F::NavParser.IsSetupTime())
	{
		cancelPath();
		return;
	}

	if (Vars::NavEng::NavEngine::VischeckEnabled.Value && !F::Ticks.m_bWarp && !F::Ticks.m_bDoubletap)
		vischeckPath();

	checkBlacklist();

	if (!isPathing())
		findNearestNavNode(pLocal);

	followCrumbs(pLocal, pCmd);

	updateStuckTime();
	map->updateIgnores();
}

void CNavEngine::abandonPath()
{
	if (!map)
		return;

	map->pather.Reset();
	crumbs.clear();
	last_crumb.navarea = nullptr;
	// We want to repath on failure
	if (repath_on_fail)
		navTo(last_destination, current_priority, true, current_navtolocal, false);
	else
		current_priority = 0;
}

void CNavEngine::cancelPath()
{
	crumbs.clear();
	last_crumb.navarea = nullptr;
	current_priority = 0;
}

bool CanJumpIfScoped(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	// You can still jump if youre scoped in water
	if (pLocal->m_fFlags() & FL_INWATER)
		return true;

	auto iWeaponID = pWeapon->GetWeaponID();
	return iWeaponID == TF_WEAPON_SNIPERRIFLE_CLASSIC ? !pWeapon->As<CTFSniperRifleClassic>()->m_bCharging() : !pLocal->InCond(TF_COND_ZOOMED);
}

void LookAtPath(CUserCmd* pCmd, const Vec2 vDest, const Vec3 vLocalEyePos, bool bSilent)
{
	static Vec3 LastAngles{};
	Vec3 next{ vDest.x, vDest.y, vLocalEyePos.z };
	next = Math::CalcAngle(vLocalEyePos, next);

	const int aim_speed = 25; // how smooth nav is/ im cringing at this damn speed.
	// activate nav spin and smoothen
	F::NavParser.DoSlowAim(next, aim_speed, LastAngles);
	if (bSilent)
		pCmd->viewangles = next;
	else
		I::EngineClient->SetViewAngles(next);
	LastAngles = next;
}

void CNavEngine::followCrumbs(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	static Timer tLastJump;
	static int iTicksSinceJump{ 0 };
	static bool bCrouch{ false };
	static Vector vPreviousPosition = Vector(0, 0, 0);
	static Timer tStuckCheck{};
	static bool bWasStuck = false;
	static Vector vAvoidDir = Vector(0, 0, 0);
	static Timer tAvoidTimer{};

	size_t crumbs_amount = crumbs.size();
	// No more crumbs, reset status
	if (!crumbs_amount)
	{
		// Invalidate last crumb
		last_crumb.navarea = nullptr;
		vPreviousPosition = Vector(0, 0, 0);
		repath_on_fail = false;
		current_priority = 0;
		return;
	}

	if (current_crumb.navarea != crumbs[0].navarea)
		time_spent_on_crumb.Update();
	current_crumb = crumbs[0];

	// Ensure we do not try to walk downwards unless we are falling
	static std::vector<float> fall_vec{};
	Vector vel = pLocal->GetAbsVelocity();

	fall_vec.push_back(vel.z);
	if (fall_vec.size() > 10)
		fall_vec.erase(fall_vec.begin());

	bool reset_z = true;
	for (const auto& entry : fall_vec)
	{
		if (!(entry <= 0.01f && entry >= -0.01f))
			reset_z = false;
	}

	const auto vLocalOrigin = pLocal->GetAbsOrigin();
	if (!F::NavEngine.IsNavMeshLoaded())
		return;

	if (reset_z && !F::Ticks.m_bWarp && !F::Ticks.m_bDoubletap)
	{
		reset_z = false;

		Vector end = vLocalOrigin;
		end.z -= 100.0f;

		CGameTrace trace;
		CTraceFilterHitscan Filter{};
		Filter.pSkip = pLocal;
		SDK::TraceHull(vLocalOrigin, end, pLocal->m_vecMins(), pLocal->m_vecMaxs(), MASK_PLAYERSOLID, &Filter, &trace);

		// Only reset if we are standing on a building
		if (trace.DidHit() && trace.m_pEnt && trace.m_pEnt->IsBuilding())
			reset_z = true;
	}

	Vector current_vec = crumbs[0].vec;
	if (reset_z)
		current_vec.z = vLocalOrigin.z;

	// Check if we're stuck by comparing position with previous tick
	bool bCurrentlyStuck = false;
	if (vPreviousPosition.DistToSqr(vLocalOrigin) < 1.0f && vel.Length2D() < 10.0f && tStuckCheck.Check(0.2f))
	{
		bCurrentlyStuck = true;

		// Only trigger stuck behavior if we've been stuck for a short time
		if (!bWasStuck)
		{
			bWasStuck = true;
			
			float angle = SDK::RandomFloat(0.0f, 2.0f * PI);
			vAvoidDir = Vector(cos(angle), sin(angle), 0.0f);
			tAvoidTimer.Update();
		}
	}
	else if (vel.Length2D() > 50.0f)
	{
		bWasStuck = false;
	}

	vPreviousPosition = vLocalOrigin;

	// We are close enough to the crumb to have reached it
	if (current_vec.DistToSqr(vLocalOrigin) < pow(50.0f, 2))
	{
		last_crumb = crumbs[0];
		crumbs.erase(crumbs.begin());
		time_spent_on_crumb.Update();
		if (!--crumbs_amount)
			return;
		inactivity.Update();
	}

	current_vec = crumbs[0].vec;
	if (reset_z)
		current_vec.z = vLocalOrigin.z;

	// We are close enough to the second crumb, Skip both (This is especially helpful with drop-downs)
	if (crumbs.size() > 1 && crumbs[1].vec.DistToSqr(vLocalOrigin) < pow(50.0f, 2))
	{
		last_crumb = crumbs[1];
		crumbs.erase(crumbs.begin(), std::next(crumbs.begin()));
		time_spent_on_crumb.Update();
		--crumbs_amount;
		if (!--crumbs_amount)
			return;
		inactivity.Update();
	}
	// If we make any progress at all, reset this
	// If we spend way too long on this crumb, ignore the logic below
	else if (!time_spent_on_crumb.Check(Vars::NavEng::NavEngine::StuckDetectTime.Value))
	{
		// 44.0f -> Revved brass beast, do not use z axis as jumping counts towards that. Yes this will mean long falls will trigger it, but that is not really bad.
		if (!vel.Get2D().IsZero(40.0f))
			inactivity.Update();
		else
			SDK::Output("CNavEngine", std::format("Spent too much time on the crumb, assuming were stuck, 2Dvelocity: ({},{})", fabsf(vel.Get2D().x), fabsf(vel.Get2D().y)).c_str(), { 255, 131, 131 }, Vars::Debug::Logging.Value, Vars::Debug::Logging.Value);
	}

	auto pWeapon = pLocal->m_hActiveWeapon().Get()->As<CTFWeaponBase>();
	{
		// Detect when jumping is necessary.
		// 1. No jumping if zoomed (or revved)
		// 2. Jump if it's necessary to do so based on z values
		// 3. Jump if stuck (not getting closer) for more than stuck_time/2
		if (pWeapon)
		{
			auto iWepID = pWeapon->GetWeaponID();
			if ((iWepID != TF_WEAPON_SNIPERRIFLE &&
				iWepID != TF_WEAPON_SNIPERRIFLE_CLASSIC &&
				iWepID != TF_WEAPON_SNIPERRIFLE_DECAP) ||
				CanJumpIfScoped(pLocal, pWeapon))
			{
				if (iWepID != TF_WEAPON_MINIGUN || !(pCmd->buttons & IN_ATTACK2))
				{
					bool bShouldJump = false;
					float height_diff = crumbs[0].vec.z - pLocal->GetAbsOrigin().z;

					bool bPreventJump = false;
					if (crumbs.size() > 1)
					{
						float next_height_diff = crumbs[0].vec.z - crumbs[1].vec.z;
						if (next_height_diff < 0 && next_height_diff <= -PLAYER_JUMP_HEIGHT)
							bPreventJump = true;
					}
					if (!bPreventJump)
					{
						// Check if we need to jump
						if (height_diff > pLocal->m_flStepSize() && height_diff <= PLAYER_JUMP_HEIGHT)
							bShouldJump = true;
						// Also jump if we're stuck and it might help
						else if (inactivity.Check(Vars::NavEng::NavEngine::StuckTime.Value / 2) || bCurrentlyStuck)
						{
							auto pLocalNav = map->findClosestNavSquare(pLocal->GetAbsOrigin());
							if (pLocalNav && !(pLocalNav->m_attributeFlags & (NAV_MESH_NO_JUMP | NAV_MESH_STAIRS)))
								bShouldJump = true;
						}
					}
					if (bShouldJump && tLastJump.Check(0.2f))
					{
						// Make it crouch until we land, but jump the first tick
						pCmd->buttons |= bCrouch ? IN_DUCK : IN_JUMP;

						// Only flip to crouch state, not to jump state
						if (!bCrouch)
						{
							bCrouch = true;
							iTicksSinceJump = 0;
						}
						iTicksSinceJump++;

						// Update jump timer now since we are back on ground
						if (bCrouch && pLocal->OnSolid() && iTicksSinceJump > 3)
						{
							// Reset
							bCrouch = false;
							tLastJump.Update();
						}
					}
				}
			}
		}
	}

	const auto vLocalEyePos = pLocal->GetEyePosition();

	if (G::Attacking != 1)
	{
		// Look at path (nav spin) (smooth nav)
		switch (Vars::NavEng::NavEngine::LookAtPath.Value)
		{
		case Vars::NavEng::NavEngine::LookAtPathEnum::Off:
			break;
		case Vars::NavEng::NavEngine::LookAtPathEnum::Silent:
			if (G::AntiAim)
				break;
			[[fallthrough]];
		case Vars::NavEng::NavEngine::LookAtPathEnum::Plain:
		default:
			LookAtPath(pCmd, { crumbs[0].vec.x, crumbs[0].vec.y }, vLocalEyePos, Vars::NavEng::NavEngine::LookAtPath.Value == Vars::NavEng::NavEngine::LookAtPathEnum::Silent);
			break;
		}
	}

	if (bWasStuck && !tAvoidTimer.Check(0.5f))
	{
		Vector vMoveDirection = vAvoidDir;
		
		pCmd->forwardmove = vMoveDirection.x * 450.0f;
		pCmd->sidemove = vMoveDirection.y * 450.0f;
		
		if (pLocal->OnSolid() && tLastJump.Check(0.4f))
		{
			pCmd->buttons |= IN_JUMP;
			tLastJump.Update();
		}
	}
	else
	{
		SDK::WalkTo(pCmd, pLocal, current_vec);
	}
}

void CNavEngine::Render()
{
	if (!Vars::NavEng::NavEngine::Draw.Value || !isReady())
		return;

	auto pLocal = H::Entities.GetLocal();
	if (!pLocal || !pLocal->IsAlive() || !map)
		return;

	/*if (!F::NavBot.m_vSlightDangerDrawlistNormal.empty())
	{
		for (auto vPos : F::NavBot.m_vSlightDangerDrawlistNormal)
		{
			RenderBox(vPos, Vector(-4.0f, -4.0f, -1.0f), Vector(4.0f, 4.0f, 1.0f), Vector(), Color_t(255, 150, 0, 255), Color_t(255, 150, 0, 255), false);
		}
	}

	if (!F::NavBot.m_vSlightDangerDrawlistDormant.empty())
	{
		for (auto vPos : F::NavBot.m_vSlightDangerDrawlistDormant)
		{
			RenderBox(vPos, Vector(-4.0f, -4.0f, -1.0f), Vector(4.0f, 4.0f, 1.0f), Vector(), Color_t(255, 150, 0, 255), Color_t(255, 150, 0, 255), false);
		}
	}*/

	if (Vars::NavEng::NavEngine::Draw.Value & Vars::NavEng::NavEngine::DrawEnum::Blacklist)
	{
		if (auto pBlacklist = getFreeBlacklist())
		{
			if (!pBlacklist->empty())
			{
				for (auto& tBlacklistedArea : *pBlacklist)
				{
					H::Draw.RenderBox(tBlacklistedArea.first->m_center, Vector(-4.0f, -4.0f, -1.0f), Vector(4.0f, 4.0f, 1.0f), Vector(), Vars::Colors::NavbotBlacklist.Value, false);
					H::Draw.RenderWireframeBox(tBlacklistedArea.first->m_center, Vector(-4.0f, -4.0f, -1.0f), Vector(4.0f, 4.0f, 1.0f), Vector(), Vars::Colors::NavbotBlacklist.Value, false);
				}
			}
		}
	}

	if (Vars::NavEng::NavEngine::Draw.Value & Vars::NavEng::NavEngine::DrawEnum::Area)
	{
		Vector vOrigin = pLocal->GetAbsOrigin();
		auto pArea = map->findClosestNavSquare(vOrigin);
		auto vEdge = pArea->getNearestPoint(Vector2D(vOrigin.x, vOrigin.y));
		vEdge.z += PLAYER_JUMP_HEIGHT;
		H::Draw.RenderBox(vEdge, Vector(-4.0f, -4.0f, -1.0f), Vector(4.0f, 4.0f, 1.0f), Vector(), Color_t(255, 0, 0, 255), false);
		H::Draw.RenderWireframeBox(vEdge, Vector(-4.0f, -4.0f, -1.0f), Vector(4.0f, 4.0f, 1.0f), Vector(), Color_t(255, 0, 0, 255), false);

		// Nw -> Ne
		H::Draw.RenderLine(pArea->m_nwCorner, pArea->getNeCorner(), Vars::Colors::NavbotArea.Value, true);
		// Nw -> Sw
		H::Draw.RenderLine(pArea->m_nwCorner, pArea->getSwCorner(), Vars::Colors::NavbotArea.Value, true);
		// Ne -> Se
		H::Draw.RenderLine(pArea->getNeCorner(), pArea->m_seCorner, Vars::Colors::NavbotArea.Value, true);
		// Sw -> Se
		H::Draw.RenderLine(pArea->getSwCorner(), pArea->m_seCorner, Vars::Colors::NavbotArea.Value, true);
	}

	if (Vars::NavEng::NavEngine::Draw.Value & Vars::NavEng::NavEngine::DrawEnum::Path && !crumbs.empty())
	{
		for (size_t i = 0; i < crumbs.size() - 1; i++)
			H::Draw.RenderLine(crumbs[i].vec, crumbs[i + 1].vec, Vars::Colors::NavbotPath.Value, false);
	}	
}