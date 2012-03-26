/*****************************************************************************
                    The Dark Mod GPL Source Code
 
 This file is part of the The Dark Mod Source Code, originally based 
 on the Doom 3 GPL Source Code as published in 2011.
 
 The Dark Mod Source Code is free software: you can redistribute it 
 and/or modify it under the terms of the GNU General Public License as 
 published by the Free Software Foundation, either version 3 of the License, 
 or (at your option) any later version. For details, see LICENSE.TXT.
 
 Project: The Dark Mod (http://www.thedarkmod.com/)
 
 $Revision$ (Revision of last commit) 
 $Date$ (Date of last commit)
 $Author$ (Author of last commit)
 
******************************************************************************/

// Copyright (C) 2004 Gerhard W. Gruber <sparhawk@gmx.at>
//

#include "precompiled_game.h"
#pragma hdrstop

static bool versioned = RegisterVersionedFile("$Id$");

#include "Game_local.h"
#include "ai/AAS_local.h"
#include "DarkModGlobals.h"
#include "BinaryFrobMover.h"
#include "SndProp.h"
#include "StimResponse/StimResponse.h"

//===============================================================================
// CBinaryFrobMover
//===============================================================================

const idEventDef EV_TDM_FrobMover_Open( "Open", NULL );
const idEventDef EV_TDM_FrobMover_Close( "Close", NULL );
const idEventDef EV_TDM_FrobMover_ToggleOpen( "ToggleOpen", NULL );
const idEventDef EV_TDM_FrobMover_Lock( "Lock", NULL );
const idEventDef EV_TDM_FrobMover_Unlock( "Unlock", NULL );
const idEventDef EV_TDM_FrobMover_ToggleLock( "ToggleLock", NULL );
const idEventDef EV_TDM_FrobMover_IsOpen( "IsOpen", NULL, 'f' );
const idEventDef EV_TDM_FrobMover_IsLocked( "IsLocked", NULL, 'f' );
const idEventDef EV_TDM_FrobMover_IsPickable( "IsPickable", NULL, 'f' );
const idEventDef EV_TDM_FrobMover_HandleLockRequest( "HandleLockRequest", NULL ); // used for periodic checks to lock the door once it is fully closed

CLASS_DECLARATION( idMover, CBinaryFrobMover )
	EVENT( EV_PostSpawn,					CBinaryFrobMover::Event_PostSpawn )
	EVENT( EV_TDM_FrobMover_Open,			CBinaryFrobMover::Event_Open)
	EVENT( EV_TDM_FrobMover_Close,			CBinaryFrobMover::Event_Close)
	EVENT( EV_TDM_FrobMover_ToggleOpen,		CBinaryFrobMover::Event_ToggleOpen)
	EVENT( EV_TDM_FrobMover_Lock,			CBinaryFrobMover::Event_Lock)
	EVENT( EV_TDM_FrobMover_Unlock,			CBinaryFrobMover::Event_Unlock)
	EVENT( EV_TDM_FrobMover_ToggleLock,		CBinaryFrobMover::Event_ToggleLock)
	EVENT( EV_TDM_FrobMover_IsOpen,			CBinaryFrobMover::Event_IsOpen)
	EVENT( EV_TDM_FrobMover_IsLocked,		CBinaryFrobMover::Event_IsLocked)
	EVENT( EV_TDM_FrobMover_IsPickable,		CBinaryFrobMover::Event_IsPickable)
	EVENT( EV_Activate,						CBinaryFrobMover::Event_Activate)
	EVENT( EV_TDM_FrobMover_HandleLockRequest,	CBinaryFrobMover::Event_HandleLockRequest)
END_CLASS

CBinaryFrobMover::CBinaryFrobMover()
{
	m_Lock = NULL;
	
	DM_LOG(LC_FUNCTION, LT_DEBUG)LOGSTRING("this: %08lX [%s]\r", this, __FUNCTION__);
	m_FrobActionScript = "frob_binary_mover";
	m_Open = false;
	m_bInterruptable = true;
	m_bInterrupted = false;
	m_StoppedDueToBlock = false;
	m_LastBlockingEnt = NULL;
	m_bIntentOpen = false;
	m_StateChange = false;
	m_Rotating = false;
	m_Translating = false;
	m_TransSpeed = 0;
	m_ImpulseThreshCloseSq = 0;
	m_ImpulseThreshOpenSq = 0;
	m_vImpulseDirOpen.Zero();
	m_vImpulseDirClose.Zero();
	m_stopWhenBlocked = false;
	m_LockOnClose = false;
	m_bFineControlStarting = false;
	m_closedBox = box_zero; // grayman #2345 - holds closed position
	m_closedBox.Clear();	// grayman #2345
	m_registeredAI.Clear();	// grayman #1145
	m_lastUsedBy = NULL;	// grayman #2859
	m_searching = NULL;		// grayman #1327 - someone searching around this door
	m_targetingOff = false; // grayman #3029
}

CBinaryFrobMover::~CBinaryFrobMover()
{
	delete m_Lock;
}

void CBinaryFrobMover::SetClosedBox(idBox box) // grayman #2345
{
	m_closedBox = box;
}

idBox CBinaryFrobMover::GetClosedBox() // grayman #2345
{
	return m_closedBox;
}

void CBinaryFrobMover::AddObjectsToSaveGame(idSaveGame* savefile)
{
	idEntity::AddObjectsToSaveGame(savefile);

	savefile->AddObject(m_Lock);
}

