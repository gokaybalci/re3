#include "common.h"
#include "patcher.h"
#include "Vector.h"
#include "PlayerPed.h"
#include "Entity.h"
#include "PointLights.h"
#include "Particle.h"
#include "Timer.h"
#include "Vehicle.h"
#include "Shadows.h"
#include "Automobile.h"
#include "World.h"
#include "General.h"
#include "EventList.h"
#include "DamageManager.h"
#include "Ped.h"
#include "Fire.h"

CFireManager &gFireManager = *(CFireManager*)0x8F31D0;

CFire::CFire()
{
	m_bIsOngoing = false;
	m_bIsScriptFire = false;
	m_bPropagationFlag = true;
	m_bAudioSet = true;
	m_vecPos = CVector(0.0f, 0.0f, 0.0f);
	m_pEntity = 0;
	m_pSource = 0;
	m_nFiremenPuttingOut = 0;
	m_nExtinguishTime = 0;
	m_nStartTime = 0;
	field_20 = 1;
	field_24 = 0;
	m_fStrength = 0.8f;
}

CFire::~CFire() {}

class CFire_ : public CFire {
public:
	void ctor() { ::new(this) CFire(); }
	void dtor() { CFire::CFire(); }
};

void
CFire::ProcessFire(void)
{
	float fDamagePlayer;
	float fDamagePeds;
	float fDamageVehicle;
	int8 nRandNumber;
	float fGreen;
	float fRed;
	CVector lightpos;
	CVector firePos;
	CPed *ped = (CPed *)m_pEntity;
	CVehicle *veh = (CVehicle*)m_pEntity;

	if (m_pEntity) {
		m_vecPos = m_pEntity->GetPosition();

		if (((CPed *)m_pEntity)->IsPed()) {
			if (ped->m_pFire != this) {
				Extinguish();
				return;
			}
			if (ped->m_nMoveState != PEDMOVE_RUN)
				m_vecPos.z -= 1.0f;
			if (ped->bInVehicle && ped->m_pMyVehicle) {
				if (ped->m_pMyVehicle->IsCar())
					ped->m_pMyVehicle->m_fHealth = 75.0f;
			} else if (m_pEntity == (CPed *)FindPlayerPed()) {
				fDamagePlayer = 1.2f * CTimer::GetTimeStep();

				((CPlayerPed *)m_pEntity)->InflictDamage(
					(CPlayerPed *)m_pSource, WEAPONTYPE_FLAMETHROWER,
						fDamagePlayer, PEDPIECE_TORSO, 0);
			} else {
				fDamagePeds = 1.2f * CTimer::GetTimeStep();

				if (((CPlayerPed *)m_pEntity)->InflictDamage(
					(CPlayerPed *)m_pSource, WEAPONTYPE_FLAMETHROWER,
					fDamagePeds, PEDPIECE_TORSO, 0)) {
					m_pEntity->bRenderScorched = true;
				}
			}
		} else if (m_pEntity->IsVehicle()) {
			if (veh->m_pCarFire != this) {
				Extinguish();
				return;
			}
			if (!m_bIsScriptFire) {
				fDamageVehicle = 1.2f * CTimer::GetTimeStep();
				veh->InflictDamage((CVehicle *)m_pSource, WEAPONTYPE_FLAMETHROWER, fDamageVehicle);
			}
		}
	}
	if (!FindPlayerVehicle() && !FindPlayerPed()->m_pFire && !(FindPlayerPed()->bFireProof)
		&& ((FindPlayerPed()->GetPosition() - m_vecPos).MagnitudeSqr() < 2.0f)) {
		FindPlayerPed()->DoStuffToGoOnFire();
		gFireManager.StartFire(FindPlayerPed(), m_pSource, 0.8f, 1);
	}
	if (CTimer::m_snTimeInMilliseconds > field_24) { /* set to 0 when a newfire starts, related to time */
		field_24 = CTimer::m_snTimeInMilliseconds + 80;
		firePos = m_vecPos;

		if (veh && veh->IsVehicle() && veh->IsCar()) {
			CVehicleModelInfo *mi = ((CVehicleModelInfo*)CModelInfo::GetModelInfo(veh->GetModelIndex()));
			CVector ModelInfo = mi->m_positions[CAR_POS_HEADLIGHTS];
			ModelInfo = m_pEntity->GetMatrix() * ModelInfo;

			firePos.x = ModelInfo.x;
			firePos.y = ModelInfo.y;
			firePos.z = ModelInfo.z + 0.15f;
		}

		CParticle::AddParticle(PARTICLE_CARFLAME, firePos,
			CVector(0.0f, 0.0f, CGeneral::GetRandomNumberInRange(0.0125f, 0.1f) * m_fStrength),
				0, m_fStrength, 0, 0, 0, 0);

		rand(); rand(); rand(); /* unsure why these three rands are called */

		CParticle::AddParticle(PARTICLE_CARFLAME_SMOKE, firePos,
			CVector(0.0f, 0.0f, 0.0f), 0, 0.0, 0, 0, 0, 0);
	}
	if (CTimer::m_snTimeInMilliseconds < m_nExtinguishTime || m_bIsScriptFire) {
		if (CTimer::m_snTimeInMilliseconds > m_nStartTime)
			m_nStartTime = CTimer::m_snTimeInMilliseconds + 400;

		nRandNumber = CGeneral::GetRandomNumber();
		lightpos.x = m_vecPos.x;
		lightpos.y = m_vecPos.y;
		lightpos.z = m_vecPos.z + 5.0f;

		if (!m_pEntity) {
			CShadows::StoreStaticShadow((uint32)this, SHADOWTYPE_ADDITIVE, gpShadowExplosionTex, &lightpos,
				7.0, 0.0, 0.0, -7.0, 0, nRandNumber / 2, nRandNumber / 2,
				0, 10.0, 1.0, 40.0, 0, 0.0);
		}
		fGreen = nRandNumber / 128;
		fRed = nRandNumber / 128;

		CPointLights::AddLight(0, m_vecPos, CVector(0.0f, 0.0f, 0.0f),
			12.0, fRed, fGreen, 0, 0, 0);
	} else {
		Extinguish();
	}
}

