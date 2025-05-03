#include "FollowBot.h"
#include <cmath>
#include "../TickHandler/TickHandler.h"
#include "../Misc/Misc.h"
#include <direct.h>

int CFollowBot::GetTargetIndex(CTFPlayer* pLocal)
{
    if (m_iTargetIndex != -1 && I::GlobalVars->curtime - m_flLastTargetTime < 1.0f)
        return m_iTargetIndex;

    m_flLastTargetTime = I::GlobalVars->curtime;
    int iBestTarget = -1;
    float flClosestDistance = FLT_MAX;
    
    // If we have a specific player ID to follow
    std::string followID = Vars::Misc::Movement::FollowBot::FollowID.Value;
    if (!followID.empty())
    {
        // Check all players to find a match by SteamID
        for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
        {
            if (!IsValidTarget(pLocal, i, true))  // Pass true to ignore FollowID check in IsValidTarget
                continue;

            auto pEntity = I::ClientEntityList->GetClientEntity(i);
            if (!pEntity || !pEntity->IsPlayer())
                continue;

            auto pPlayer = pEntity->As<CTFPlayer>();
            if (!pPlayer->IsAlive() || pPlayer == pLocal)
                continue;
                
            // Check if this player matches our target SteamID
            PlayerInfo_t info;
            if (I::EngineClient->GetPlayerInfo(i, &info))
            {
                std::string playerID = std::to_string(info.friendsID);
                if (playerID == followID)
                    return i;  // Found our target by ID, return immediately
            }
        }
    }
    
    // If we get here and have a follow ID set but didn't find a match, don't follow anyone else
    if (!followID.empty())
        return -1;

    // Otherwise, look for the closest valid player
    for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
    {
        if (!IsValidTarget(pLocal, i, false))
            continue;

        auto pEntity = I::ClientEntityList->GetClientEntity(i);
        if (!pEntity || !pEntity->IsPlayer())
            continue;

        auto pPlayer = pEntity->As<CTFPlayer>();
        if (!pPlayer->IsAlive() || pPlayer == pLocal)
            continue;

        float flDistance = pLocal->GetAbsOrigin().DistTo(pPlayer->GetAbsOrigin());
        
        if (flDistance < flClosestDistance)
        {
            flClosestDistance = flDistance;
            iBestTarget = i;
        }
    }


    if (iBestTarget != m_iTargetIndex)
    {
        if (iBestTarget != -1)
        {
            m_bIsFollowing = true;
        }
        else
        {
            m_bIsFollowing = false;
        }
        m_iTargetIndex = iBestTarget;
    }

    return m_iTargetIndex;
}

bool CFollowBot::IsValidTarget(CTFPlayer* pLocal, int iEntIndex, bool ignoreIDCheck)
{
    if (iEntIndex == pLocal->entindex())
        return false;

    auto pEntity = I::ClientEntityList->GetClientEntity(iEntIndex);
    if (!pEntity || !pEntity->IsPlayer())
        return false;

    auto pPlayer = pEntity->As<CTFPlayer>();
    if (!pPlayer->IsAlive())
        return false;

    if (pPlayer->m_iTeamNum() != pLocal->m_iTeamNum() && 
        !(Vars::Misc::Movement::FollowBot::FollowEnemies.Value))
        return false;

    if (F::PlayerUtils.IsIgnored(iEntIndex))
        return false;
    
    // If we have a specific ID to follow and we're not ignoring that check
    if (!ignoreIDCheck && !Vars::Misc::Movement::FollowBot::FollowID.Value.empty())
    {
        PlayerInfo_t info;
        if (I::EngineClient->GetPlayerInfo(iEntIndex, &info))
        {
            std::string playerID = std::to_string(info.friendsID);
            if (playerID != Vars::Misc::Movement::FollowBot::FollowID.Value)
                return false;  // Not the player we want to follow
        }
        else
        {
            return false;  // Couldn't get player info
        }
    }

    // Only follow friends if that option is enabled
    if (Vars::Misc::Movement::FollowBot::OnlyFriends.Value)
    {
        return H::Entities.IsFriend(iEntIndex) || H::Entities.InParty(iEntIndex);
    }
    
    // Only follow party members if that option is enabled
    if (Vars::Misc::Movement::FollowBot::OnlyParty.Value)
    {
        return H::Entities.InParty(iEntIndex);
    }

    return true;
}