void CBinaryFrobMover::Save(idSaveGame *savefile) const
{
	// The lock class is saved by the idSaveGame class on close, no need to handle it here
	savefile->WriteObject(m_Lock);

	savefile->WriteBool(m_Open);
	savefile->WriteBool(m_bIntentOpen);
	savefile->WriteBool(m_StateChange);
	savefile->WriteBool(m_bInterruptable);
	savefile->WriteBool(m_bInterrupted);
	savefile->WriteBool(m_StoppedDueToBlock);

	m_LastBlockingEnt.Save(savefile);
	
	savefile->WriteAngles(m_Rotate);
		
	savefile->WriteVec3(m_StartPos);
	savefile->WriteVec3(m_Translation);
	savefile->WriteFloat(m_TransSpeed);

	savefile->WriteAngles(m_ClosedAngles);
	savefile->WriteAngles(m_OpenAngles);

	savefile->WriteVec3(m_ClosedOrigin);
	savefile->WriteVec3(m_OpenOrigin);

	savefile->WriteVec3(m_ClosedPos);
	savefile->WriteVec3(m_OpenPos);
	savefile->WriteVec3(m_OpenDir);

	savefile->WriteString(m_CompletionScript);

	savefile->WriteBool(m_Rotating);
	savefile->WriteBool(m_Translating);
	savefile->WriteFloat(m_ImpulseThreshCloseSq);
	savefile->WriteFloat(m_ImpulseThreshOpenSq);
	savefile->WriteVec3(m_vImpulseDirOpen);
	savefile->WriteVec3(m_vImpulseDirClose);

	savefile->WriteBool(m_stopWhenBlocked);
	savefile->WriteBool(m_LockOnClose);
	savefile->WriteBool(m_bFineControlStarting);
	savefile->WriteBox(m_closedBox); // grayman #2345
	
	// grayman #1145 - registered AI for a locked door
	savefile->WriteInt(m_registeredAI.Num());
	for (int i = 0 ; i < m_registeredAI.Num() ; i++ )
	{
		m_registeredAI[i].Save(savefile);
	}

	m_lastUsedBy.Save(savefile);			// grayman #2859
	m_searching.Save(savefile);				// grayman #1327
	savefile->WriteBool(m_targetingOff);	// grayman #3029
}

void CBinaryFrobMover::Restore( idRestoreGame *savefile )
{
	// The lock class is restored by the idRestoreGame, don't handle it here
	savefile->ReadObject(reinterpret_cast<idClass*&>(m_Lock));

	savefile->ReadBool(m_Open);
	savefile->ReadBool(m_bIntentOpen);
	savefile->ReadBool(m_StateChange);
	savefile->ReadBool(m_bInterruptable);
	savefile->ReadBool(m_bInterrupted);
	savefile->ReadBool(m_StoppedDueToBlock);

	m_LastBlockingEnt.Restore(savefile);
	
	savefile->ReadAngles(m_Rotate);
	
	savefile->ReadVec3(m_StartPos);
	savefile->ReadVec3(m_Translation);
	savefile->ReadFloat(m_TransSpeed);

	savefile->ReadAngles(m_ClosedAngles);
	savefile->ReadAngles(m_OpenAngles);

	savefile->ReadVec3(m_ClosedOrigin);
	savefile->ReadVec3(m_OpenOrigin);

	savefile->ReadVec3(m_ClosedPos);
	savefile->ReadVec3(m_OpenPos);
	savefile->ReadVec3(m_OpenDir);

	savefile->ReadString(m_CompletionScript);

	savefile->ReadBool(m_Rotating);
	savefile->ReadBool(m_Translating);
	savefile->ReadFloat(m_ImpulseThreshCloseSq);
	savefile->ReadFloat(m_ImpulseThreshOpenSq);
	savefile->ReadVec3(m_vImpulseDirOpen);
	savefile->ReadVec3(m_vImpulseDirClose);

	savefile->ReadBool(m_stopWhenBlocked);
	savefile->ReadBool(m_LockOnClose);
	savefile->ReadBool(m_bFineControlStarting);
	savefile->ReadBox(m_closedBox); // grayman #2345

	// grayman #1145 - registered AI for a locked door
	m_registeredAI.Clear();
	int num;
	savefile->ReadInt(num);
	m_registeredAI.SetNum(num);
	for (int i = 0 ; i < num ; i++)
	{
		m_registeredAI[i].Restore(savefile);
	}

	m_lastUsedBy.Restore(savefile);		// grayman #2859
	m_searching.Restore(savefile);		// grayman #1327
	savefile->ReadBool(m_targetingOff);	// grayman #3029
}

void CBinaryFrobMover::Spawn()
{
	// Setup our PickableLock instance
	m_Lock = static_cast<PickableLock*>(PickableLock::CreateInstance());
	m_Lock->SetOwner(this);
	m_Lock->SetLocked(false);

	m_stopWhenBlocked = spawnArgs.GetBool("stop_when_blocked", "1");

	m_Open = spawnArgs.GetBool("open");
	DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("[%s] open (%u)\r", name.c_str(), m_Open);

	m_bInterruptable = spawnArgs.GetBool("interruptable");
	DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("[%s] interruptable (%u)\r", name.c_str(), m_bInterruptable);

	// log if visportal was found
	if( areaPortal > 0 )
	{
		DM_LOG(LC_SYSTEM, LT_DEBUG)LOGSTRING("FrobDoor [%s] found portal handle %d on spawn \r", name.c_str(), areaPortal);
	}

	// Load the spawnargs for the lock
	m_Lock->InitFromSpawnargs(spawnArgs);
	
	// Schedule a post-spawn event to parse the rest of the spawnargs
	// greebo: Be sure to use 16 ms as delay to allow the SpawnBind event to execute before this one.
	PostEventMS( &EV_PostSpawn, 16 );
}