void
CFire::ReportThisFire(void)
{
	gFireManager.m_nTotalFires++;
	CEventList::RegisterEvent(EVENT_FIRE, m_vecPos, 1000);
}

void
CFire::Extinguish(void)
{
	if (m_bIsOngoing) {
		if (!m_bIsScriptFire)
			gFireManager.m_nTotalFires--;

		m_nExtinguishTime = 0;
		m_bIsOngoing = false;

		if (m_pEntity) {
			if (m_pEntity->IsPed()) {
				((CPed *)m_pEntity)->RestorePreviousState();
				((CPed *)m_pEntity)->m_pFire = 0;
			} else if (m_pEntity->IsVehicle()) {
				((CVehicle *)m_pEntity)->m_pCarFire = nil;
			}
			m_pEntity = nil;
		}
	}
}

void
CFireManager::StartFire(CVector pos, float size, bool propagation)
{
	CFire *fire = GetNextFreeFire();

	if (fire) {
		fire->m_bIsOngoing = true;
		fire->m_bIsScriptFire = false;
		fire->m_bPropagationFlag = propagation;
		fire->m_bAudioSet = true;
		fire->m_vecPos = pos;
		fire->m_nExtinguishTime = CTimer::m_snTimeInMilliseconds + 10000;
		fire->m_nStartTime = CTimer::m_snTimeInMilliseconds + 400;
		fire->m_pEntity = nil;
		fire->m_pSource = nil;
		fire->field_24 = 0;
		fire->ReportThisFire();
		fire->m_fStrength = size;
	}
}

