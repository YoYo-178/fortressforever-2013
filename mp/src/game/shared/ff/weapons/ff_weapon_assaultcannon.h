// =============== Fortress Forever ==============
// ======== A modification for Half-Life 2 =======
//
// @file ff_weapon_assaultcannon.h
// @author Mike Parker (AfterShock)
// @date Jan 27th 2008
// @brief The FF assault cannon code .h file
//

#ifndef FF_WEAPON_ASSAULTCANNON_H
#define FF_WEAPON_ASSAULTCANNON_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "ff_weapon_base.h"
#include "ff_fx_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL 
	#define CFFWeaponAssaultCannon C_FFWeaponAssaultCannon
	#include "c_ff_player.h"
	#include "ff_utils.h"
	#include "soundenvelope.h"
#else
	#include "omnibot_interface.h"
	#include "ff_player.h"
#endif

//ConVar ffdev_ac_maxchargetime( "ffdev_ac_maxchargetime", "1.5", FCVAR_FF_FFDEV_REPLICATED, "Time AC takes to reach full firing rate.", true, 1.0f, true, 3.0f );
#define FF_AC_MAXCHARGETIME 1.5f // ffdev_ac_maxchargetime.GetFloat()

//=============================================================================
// CFFWeaponAssaultCannon
//=============================================================================

class CFFWeaponAssaultCannon : public CFFWeaponBase
{
public:
	DECLARE_CLASS(CFFWeaponAssaultCannon, CFFWeaponBase);
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CFFWeaponAssaultCannon();
	~CFFWeaponAssaultCannon();

	virtual void Precache();
	virtual bool Deploy();
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void Drop( const Vector& vecVelocity );
	virtual void ClampOn();
	virtual void ClampOff();
	virtual void ItemPostFrame();
	virtual void PrimaryAttack();
	//virtual void Fire();

	void UpdateChargeTime();

	virtual void GetHeatLevel(int _firemode, float &_current, float &_max) 
	{
		_current = m_flChargeTime;
		_max = FF_AC_MAXCHARGETIME;
	}
private:
	virtual float GetFireRate();
	Vector GetFireSpread();
	CNetworkVar(bool, m_bClamped);
	CNetworkVar(bool, m_bAmmoTick);

	void UpdateBarrelRotation();
	CNetworkVar(int, m_iBarrelRotation);
	CNetworkVar(float, m_flBarrelRotationValue);
	CNetworkVar(float, m_flBarrelRotationDelta);
	CNetworkVar(float, m_flBarrelRotationStopTimer);

#ifdef CLIENT_DLL

	void StopBarrelRotationSound();
	CSoundPatch *m_sndBarrelRotation;

	void StopLoopShotSound();
	CSoundPatch *m_sndLoopShot;

#endif

	virtual FFWeaponID GetWeaponID() const		{ return FF_WEAPON_ASSAULTCANNON; }
	const char *GetTracerType() { return "AR2Tracer"; }

private:

	CFFWeaponAssaultCannon(const CFFWeaponAssaultCannon &);

public:	// temp while i expose m_flChargeTime to global function

	CNetworkVar(float, m_flLastTick);
	CNetworkVar(float, m_flDeployTick);
	CNetworkVar(float, m_flTriggerPressed);
	CNetworkVar(float, m_flTriggerReleased);
	CNetworkVar(bool, m_bFiring);
	CNetworkVar(float, m_flChargeTime);
	CNetworkVar(float, m_flMaxChargeTime);
};

#endif // FF_WEAPON_ASSAULTCANNON_H