void CBinaryFrobMover::PostSpawn()
{
	// m_Translation is the vector between start position and end position
	m_Rotate = spawnArgs.GetAngles("rotate", "0 90 0");
	m_Translation = spawnArgs.GetVector("translate", "0 0 0");
	m_TransSpeed = spawnArgs.GetFloat( "translate_speed", "0");

	// angua: the origin of the door in opened and closed state
	m_ClosedOrigin = physicsObj.GetLocalOrigin();
	m_OpenOrigin = m_ClosedOrigin + m_Translation;

	m_ClosedAngles = physicsObj.GetLocalAngles();
	m_ClosedAngles.Normalize180();
	m_OpenAngles = (m_ClosedAngles + m_Rotate).Normalize180();

	if (m_ClosedOrigin.Compare(m_OpenOrigin) && m_ClosedAngles.Compare(m_OpenAngles))
	{
		gameLocal.Warning("FrobMover %s will not move, translation and rotation not set.", name.c_str());
	}

	// set up physics impulse behavior
	spawnArgs.GetFloat("impulse_thresh_open", "0", m_ImpulseThreshOpenSq );
	spawnArgs.GetFloat("impulse_thresh_close", "0", m_ImpulseThreshCloseSq );
	m_ImpulseThreshOpenSq *= m_ImpulseThreshOpenSq;
	m_ImpulseThreshCloseSq *= m_ImpulseThreshCloseSq;
	spawnArgs.GetVector("impulse_dir_open", "0 0 0", m_vImpulseDirOpen );
	spawnArgs.GetVector("impulse_dir_close", "0 0 0", m_vImpulseDirClose );
	if( m_vImpulseDirOpen.LengthSqr() > 0 )
		m_vImpulseDirOpen.Normalize();
	if( m_vImpulseDirClose.LengthSqr() > 0 )
		m_vImpulseDirClose.Normalize();

	// set the first intent according to the initial doorstate
	m_bIntentOpen = !m_Open;

	// Let the mapper override the initial frob intent on a partially opened door
	if( m_Open && spawnArgs.GetBool("first_frob_open") )
	{
		m_bIntentOpen = true;
		m_bInterrupted = true;
		m_StateChange = true;
	}

	// greebo: Partial Angles define the initial angles of the door
	idAngles partialAngles(0,0,0);

	// Check if the door should spawn as "open"
	if (m_Open)
	{
		DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("[%s] shall be open at spawn time, checking start_* spawnargs.\r", name.c_str());

		// greebo: Load values from spawnarg, we might need to adjust them later on
		partialAngles = spawnArgs.GetAngles("start_rotate", "0 0 0");
		m_StartPos = spawnArgs.GetVector("start_position", "0 0 0");

		bool hasPartialRotation = !partialAngles.Compare(idAngles(0,0,0));
		bool hasPartialTranslation = !m_StartPos.Compare(idVec3(0,0,0));

		if (hasPartialRotation && !m_Translation.Compare(idVec3(0,0,0)))
		{
			DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("[%s] has partial angles set, calculating partial translation automatically.\r", name.c_str());

			// Sliding door has partial angles set, calculate the partial translation automatically
			idRotation maxRot = (m_OpenAngles - m_ClosedAngles).Normalize360().ToRotation();

			if (maxRot.GetAngle() > 0)
			{
				idRotation partialRot = partialAngles.Normalize360().ToRotation(); // grayman #720 - fixed partial rotation value
				float fraction = partialRot.GetAngle() / maxRot.GetAngle();
				m_StartPos = m_Translation * fraction;
			}
			else
			{
				gameLocal.Warning("Mover '%s' has start_rotate set, but rotation angles are zero.", name.c_str());
				DM_LOG(LC_SYSTEM, LT_ERROR)LOGSTRING("[%s] has start_rotate set, but rotation angles are zero.\r", name.c_str());
			}
		}
		else if (hasPartialTranslation && !m_Rotate.Compare(idAngles(0,0,0)))
		{
			DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("[%s] has partial translation set, calculating partial rotation automatically.\r", name.c_str());

			// Rotating door has partial translation set, calculate the partial rotation automatically
			float maxTrans = (m_OpenOrigin - m_ClosedOrigin).Length();
			float partialTrans = m_StartPos.Length();

			if (maxTrans > 0)
			{
				float fraction = partialTrans / maxTrans;

				partialAngles = m_ClosedAngles + (m_OpenAngles - m_ClosedAngles) * fraction;
			}
			else
			{
				gameLocal.Warning("Mover '%s' has start_position set, but translation is zero.", name.c_str());
				DM_LOG(LC_SYSTEM, LT_ERROR)LOGSTRING("[%s] has partial translation set, but translation is zero?\r", name.c_str());
			}
		}
		else if (!hasPartialRotation && !hasPartialTranslation)
		{
			DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("[%s]: has open='1' but neither partial translation nor rotation are set, assuming fully opened mover.\r", name.c_str());
			DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("[%s]: Resetting first_frob_open and interrupted flags.\r", name.c_str());
			
			// Neither start_position nor start_rotate set, assume fully opened door
			partialAngles = m_OpenAngles - m_ClosedAngles;
			m_StartPos = m_OpenOrigin - m_ClosedOrigin;

			// greebo: In this case, first_frob_open makes no sense, override the settings
			m_bIntentOpen = false;
			m_bInterrupted = false;
			m_StateChange = false; 
		}
	}

	// angua: calculate the positions of the vertex  with the largest 
	// distance to the origin when the door is closed or open
	idClipModel *clipModel = GetPhysics()->GetClipModel();
	if (clipModel == NULL)
	{
		gameLocal.Error("Binary Frob Mover %s has no clip model", name.c_str());
	}
	idBox closedBox(clipModel->GetBounds(), m_ClosedOrigin, m_ClosedAngles.ToMat3());
	idVec3 closedBoxVerts[8];
	closedBox.GetVerts(closedBoxVerts);
	m_closedBox = closedBox; // grayman #720 - save for AI obstacle detection

	float maxDistSquare = 0;
	for (int i = 0; i < 8; i++)
	{
		float distSquare = (closedBoxVerts[i] - m_ClosedOrigin).LengthSqr();
		if (distSquare > maxDistSquare)
		{
			m_ClosedPos = closedBoxVerts[i] - m_ClosedOrigin;
			maxDistSquare = distSquare;
		}
	}
	//gameRenderWorld->DebugArrow(colorGreen, GetPhysics()->GetOrigin() + m_ClosedPos, GetPhysics()->GetOrigin() + m_ClosedPos + idVec3(0, 0, 30), 2, 200000);

	idBox openBox(clipModel->GetBounds(), m_OpenOrigin, m_OpenAngles.ToMat3());
	idVec3 openBoxVerts[8];
	openBox.GetVerts(openBoxVerts);

	maxDistSquare = 0;
	for (int i = 0; i < 8; i++)
	{
		float distSquare = (openBoxVerts[i] - m_OpenOrigin).LengthSqr();
		if (distSquare > maxDistSquare)
		{
			m_OpenPos = openBoxVerts[i] - m_OpenOrigin;
			maxDistSquare = distSquare;
		}
	}
	// gameRenderWorld->DebugArrow(colorRed, GetPhysics()->GetOrigin() + m_OpenPos, GetPhysics()->GetOrigin() + m_OpenPos + idVec3(0, 0, 30), 2, 200000);

	idRotation rot = m_Rotate.ToRotation();
	idVec3 rotationAxis = rot.GetVec();
	idVec3 normal = rotationAxis.Cross(m_ClosedPos);

	m_OpenDir = (m_OpenPos * normal) * normal;
	m_OpenDir.Normalize();
	// gameRenderWorld->DebugArrow(colorBlue, GetPhysics()->GetOrigin(), GetPhysics()->GetOrigin() + 20 * m_OpenDir, 2, 200000);

	if (m_Open) 
	{
		// door starts out partially open, set origin and angles to the values defined in the spawnargs.
		physicsObj.SetLocalOrigin(m_ClosedOrigin + m_StartPos);
		physicsObj.SetLocalAngles(m_ClosedAngles + partialAngles);
	}

	UpdateVisuals();

	// grayman #2603 - Process targets. For those that are lights, add yourself
	// to their switch list.

	for (int i = 0 ; i < targets.Num() ; i++ )
	{
		idEntity* e = targets[i].GetEntity();
		if (e)
		{
			if (e->IsType(idLight::Type))
			{
				idLight* light = static_cast<idLight*>(e);
				light->AddSwitch(this);
			}
		}
	}

	// Check if we should auto-open, which could also happen right at the map start
	if (IsAtClosedPosition())
	{
		float autoOpenTime = -1;
		if (spawnArgs.GetFloat("auto_open_time", "-1", autoOpenTime) && autoOpenTime >= 0)
		{
			// Convert the time to msec and post the event
			PostEventMS(&EV_TDM_FrobMover_Open, static_cast<int>(SEC2MS(autoOpenTime)));
		}
	}
	else if (IsAtOpenPosition())
	{
		// Check if we should move back to the closedpos
		float autoCloseTime = -1;
		if (spawnArgs.GetFloat("auto_close_time", "-1", autoCloseTime) && autoCloseTime >= 0)
		{
			// Convert the time to msec and post the event
			PostEventMS(&EV_TDM_FrobMover_Close, static_cast<int>(SEC2MS(autoCloseTime)));
		}
	}
}

