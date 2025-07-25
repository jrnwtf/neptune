#pragma once
#include "CBaseAnimating.h"

enum
{
	TF_GL_MODE_REGULAR = 0,
	TF_GL_MODE_REMOTE_DETONATE,
	TF_GL_MODE_REMOTE_DETONATE_PRACTICE,
	TF_GL_MODE_CANNONBALL
};

class CBaseProjectile : public CBaseAnimating
{
public:
	NETVAR(m_hOriginalLauncher, EHANDLE, "CBaseProjectile", "m_hOriginalLauncher");
};

class CBaseGrenade : public CBaseProjectile
{
public:
	NETVAR(m_flDamage, float, "CBaseGrenade", "m_flDamage");
	NETVAR(m_DmgRadius, float, "CBaseGrenade", "m_DmgRadius");
	NETVAR(m_bIsLive, bool, "CBaseGrenade", "m_bIsLive");
	NETVAR(m_hThrower, EHANDLE, "CBaseGrenade", "m_hThrower");
	NETVAR(m_vecVelocity, Vec3, "CBaseGrenade", "m_vecVelocity");
	NETVAR(m_fFlags, int, "CBaseGrenade", "m_fFlags");
};

class CTFBaseRocket : public CBaseProjectile
{
public:
	NETVAR(m_vInitialVelocity, Vec3, "CTFBaseRocket", "m_vInitialVelocity");
	NETVAR(m_vecOrigin, Vec3, "CTFBaseRocket", "m_vecOrigin");
	NETVAR(m_angRotation, Vec3, "CTFBaseRocket", "m_angRotation");
	NETVAR(m_iDeflected, int, "CTFBaseRocket", "m_iDeflected");
	NETVAR(m_hLauncher, EHANDLE, "CTFBaseRocket", "m_hLauncher");

	OFFSET(m_flSpawnTime, float, 3112);
};

class CTFBaseProjectile : public CBaseProjectile
{
public:
	NETVAR(m_hLauncher, EHANDLE, "CTFBaseProjectile", "m_hLauncher");
	NETVAR(m_vInitialVelocity, Vec3, "CTFBaseProjectile", "m_vInitialVelocity");
};

class CTFWeaponBaseGrenadeProj : public CBaseGrenade
{
public:
	NETVAR(m_vInitialVelocity, Vec3, "CTFWeaponBaseGrenadeProj", "m_vInitialVelocity");
	NETVAR(m_bCritical, bool, "CTFWeaponBaseGrenadeProj", "m_bCritical");
	NETVAR(m_iDeflected, int, "CTFWeaponBaseGrenadeProj", "m_iDeflected");
	NETVAR(m_vecOrigin, Vec3, "CTFWeaponBaseGrenadeProj", "m_vecOrigin");
	NETVAR(m_angRotation, Vec3, "CTFWeaponBaseGrenadeProj", "m_angRotation");
	NETVAR(m_hDeflectOwner, EHANDLE, "CTFWeaponBaseGrenadeProj", "m_hDeflectOwner");

	VIRTUAL(GetDamageRadius, float, 220, this);
	VIRTUAL(GetDamageType, int, 229, this);
};

class CTFProjectile_Rocket : public CTFBaseRocket
{
public:
	NETVAR(m_bCritical, bool, "CTFProjectile_Rocket", "m_bCritical");
};

class CTFProjectile_EnergyBall : public CTFBaseRocket
{
public:
	NETVAR(m_bChargedShot, bool, "CTFProjectile_EnergyBall", "m_bChargedShot");
	NETVAR(m_vColor1, Vec3, "CTFProjectile_EnergyBall", "m_bChargedShot");
	NETVAR(m_vColor2, Vec3, "CTFProjectile_EnergyBall", "m_bChargedShot");
};

class CTFProjectile_Flare : public CTFBaseRocket
{
public:
	NETVAR(m_bCritical, bool, "CTFProjectile_Flare", "m_bCritical");
};

class CTFProjectile_Arrow : public CTFBaseRocket
{
public:
	NETVAR(m_bArrowAlight, bool, "CTFProjectile_Arrow", "m_bArrowAlight");
	NETVAR(m_bCritical, bool, "CTFProjectile_Arrow", "m_bCritical");
	NETVAR(m_iProjectileType, int, "CTFProjectile_Arrow", "m_iProjectileType");

	bool CanHeadshot();
};

class CTFProjectile_GrapplingHook : public CTFProjectile_Arrow
{
public:

};

class CTFProjectile_HealingBolt : public CTFProjectile_Arrow
{
public:

};

class CTFProjectile_MechanicalArmOrb : public CTFProjectile_Rocket
{
public:

};

class CTFProjectile_SentryRocket : public CTFProjectile_Rocket
{
public:

};

class CTFProjectile_BallOfFire : public CTFProjectile_Rocket
{
public:
	NETVAR(m_vecInitialVelocity, Vec3, "CTFProjectile_BallOfFire", "m_vecInitialVelocity");
	NETVAR(m_vecSpawnOrigin, Vec3, "CTFProjectile_BallOfFire", "m_vecSpawnOrigin");
};

class CTFGrenadePipebombProjectile : public CTFWeaponBaseGrenadeProj
{
public:
	NETVAR(m_bTouched, bool, "CTFGrenadePipebombProjectile", "m_bTouched");
	NETVAR(m_iType, int, "CTFGrenadePipebombProjectile", "m_iType");
	NETVAR(m_hLauncher, EHANDLE, "CTFGrenadePipebombProjectile", "m_hLauncher");
	NETVAR(m_bDefensiveBomb, int, "CTFGrenadePipebombProjectile", "m_bDefensiveBomb");

	NETVAR_OFF(m_flCreationTime, float, "CTFGrenadePipebombProjectile", "m_iType", 4);
	NETVAR_OFF(m_bPulsed, bool, "CTFGrenadePipebombProjectile", "m_iType", 12);

	bool HasStickyEffects();
};

class CTFProjectile_Jar : public CTFGrenadePipebombProjectile
{
public:
	NETVAR(m_bTouched, byte, "CTFProjectile_Pipebomb", "m_bTouched");
	NETVAR(m_iType, int, "CTFProjectile_Pipebomb", "m_iType");
	NETVAR(m_hLauncher, EHANDLE, "CTFProjectile_Pipebomb", "m_hLauncher");
	NETVAR(m_bDefensiveBomb, byte, "CTFProjectile_Pipebomb", "m_bDefensiveBomb");
};

class CTFProjectile_JarGas : public CTFProjectile_Jar
{
public:

};

class CTFProjectile_Cleaver : public CTFProjectile_Jar
{
public:

};

class CTFProjectile_JarMilk : public CTFProjectile_Jar
{
public:

};

class CTFProjectile_Throwable : public CTFProjectile_Jar
{
public:

};

class CTFProjectile_ThrowableBreadMonster : public CTFProjectile_Throwable
{
public:

};

class CTFProjectile_ThrowableBrick : public CTFProjectile_Throwable
{
public:

};

class CTFProjectile_ThrowableRepel : public CTFProjectile_Throwable
{
public:

};