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

#include "precompiled_game.h"
#pragma hdrstop

static bool versioned = RegisterVersionedFile("$Id$");

#include "FleeState.h"
#include "../Memory.h"
#include "../../AIComm_Message.h"
#include "../Library.h"
#include "../Tasks/WaitTask.h"
#include "../Tasks/FleeTask.h"
#include "../Tasks/SingleBarkTask.h"
#include "../Tasks/RepeatedBarkTask.h"
#include "FleeDoneState.h"

namespace ai
{

// Get the name of this state
const idStr& FleeState::GetName() const
{
	static idStr _name(STATE_FLEE);
	return _name;
}

void FleeState::Init(idAI* owner)
{
	State::Init(owner);

	DM_LOG(LC_AI, LT_INFO)LOGSTRING("FleeState initialised.\r");
	assert(owner);

	// Shortcut reference
	Memory& memory = owner->GetMemory();
	memory.fleeingDone = false;

	// Fill the subsystems with their tasks

	// The movement subsystem should wait half a second before starting to run
	owner->StopMove(MOVE_STATUS_DONE);
	memory.stopRelight = true; // grayman #2603 - abort a relight in progress
	memory.stopExaminingRope = true; // grayman #2872 - stop examining rope

	if (owner->GetEnemy())
	{
		owner->FaceEnemy();
	}

	owner->movementSubsystem->ClearTasks();
	owner->movementSubsystem->PushTask(TaskPtr(new WaitTask(1000)));
	owner->movementSubsystem->QueueTask(FleeTask::CreateInstance());

	// The communication system cries for help
	owner->StopSound(SND_CHANNEL_VOICE, false);
	owner->GetSubsystem(SubsysCommunication)->ClearTasks();
/*	owner->GetSubsystem(SubsysCommunication)->PushTask(TaskPtr(new WaitTask(200)));*/// TODO_AI
	
	// Setup the message to be delivered each time
	CommMessagePtr message(new CommMessage(
		CommMessage::RequestForHelp_CommType, 
		owner, NULL, // from this AI to anyone 
		NULL,
		memory.alertPos
	));

	owner->commSubsystem->AddCommTask(
		CommunicationTaskPtr(new SingleBarkTask("snd_to_flee",message))
	);

	owner->commSubsystem->AddSilence(3000);

	CommunicationTaskPtr barkTask(new RepeatedBarkTask("snd_flee", 4000,8000, message));
	owner->commSubsystem->AddCommTask(barkTask);

	// The sensory system 
	owner->senseSubsystem->ClearTasks();

	// No action
	owner->actionSubsystem->ClearTasks();

	// Play the surprised animation
	owner->SetAnimState(ANIMCHANNEL_TORSO, "Torso_Surprise", 5);
	owner->SetAnimState(ANIMCHANNEL_LEGS, "Legs_Surprise", 5);
	owner->SetWaitState("surprise");
}

// Gets called each time the mind is thinking
void FleeState::Think(idAI* owner)
{
	// Shortcut reference
	Memory& memory = owner->GetMemory();

	if (memory.fleeingDone)
	{
		owner->ClearEnemy();
		owner->GetMind()->SwitchState(STATE_FLEE_DONE);
	}
}

void FleeState::OnFailedKnockoutBlow(idEntity* attacker, const idVec3& direction, bool hitHead)
{
	// Ignore failed knockout attempts in flee mode
}

StatePtr FleeState::CreateInstance()
{
	return StatePtr(new FleeState);
}

// Register this state with the StateLibrary
StateLibrary::Registrar fleeStateRegistrar(
	STATE_FLEE, // Task Name
	StateLibrary::CreateInstanceFunc(&FleeState::CreateInstance) // Instance creation callback
);

} // namespace ai