void CBinaryFrobMover::Event_PostSpawn() 
{
	// Call the virtual function
	PostSpawn();
}

void CBinaryFrobMover::Lock(bool bMaster)
{
	if (!PreLock(bMaster)) {
		// PreLock returned FALSE, cancel the operation
		DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("[%s] FrobMover prevented from being locked\r", name.c_str());
		return;
	}

	DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("[%s] FrobMover is locked\r", name.c_str());

	m_Lock->SetLocked(true);
	CallStateScript();

	// Fire the event for the subclasses
	OnLock(bMaster);
}

void CBinaryFrobMover::TellRegisteredUsers()
{
	// grayman #1145 - remove this door's area number from each registered AI's forbidden area list

	int numUsers = m_registeredAI.Num();

	for (int i = 0 ; i < numUsers ; i++)
	{
		idAI* ai = m_registeredAI[i].GetEntity();
		idAAS* aas = ai->GetAAS();
		if (aas != NULL)
		{
			gameLocal.m_AreaManager.RemoveForbiddenArea(GetAASArea(aas),ai);
		}
	}
	m_registeredAI.Clear(); // served its purpose, clear for next batch
}

void CBinaryFrobMover::Unlock(bool bMaster)
{
	if (!PreUnlock(bMaster)) {
		// PreUnlock returned FALSE, cancel the operation
		DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("[%s] FrobMover prevented from being unlocked\r", name.c_str());
		return;
	}
	
	DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("[%s] FrobMover is unlocked\r", name.c_str());

	m_Lock->SetLocked(false);
	CallStateScript();

	// Fire the event for the subclasses
	OnUnlock(bMaster);

	TellRegisteredUsers(); // grayman #1145 - remove this door's area number from each registered AI's forbidden area list
}

void CBinaryFrobMover::ToggleLock()
{
	// greebo: When the mover is open, close and lock it.
	if (m_Open == true)
	{
		Close();
	}

	if (m_Lock->IsLocked())
	{
		Unlock();
	}
	else
	{
		Lock();
	}
}

void CBinaryFrobMover::CloseAndLock()
{
	if (!IsLocked() && !IsAtClosedPosition())
	{
		m_LockOnClose = true;
		Close();
	}
	else
	{
		// We're at close position, just lock it
		Lock();
	}
}

bool CBinaryFrobMover::StartMoving(bool open) 
{
	// Get the target position and orientation
	idVec3 targetOrigin = open ? m_OpenOrigin : m_ClosedOrigin;
	idAngles targetAngles = open ? m_OpenAngles : m_ClosedAngles;

	// Assume we are moving
	m_Rotating = true;
	m_Translating = true;

	// Clear the block variables
	m_StoppedDueToBlock = false;
	m_LastBlockingEnt = NULL;
	m_bInterrupted = false;
	
	idAngles angleDelta = (targetAngles - physicsObj.GetLocalAngles()).Normalize180();

	if (!angleDelta.Compare(idAngles(0,0,0)))
	{
		Event_RotateOnce(angleDelta);
	}
	else
	{
		m_Rotating = false; // nothing to rotate
	}
	
	if (!targetOrigin.Compare(physicsObj.GetLocalOrigin(), VECTOR_EPSILON))
	{	
		if (m_TransSpeed)
		{
			Event_SetMoveSpeed(m_TransSpeed);
		}

		MoveToLocalPos(targetOrigin); // Start moving
	}
	else
	{
		m_Translating = false;
	}

	// Now let's look if we are actually moving
	m_StateChange = (m_Translating || m_Rotating);

	if (m_StateChange)
	{
		// Fire the "we're starting to move" event
		OnMoveStart(open);

		// If we're changing states from closed to open, set the bool right now
		if (!m_Open)
		{
			m_Open = true;
		}
	}

	return m_StateChange;
}