CFire *
CFireManager::StartFire(CEntity *entityOnFire, CEntity *fleeFrom, float strength, bool propagation)
{
	CPed *ped = (CPed *)entityOnFire;
	CVehicle *veh = (CVehicle *)entityOnFire;
	
	if (entityOnFire->IsPed()) {
		if (ped->m_pFire)
			return nil;
		if (!ped->IsPedInControl())
			return nil;
	} else if (entityOnFire->IsVehicle()) {
		if (veh->m_pCarFire)
			return nil;
		if (veh->IsCar() &&  ((CAutomobile *)veh)->Damage.GetEngineStatus() >= 225)
			return nil;
	}
	CFire *fire = GetNextFreeFire();
	
	if (fire) {
		if (entityOnFire->IsPed()) {
			ped->m_pFire = fire;
			if (ped != FindPlayerPed()) {
				if (fleeFrom) {
					ped->SetFlee(fleeFrom, 10000);
				} else {
					CVector2D pos = entityOnFire->GetPosition();
					ped->SetFlee(pos, 10000);
					ped->m_fleeFrom = nil;
				}
				ped->bDrawLast = false;
				ped->SetMoveState(PEDMOVE_SPRINT);
				ped->SetMoveAnim();
				ped->m_nPedState = PED_ON_FIRE;
			}
			if (fleeFrom) {
				if (ped->m_nPedType == PEDTYPE_COP) {
					CEventList::RegisterEvent(EVENT_COP_SET_ON_FIRE, EVENT_ENTITY_PED,
						entityOnFire, (CPed *)fleeFrom, 10000);
				} else {
					CEventList::RegisterEvent(EVENT_PED_SET_ON_FIRE, EVENT_ENTITY_PED,
						entityOnFire, (CPed *)fleeFrom, 10000);
				}
			}
		} else {
			if (entityOnFire->IsVehicle()) {
				veh->m_pCarFire = fire;
				if (fleeFrom) {
					CEventList::RegisterEvent(EVENT_CAR_SET_ON_FIRE, EVENT_ENTITY_VEHICLE,
						entityOnFire, (CPed *)fleeFrom, 10000);	
				}
			}
		}

		fire->m_bIsOngoing = true;
		fire->m_bIsScriptFire = false;
		fire->m_vecPos = entityOnFire->GetPosition();

		if (entityOnFire && entityOnFire->IsPed() && ped->IsPlayer()) {
			fire->m_nExtinguishTime = CTimer::m_snTimeInMilliseconds + 3333;
		} else if (entityOnFire->IsVehicle()) {
			fire->m_nExtinguishTime = CTimer::m_snTimeInMilliseconds + CGeneral::GetRandomNumberInRange(4000, 5000);
		} else {
			fire->m_nExtinguishTime = CTimer::m_snTimeInMilliseconds + CGeneral::GetRandomNumberInRange(10000, 11000);
		}
		fire->m_nStartTime = CTimer::m_snTimeInMilliseconds + 400;
		fire->m_pEntity = entityOnFire;

		entityOnFire->RegisterReference(&fire->m_pEntity);
		fire->m_pSource = fleeFrom;

		if (fleeFrom)
			fleeFrom->RegisterReference(&fire->m_pSource);
		fire->ReportThisFire();
		fire->field_24 = 0;
		fire->m_fStrength = strength;
		fire->m_bPropagationFlag = propagation;
		fire->m_bAudioSet = true;
	}
	return fire;
}

void
CFireManager::Update(void)
{
	for (int i = 0; i < NUM_FIRES; i++) {
		if (m_aFires[i].m_bIsOngoing)
			m_aFires[i].ProcessFire();
	}
}

CFire* CFireManager::FindNearestFire(CVector vecPos, float *pDistance)
{
	for (int i = 0; i < MAX_FIREMEN_ATTENDING; i++) {
		int fireId = -1;
		float minDistance = 999999;
		for (int j = 0; j < NUM_FIRES; j++) {
			if (!m_aFires[j].m_bIsOngoing)
				continue;
			if (m_aFires[j].m_bIsScriptFire)
				continue;
			if (m_aFires[j].m_nFiremenPuttingOut != i)
				continue;
			float distance = (m_aFires[j].m_vecPos - vecPos).Magnitude2D();
			if (distance < minDistance) {
				minDistance = distance;
				fireId = j;
			}
		}
		*pDistance = minDistance;
		if (fireId != -1)
			return &m_aFires[fireId];
	}
	return nil;
}

CFire *
CFireManager::FindFurthestFire_NeverMindFireMen(CVector coords, float minRange, float maxRange)
{
	int furthestFire = -1;
	float lastFireDist = 0.0;
	float fireDist;

	for (int i = 0; i < NUM_FIRES; i++) {
		if (m_aFires[i].m_bIsOngoing && !m_aFires[i].m_bIsScriptFire) {
			fireDist = (m_aFires[i].m_vecPos - coords).Magnitude2D();
			if (fireDist > minRange && fireDist < maxRange && fireDist > lastFireDist) {
				lastFireDist = fireDist;
				furthestFire = i;
			}
		}
	}
	if (furthestFire == -1)
		return nil;
	else
		return &m_aFires[furthestFire];
}

CFire *
CFireManager::GetNextFreeFire(void)
{
	for (int i = 0; i < NUM_FIRES; i++) {
		if (!m_aFires[i].m_bIsOngoing && !m_aFires[i].m_bIsScriptFire)
			return &m_aFires[i];
	}
	return nil;
}

uint32
CFireManager::GetTotalActiveFires(void) const
{
	return m_nTotalFires;
}

void
CFireManager::ExtinguishPoint(CVector point, float range)
{
	for (int i = 0; i < NUM_FIRES; i++) {
		if (m_aFires[i].m_bIsOngoing) {
			if ((point - m_aFires[i].m_vecPos).MagnitudeSqr() < sq(range))
				m_aFires[i].Extinguish();
		}
	}
}