float CFollowBot::GetFollowDistance()
{

    return static_cast<float>(Vars::Misc::Movement::FollowBot::Distance.Value);
}

std::optional<Vector> CFollowBot::GetFollowPosition(CTFPlayer* pLocal, CTFPlayer* pTarget)
{
    if (!pTarget)
        return std::nullopt;


    Vector vTargetPos = pTarget->GetAbsOrigin();
    Vector vTargetAng = pTarget->GetAbsAngles();
    

    float flDistance = GetFollowDistance();
    
    if (Vars::Misc::Movement::FollowBot::PositionMode.Value == 0) // Behind
    {

        Vector vForward;
        Math::AngleVectors(vTargetAng, &vForward);
        vForward *= -flDistance;
        return vTargetPos + vForward;
    }
    else if (Vars::Misc::Movement::FollowBot::PositionMode.Value == 1) // Side
    {

        Vector vRight;
        Math::AngleVectors(vTargetAng, nullptr, &vRight, nullptr);
        vRight *= flDistance * (std::fmod(I::GlobalVars->curtime, 10.0f) < 5.0f ? 1.0f : -1.0f);
        return vTargetPos + vRight;
    }
    else // Adaptive
    {

        Vector vForward, vRight;
        Math::AngleVectors(vTargetAng, &vForward, &vRight, nullptr);
        

        Vector vBehindPos = vTargetPos - (vForward * flDistance);
        CNavArea* pBehindArea = F::NavEngine.map->findClosestNavSquare(vBehindPos);
        if (pBehindArea && F::NavParser.IsVectorVisibleNavigation(pLocal->GetAbsOrigin(), vBehindPos))
            return vBehindPos;
        

        Vector vRightPos = vTargetPos + (vRight * flDistance);
        CNavArea* pRightArea = F::NavEngine.map->findClosestNavSquare(vRightPos);
        if (pRightArea && F::NavParser.IsVectorVisibleNavigation(pLocal->GetAbsOrigin(), vRightPos))
            return vRightPos;
        

        Vector vLeftPos = vTargetPos - (vRight * flDistance);
        CNavArea* pLeftArea = F::NavEngine.map->findClosestNavSquare(vLeftPos);
        if (pLeftArea && F::NavParser.IsVectorVisibleNavigation(pLocal->GetAbsOrigin(), vLeftPos))
            return vLeftPos;
        

        return vTargetPos;
    }
}

void CFollowBot::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{

    if (!Vars::Misc::Movement::FollowBot::Enabled.Value)
    {

        if (m_bIsFollowing)
        {
            Reset();
        }
        return;
    }


    if (!pLocal->IsAlive() || !F::NavEngine.isReady(true))
    {
        return;
    }


    int iTargetIndex = GetTargetIndex(pLocal);
    if (iTargetIndex == -1)
    {

        if (m_bIsFollowing)
        {
            F::NavEngine.cancelPath();
            m_bIsFollowing = false;
        }
        return;
    }

    auto pTarget = I::ClientEntityList->GetClientEntity(iTargetIndex);
    if (!pTarget || !pTarget->IsPlayer())
        return;

    auto pPlayerTarget = pTarget->As<CTFPlayer>();
    if (!pPlayerTarget->IsAlive())
        return;


    auto vFollowPos = GetFollowPosition(pLocal, pPlayerTarget);
    if (!vFollowPos)
        return;


    if (vFollowPos->DistTo(pLocal->GetAbsOrigin()) < 50.0f)
        return;

    // Update our path at a reasonable rate
    bool bShouldUpdatePath = (I::GlobalVars->curtime - m_flLastPathUpdateTime > 0.5f);
    if (bShouldUpdatePath)
    {
        // Follow with normal priority
        F::NavEngine.navTo(*vFollowPos, 5);
        m_flLastPathUpdateTime = I::GlobalVars->curtime;
    }
}

void CFollowBot::Reset()
{
    // Cancel the current path
    F::NavEngine.cancelPath();
    m_bIsFollowing = false;
    m_iTargetIndex = -1;
    m_flLastTargetTime = 0.0f;
    m_flLastPathUpdateTime = 0.0f;
}