void CBinaryFrobMover::Open(bool bMaster)
{
	DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("BinaryFrobMover: Opening\r" );

	if (!PreOpen()) 
	{
		// PreOpen returned FALSE, cancel the operation
		return;
	}

	// We have green light, let's see where we are starting from
	bool wasClosed = IsAtClosedPosition();

	// Now, actually trigger the moving process
	if (StartMoving(true))
	{
		// We're moving, fire the event and pass the starting state
		OnStartOpen(wasClosed, bMaster);
	}

	// Set the "intention" flag so that we're closing next time, even if we didn't move
	m_bIntentOpen = false;
}

void CBinaryFrobMover::Close(bool bMaster)
{
	DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("BinaryFrobMover: Closing\r" );

	if (!PreClose()) 
	{
		// PreClose returned FALSE, cancel the operation
		return;
	}

	// We have green light, let's see where we are starting from
	bool wasOpen = IsAtOpenPosition();

	// Now, actually trigger the moving process
	if (StartMoving(false))
	{
		// We're moving, fire the event and pass the starting state
		OnStartClose(wasOpen, bMaster);
	}

	// Set the "intention" flag so that we're opening next time, even if we didn't move
	m_bIntentOpen = true;
}

void CBinaryFrobMover::ToggleOpen()
{
	if (!IsMoving())
	{
		// We are not moving
		DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("BinaryFrobMover: Was stationary on ToggleOpen\r" );

		if (m_bIntentOpen)
		{
			Open();
		}
		else
		{
			Close();
		}

		return;
	}

	DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("FrobDoor: Was moving on ToggleOpen.\r" );

	// We are moving, is the mover interruptable?
	if (m_bInterruptable && PreInterrupt())
	{
		DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("FrobDoor: Interrupted! Stopping door\r" );

		m_bInterrupted = true;
		Event_StopRotating();
		Event_StopMoving();

		OnInterrupt();
	}
}

void CBinaryFrobMover::DoneMoving()
{
	idMover::DoneMoving();
    m_Translating = false;

	DoneStateChange();
}

void CBinaryFrobMover::DoneRotating()
{
	idMover::DoneRotating();
    m_Rotating = false;

	DoneStateChange();
}

void CBinaryFrobMover::DoneStateChange()
{
	if (!m_StateChange || IsMoving())
	{
		// Entity is still moving, wait for the next call (this usually gets called twice)
		return;
	}

	DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("BinaryFrobMover: DoneStateChange\r" );

	// Check which position we're at, set the state variables and fire the correct events

	m_StateChange = false;

	// We are assuming that we're still "open", this gets overwritten by the check below
	// if we are at the final "closed" position
	m_Open = true;

	if (IsAtClosedPosition())
	{
		DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("FrobDoor: Closed completely\r" );

		m_Open = false;

		// We have reached the final close position, fire the event
		OnClosedPositionReached();
	}
	else if (IsAtOpenPosition())
	{
		// We have reached the final open position, fire the event
		OnOpenPositionReached();
	}

	// Invoked the script object to notify it about the state change
	CallStateScript();
}

bool CBinaryFrobMover::IsAtOpenPosition()
{
	const idVec3& localOrg = physicsObj.GetLocalOrigin();

	const idAngles& localAngles = physicsObj.GetLocalAngles();
	
	// greebo: Let the check be slightly inaccurate (use the standard epsilon).
	return (localAngles - m_OpenAngles).Normalize360().Compare(ang_zero, VECTOR_EPSILON) && 
		   localOrg.Compare(m_OpenOrigin, VECTOR_EPSILON);
}

bool CBinaryFrobMover::IsAtClosedPosition()
{
	const idVec3& localOrg = physicsObj.GetLocalOrigin();

	const idAngles& localAngles = physicsObj.GetLocalAngles();
	
	// greebo: Let the check be slightly inaccurate (use the standard epsilon).
	return (localAngles - m_ClosedAngles).Normalize360().Compare(ang_zero, VECTOR_EPSILON) && 
		   localOrg.Compare(m_ClosedOrigin, VECTOR_EPSILON);
}

void CBinaryFrobMover::CallStateScript()
{
	idStr functionName = spawnArgs.GetString("state_change_callback", "");

	if (!functionName.IsEmpty())
	{
		DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("FrobDoor: Callscript '%s' Open: %d, Locked: %d Interrupted: %d\r",
			functionName.c_str(), m_Open, m_Lock->IsLocked(), m_bInterrupted);
		CallScriptFunctionArgs(functionName, true, 0, "ebbb", this, m_Open, m_Lock->IsLocked(), m_bInterrupted);
	}
}

void CBinaryFrobMover::Event_Activate(idEntity *activator) 
{
	ToggleOpen();
}

void CBinaryFrobMover::OnTeamBlocked(idEntity* blockedEntity, idEntity* blockingEntity)
{
	m_LastBlockingEnt = blockingEntity;
	// greebo: If we're blocked by something, check if we should stop moving
	if (m_stopWhenBlocked)
	{
		m_bInterrupted = true;
		m_StoppedDueToBlock = true;

		Event_StopRotating();
		Event_StopMoving();
	}

	// Clear the close request flag
	m_LockOnClose = false;
	CancelEvents(&EV_TDM_FrobMover_HandleLockRequest);
}