int32
CFireManager::StartScriptFire(const CVector &pos, CEntity *target, float strength, bool propagation)
{
	CFire *fire;
	CPed *ped = (CPed *)target;
	CVehicle *veh = (CVehicle *)target;

	if (target) {
		if (target->IsPed()) {
			if (ped->m_pFire)
				ped->m_pFire->Extinguish();
		} else if (target->IsVehicle()) {
			if (veh->m_pCarFire)
				veh->m_pCarFire->Extinguish();
			if (veh->IsCar() && ((CAutomobile *)veh)->Damage.GetEngineStatus() >= 225) {
				((CAutomobile *)veh)->Damage.SetEngineStatus(215);
			}
		}
	}

	fire = GetNextFreeFire();
	fire->m_bIsOngoing = true;
	fire->m_bIsScriptFire = true;
	fire->m_bPropagationFlag = propagation;
	fire->m_bAudioSet = true;
	fire->m_vecPos = pos;
	fire->m_nStartTime = CTimer::m_snTimeInMilliseconds + 400;
	fire->m_pEntity = target;

	if (target)
		target->RegisterReference(&fire->m_pEntity);
	fire->m_pSource = nil;
	fire->field_24 = 0;
	fire->m_fStrength = strength;
	if (target) {
		if (target->IsPed()) {
			ped->m_pFire = fire;
			if (target != (CVehicle *)FindPlayerPed()) {
				CVector2D pos = target->GetPosition();
				ped->SetFlee(pos, 10000);
				ped->SetMoveAnim();
				ped->m_nPedState = PED_ON_FIRE;
			}
		} else if (target->IsVehicle()) {
			veh->m_pCarFire = fire;
		}
	}
	return fire - m_aFires;
}

bool
CFireManager::IsScriptFireExtinguish(int16 index)
{
	return (!m_aFires[index].m_bIsOngoing) ? true : false;
}

void
CFireManager::RemoveAllScriptFires(void)
{
	for (int i = 0; i < NUM_FIRES; i++) {
		if (m_aFires[i].m_bIsScriptFire) {
			m_aFires[i].Extinguish();
			m_aFires[i].m_bIsScriptFire = false;
		}
	}
}

void
CFireManager::RemoveScriptFire(int16 index)
{
	m_aFires[index].Extinguish();
	m_aFires[index].m_bIsScriptFire = false;
}

void
CFireManager::SetScriptFireAudio(int16 index, bool state)
{
	m_aFires[index].m_bAudioSet = state;
}

STARTPATCHES
	InjectHook(0x479220, &CFire_::ctor, PATCH_JUMP);
	InjectHook(0x479280, &CFire_::dtor, PATCH_JUMP);
	InjectHook(0x4798D0, &CFire::ProcessFire, PATCH_JUMP);
	InjectHook(0x4798B0, &CFire::ReportThisFire, PATCH_JUMP);
	InjectHook(0x479D40, &CFire::Extinguish, PATCH_JUMP);
	InjectHook(0x479500, (void(CFireManager::*)(CVector pos, float size, bool propagation))&CFireManager::StartFire, PATCH_JUMP);
	InjectHook(0x479590, (CFire *(CFireManager::*)(CEntity *, CEntity *, float, bool))&CFireManager::StartFire, PATCH_JUMP);
	InjectHook(0x479310, &CFireManager::Update, PATCH_JUMP);
	InjectHook(0x479430, &CFireManager::FindFurthestFire_NeverMindFireMen, PATCH_JUMP);
	InjectHook(0x479340, &CFireManager::FindNearestFire, PATCH_JUMP);
	InjectHook(0x4792E0, &CFireManager::GetNextFreeFire, PATCH_JUMP);
	InjectHook(0x479DB0, &CFireManager::ExtinguishPoint, PATCH_JUMP);
	InjectHook(0x479E60, &CFireManager::StartScriptFire, PATCH_JUMP);
	InjectHook(0x479FC0, &CFireManager::IsScriptFireExtinguish, PATCH_JUMP);
	InjectHook(0x47A000, &CFireManager::RemoveAllScriptFires, PATCH_JUMP);
	InjectHook(0x479FE0, &CFireManager::RemoveScriptFire, PATCH_JUMP);
	InjectHook(0x47A040, &CFireManager::SetScriptFireAudio, PATCH_JUMP);
ENDPATCHES