void CBinaryFrobMover::ApplyImpulse(idEntity *ent, int id, const idVec3 &point, const idVec3 &impulse)
{
	idVec3 SwitchDir;
	float SwitchThreshSq(0), ImpulseMagSq(0);

	if ( (m_Open && m_ImpulseThreshCloseSq > 0) || (!m_Open && m_ImpulseThreshOpenSq > 0) )
	{
		if (m_Open)
		{
			SwitchDir = m_vImpulseDirClose;
			SwitchThreshSq = m_ImpulseThreshCloseSq;
		}
		else
		{
			SwitchDir = m_vImpulseDirOpen;
			SwitchThreshSq = m_ImpulseThreshOpenSq;
		}
		
		// only resolve along the axis if it is set.  Defaults to (0,0,0) if not set
		if (SwitchDir.LengthSqr())
		{
			SwitchDir = GetPhysics()->GetAxis() * SwitchDir;
			ImpulseMagSq = impulse*SwitchDir;
			ImpulseMagSq *= ImpulseMagSq;
		}
		else
		{
			ImpulseMagSq = impulse.LengthSqr();
		}

		if (ImpulseMagSq >= SwitchThreshSq)
		{
			ToggleOpen();
		}
	}

	idEntity::ApplyImpulse( ent, id, point, impulse);
}

/*-------------------------------------------------------------------------*/

bool CBinaryFrobMover::IsMoving()
{
	return (m_Translating || m_Rotating);
}

/*-------------------------------------------------------------------------*/

bool CBinaryFrobMover::IsChangingState()
{
	return m_StateChange;
}

/*-------------------------------------------------------------------------*/

void CBinaryFrobMover::GetRemainingMovement(idVec3& out_deltaPosition, idAngles& out_deltaAngles)
{
	// Get remaining translation if translating
	if (m_bIntentOpen)
	{
		out_deltaPosition = (m_OpenOrigin + m_Translation) - physicsObj.GetOrigin(); // grayman #2345
//		out_deltaPosition = (m_StartPos + m_Translation) - physicsObj.GetOrigin(); // grayman #2345
	}
	else
	{
		out_deltaPosition = m_ClosedOrigin - physicsObj.GetOrigin(); // grayman #2345
//		out_deltaPosition = m_StartPos - physicsObj.GetOrigin(); // grayman #2345
	}

	// Get remaining rotation
	idAngles curAngles;
	physicsObj.GetAngles(curAngles);

	if (m_bIntentOpen)
	{
		out_deltaAngles = m_OpenAngles - curAngles;
	}
	else
	{
		out_deltaAngles = m_ClosedAngles - curAngles;
	}

	// Done
}

float CBinaryFrobMover::GetMoveTimeFraction()
{
	// Get the current angles
	const idAngles& curAngles = physicsObj.GetLocalAngles();

	// Calculate the delta
	idAngles delta = dest_angles - curAngles;
	delta[0] = idMath::Fabs(delta[0]);
	delta[1] = idMath::Fabs(delta[1]);
	delta[2] = idMath::Fabs(delta[2]);

	// greebo: Note that we don't need to compare against zero angles here, because
	// this code won't be called in this case (see idMover::BeginRotation).

	idAngles fullRotation = (m_OpenAngles - m_ClosedAngles).Normalize180();
	fullRotation[0] = idMath::Fabs(fullRotation[0]);
	fullRotation[1] = idMath::Fabs(fullRotation[1]);
	fullRotation[2] = idMath::Fabs(fullRotation[2]);

	// Get the maximum angle component
	int index = (delta[0] > delta[1]) ? 0 : 1;
	index = (delta[2] > delta[index]) ? 2 : index;

	if (fullRotation[index] < idMath::FLT_EPSILON) return 1;

	float fraction = delta[index]/fullRotation[index];

	return fraction;
}

int CBinaryFrobMover::GetAASArea(idAAS* aas)
{
	if (aas == NULL)
	{
		return -1;
	}

	if (GetPhysics() == NULL)
	{
		return -1;
	}

	idClipModel *clipModel = GetPhysics()->GetClipModel();
	if (clipModel == NULL)
	{
		gameLocal.Error("FrobMover %s has no clip model", name.c_str());
	}

	const idBounds& bounds = clipModel->GetAbsBounds();

	idVec3 center = GetClosedBox().GetCenter(); // grayman #2877 - new way
//	idVec3 center = GetPhysics()->GetOrigin() + m_ClosedPos * 0.5; // grayman #2877 - old way
	center.z = bounds[0].z + 1;

	int areaNum = aas->PointReachableAreaNum( center, bounds, AREA_REACHABLE_WALK );
	idAASLocal* aasLocal = dynamic_cast<idAASLocal*> (aas);

	if (aasLocal)
	{
		int clusterNum = aasLocal->GetClusterNum(areaNum);
		if (clusterNum > 0)
		{
			// angua: This is not a portal area, check the surroundings for portals
			idReachability *reach;
			for ( reach = aasLocal->GetAreaFirstReachability(areaNum); reach; reach = reach->next ) {
				int testAreaNum = reach->toAreaNum;
				int testClusterNum = aasLocal->GetClusterNum(testAreaNum);
				if (testClusterNum < 0)
				{
					// we have found a portal area, take this one instead
					areaNum = testAreaNum;
					break;
				}
			}
		}

		// aasLocal->DrawArea(areaNum);
	}

//	idStr areatext(areaNum);
//	gameRenderWorld->DebugLine(colorGreen,center,center + idVec3(0,0,20),10000000);
//	gameRenderWorld->DebugLine(colorOrange,GetPhysics()->GetOrigin(),GetPhysics()->GetOrigin() + m_ClosedPos,10000000);
//	gameRenderWorld->DrawText(areatext.c_str(), center + idVec3(0,0,1), 0.2f, colorGreen, mat3_identity, 1, 10000000);

	return areaNum;
}

void CBinaryFrobMover::OnMoveStart(bool opening)
{
	// Clear this door from the ignore list so AI can react to it again
	// grayman #2859 - but only if the door was closed and is now opening

	if ( opening )
	{
		ClearStimIgnoreList(ST_VISUAL);
		EnableStim(ST_VISUAL);
	}
}

bool CBinaryFrobMover::PreOpen() 
{
	if (m_Lock->IsLocked()) 
	{
		// Play the "I'm locked" sound 
		FrobMoverStartSound("snd_locked");
		// and prevent the door from opening (return false)
		return false;
	}

	return true; // default: mover is allowed to open
}

bool CBinaryFrobMover::PreClose()
{
	return true; // default: mover is allowed to close
}

bool CBinaryFrobMover::PreInterrupt()
{
	return true; // default: mover is allowed to be interrupted
}

bool CBinaryFrobMover::PreLock(bool bMaster)
{
	return true; // default: mover is allowed to be locked
}

bool CBinaryFrobMover::PreUnlock(bool bMaster)
{
	return true; // default: mover is allowed to be unlocked
}

void CBinaryFrobMover::OnStartOpen(bool wasClosed, bool bMaster)
{
	if (wasClosed)
	{
		// Only play the "open" sound when the door was completely closed
		FrobMoverStartSound("snd_open");

		// trigger our targets on opening, if set to do so
		if (spawnArgs.GetBool("trigger_on_open", "0"))
		{
			ActivateTargets(this);
		}
	}

	// Clear the lock request flag in any case
	m_LockOnClose = false;
	CancelEvents(&EV_TDM_FrobMover_HandleLockRequest);
}

void CBinaryFrobMover::OnStartClose(bool wasOpen, bool bMaster)
{
	// To be implemented by the subclasses
}

void CBinaryFrobMover::OnOpenPositionReached()
{
	TellRegisteredUsers(); // grayman #1145

	// trigger our targets when completely opened, if set to do so
	if (spawnArgs.GetBool("trigger_when_opened", "0"))
	{
		ActivateTargets(this);
	}

	// Check if we should move back to the closedpos after use
	float autoCloseTime = -1;
	if (spawnArgs.GetFloat("auto_close_time", "-1", autoCloseTime) && autoCloseTime >= 0)
	{
		// Convert the time to msec and post the event
		PostEventMS(&EV_TDM_FrobMover_Close, static_cast<int>(SEC2MS(autoCloseTime)));
	}
}

void CBinaryFrobMover::OnClosedPositionReached()
{
	// play the closing sound when the door closes completely
	FrobMoverStartSound("snd_close");

	// trigger our targets on completely closing, if set to do so
	if (spawnArgs.GetBool("trigger_on_close", "0"))
	{
		ActivateTargets(this);
	}

	// Check if we should move back to the openpos
	float autoOpenTime = -1;
	if (spawnArgs.GetFloat("auto_open_time", "-1", autoOpenTime) && autoOpenTime >= 0)
	{
		// Convert the time to msec and post the event
		PostEventMS(&EV_TDM_FrobMover_Open, static_cast<int>(SEC2MS(autoOpenTime)));
	}

	// Do we have a close request?
	if (m_LockOnClose)
	{
		// Clear the flag, regardless what happens
		m_LockOnClose = false;
		
		// Post a lock request in LOCK_REQUEST_DELAY msecs
		PostEventMS(&EV_TDM_FrobMover_HandleLockRequest, LOCK_REQUEST_DELAY);
	}
}

void CBinaryFrobMover::OnInterrupt()
{
	// Clear the close request flag
	m_LockOnClose = false;
	CancelEvents(&EV_TDM_FrobMover_HandleLockRequest);
}

void CBinaryFrobMover::OnLock(bool bMaster)
{
	FrobMoverStartSound("snd_lock");

	m_Lock->OnLock();
}

void CBinaryFrobMover::OnUnlock(bool bMaster)
{
	m_Lock->OnUnlock();

	int soundLength = FrobMoverStartSound("snd_unlock");

	// angua: only open the master
	// only if the other part is an openpeer it will be opened by ToggleOpen
	// otherwise it will stay closed
	if (cv_door_auto_open_on_unlock.GetBool() && bMaster)
	{
		// The configuration says: open the mover when it's unlocked, but let's check the mapper's settings
		bool openOnUnlock = true;
		bool spawnArgSet = spawnArgs.GetBool("open_on_unlock", "1", openOnUnlock);

		if (!spawnArgSet || openOnUnlock)
		{
			// No spawnarg set or opening is allowed, just open the mover after a short delay
			PostEventMS(&EV_TDM_FrobMover_ToggleOpen, soundLength);
		}
	}
}

int CBinaryFrobMover::FrobMoverStartSound(const char* soundName)
{
	// Default implementation: Just play the sound on this entity.
	int length = 0;
	StartSound(soundName, SND_CHANNEL_ANY, 0, false, &length);

	return length;
}

void CBinaryFrobMover::Event_Open()
{
	Open();
}

void CBinaryFrobMover::Event_Close()
{
	Close();
}

void CBinaryFrobMover::Event_ToggleOpen()
{
	ToggleOpen();
}

void CBinaryFrobMover::Event_IsOpen()
{
	idThread::ReturnInt(m_Open);
}

void CBinaryFrobMover::Event_Lock()
{
	Lock();
}

void CBinaryFrobMover::Event_Unlock()
{
	Unlock();
}

void CBinaryFrobMover::Event_ToggleLock()
{
	ToggleLock();
}

void CBinaryFrobMover::Event_IsLocked()
{
	idThread::ReturnInt(IsLocked());
}

void CBinaryFrobMover::Event_IsPickable()
{
	idThread::ReturnInt(IsPickable());
}

idVec3 CBinaryFrobMover::GetCurrentPos()
{
	idVec3 closedDir = m_ClosedPos;
	closedDir.z = 0;
	float length = closedDir.LengthFast();

	// grayman #720 - previous version was giving the wrong result for a
	// NS door partially opened clockwise. Corrected by normalizing the
	// closed angle and using abs() on the cos() and sin() calcs and letting
	// closedDir and m_OpenDir set the signs when calculating currentPos.

	idAngles angles = physicsObj.GetLocalAngles();
	idAngles closedAngles = GetClosedAngles();
	idAngles deltaAngles = angles - closedAngles.Normalize360();
	idRotation rot = deltaAngles.ToRotation();
	float alpha = idMath::Fabs(rot.GetAngle());
	
	idVec3 currentPos = GetPhysics()->GetOrigin() 
		+ closedDir * idMath::Fabs(idMath::Cos(alpha * idMath::PI / 180))
		+ m_OpenDir * length * idMath::Fabs(idMath::Sin(alpha * idMath::PI / 180));

	return currentPos;
}

float CBinaryFrobMover::GetFractionalPosition()
{
	// Don't know if a door translates or rotates or both,
	// so look at fractional movement of both and take the max?
	float returnval(0.0f);
	const idVec3& localOrg = physicsObj.GetLocalOrigin();
	const idAngles& localAngles = physicsObj.GetLocalAngles();
	
	// check for non-zero rotation first
	idRotation maxRot = (m_OpenAngles - m_ClosedAngles).Normalize360().ToRotation();
	if( maxRot.GetAngle() != 0 )
	{
		idRotation curRot = (localAngles - m_ClosedAngles).Normalize360().ToRotation();
		returnval = curRot.GetAngle() / maxRot.GetAngle();
	}
	else
	{
		// if door doesn't have rotation, check translation
		float maxTrans = (m_OpenOrigin - m_ClosedOrigin).Length();
		returnval = (localOrg - m_ClosedOrigin).Length() / maxTrans;
	}

	return returnval;
}

void CBinaryFrobMover::SetFractionalPosition(float fraction)
{
	idAngles targetAngles = m_ClosedAngles + (m_OpenAngles - m_ClosedAngles) * fraction;
	idAngles angleDelta = (targetAngles - physicsObj.GetLocalAngles()).Normalize180();

	if (!angleDelta.Compare(ang_zero, 0.01f))
	{
		Event_RotateOnce(angleDelta);
	}

	MoveToLocalPos(m_ClosedOrigin + (m_OpenOrigin - m_ClosedOrigin)*fraction);

	UpdateVisuals();
}

void CBinaryFrobMover::Event_HandleLockRequest()
{
	// Check if we are at our "closed" position, if yes: lock, if no: postpone the event
	if (IsAtClosedPosition())
	{
		// Yes, we are at our "closed" position, lock ourselves
		Lock(true);
	}
	else
	{
		// Not at closed position (yet), postpone the event
		PostEventMS(&EV_TDM_FrobMover_HandleLockRequest, LOCK_REQUEST_DELAY);
	}
}

void CBinaryFrobMover::FrobAction(bool frobMaster, bool isFrobPeerAction)
{
	idEntity::FrobAction( frobMaster, isFrobPeerAction );
	if( m_bInterruptable && cv_tdm_door_control.GetBool() )
		m_bFineControlStarting = true;
}

void CBinaryFrobMover::FrobHeld(bool frobMaster, bool isFrobPeerAction, int holdTime)
{
	if ( !m_bInterruptable || !cv_tdm_door_control.GetBool() || holdTime < 200 )
		return;

	idPlayer *player = gameLocal.GetLocalPlayer();
	
	if( m_bFineControlStarting )
	{
		// initialize fine control
		player->SetImmobilization( "door handling",  EIM_VIEW_ANGLE );
		m_mousePosition.x = player->usercmd.mx;
		m_mousePosition.y = player->usercmd.my;

		// Stop the door from opening or closing as normal:
		m_bInterrupted = true;
		Event_StopRotating();
		Event_StopMoving();
		OnInterrupt();

		m_bFineControlStarting = false;
	}

	//float dx = player->usercmd.mx - m_mousePosition.x;
	float dy = player->usercmd.my - m_mousePosition.y;
	m_mousePosition.x = player->usercmd.mx;
	m_mousePosition.y = player->usercmd.my;

	// figure out the view direction (rotation only for now)
	float sign = 1.0f;
	idRotation openRot = (0.1f*(m_OpenAngles - m_ClosedAngles).Normalize360()).ToRotation();
	openRot.SetOrigin(GetPhysics()->GetOrigin());
	idVec3 playerOrg = player->GetPhysics()->GetOrigin();
	idVec3 testOrg = playerOrg;
	openRot.RotatePoint(testOrg);
	float viewDot = (testOrg - playerOrg) * player->viewAxis[0];
	if (viewDot < 0)
		sign = 1.0f;
	else
		sign = -1.0f;

	float desiredPos = GetFractionalPosition() + sign * cv_tdm_door_control_sensitivity.GetFloat() * dy;
	desiredPos = idMath::ClampFloat( 0.0f, 1.0f, desiredPos );
	SetFractionalPosition( desiredPos );
}

void CBinaryFrobMover::FrobReleased(bool frobMaster, bool isFrobPeerAction, int holdTime)
{
	idPlayer *player = gameLocal.GetLocalPlayer();
	player->SetImmobilization( "door handling",  0 );
}

// grayman #1145 - add an AI who unsuccessfully tried to open a locked door

void CBinaryFrobMover::RegisterAI(idAI* ai)
{
	idEntityPtr<idAI> aiPtr;
	aiPtr = ai;
	m_registeredAI.Append(aiPtr);
}

// grayman #2691 - get the axis of rotation

idVec3 CBinaryFrobMover::GetRotationAxis()
{
	// grayman #2866 - ToRotation() defaults to rotation about the x axis for sliding doors with m_Rotate = (0,0,0),
	// so check for that special case. I don't want to change ToRotation() unless this proves
	// to be a general problem.

	idVec3 axis;
	if ( ( m_Rotate.yaw == 0 ) && ( m_Rotate.pitch == 0 ) && ( m_Rotate.roll == 0 ) )
	{
		axis.Zero();
	}
	else
	{
		axis = m_Rotate.ToRotation().GetVec(); 
	}
	return axis;
}

// grayman #2861 - get the closed origin

idVec3 CBinaryFrobMover::GetClosedOrigin()
{
	return m_ClosedOrigin;
}
