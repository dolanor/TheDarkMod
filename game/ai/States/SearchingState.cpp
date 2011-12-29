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

#include "../../../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id$", init_version);

#include "SearchingState.h"
#include "../Memory.h"
#include "../Tasks/InvestigateSpotTask.h"
#include "../Tasks/SingleBarkTask.h"
#include "../Library.h"
#include "IdleState.h"
#include "AgitatedSearchingState.h"
#include "../../AbsenceMarker.h"
#include "../../AIComm_Message.h"

namespace ai
{

// Get the name of this state
const idStr& SearchingState::GetName() const
{
	static idStr _name(STATE_SEARCHING);
	return _name;
}

bool SearchingState::CheckAlertLevel(idAI* owner)
{
	if (owner->AI_AlertIndex < 3)
	{
		// Alert index is too low for this state, fall back
		owner->Event_CloseHidingSpotSearch();
		owner->GetMind()->EndState();
		return false;
	}
	else if (owner->AI_AlertIndex > 3)
	{
		// Alert index is too high, switch to the higher State
		owner->GetMind()->PushState(owner->backboneStates[EAgitatedSearching]);
		return false;
	}

	// Alert Index is matching, return OK
	return true;
}

void SearchingState::Init(idAI* owner)
{
	// Init base class first
	State::Init(owner);

	DM_LOG(LC_AI, LT_INFO)LOGSTRING("SearchingState initialised.\r");
	assert(owner);

	// Ensure we are in the correct alert level
	if (!CheckAlertLevel(owner)) return;

	if (owner->GetMoveType() == MOVETYPE_SIT || owner->GetMoveType() == MOVETYPE_SLEEP)
	{
		owner->GetUp();
	}

	// Shortcut reference
	Memory& memory = owner->GetMemory();

	float alertTime = owner->atime3 + owner->atime3_fuzzyness * (gameLocal.random.RandomFloat() - 0.5);

	_alertLevelDecreaseRate = (owner->thresh_4 - owner->thresh_3) / alertTime;

	idStr bark;

	if (owner->AlertIndexIncreased())
	{
		// Setup a new hiding spot search
		StartNewHidingSpotSearch(owner);

		if ((memory.alertedDueToCommunication == false) && ((memory.alertType == EAlertTypeSuspicious) || (memory.alertType == EAlertTypeEnemy)))
		{
			if ((memory.alertClass == EAlertVisual_1) || (memory.alertClass == EAlertVisual_2)) // grayman #2603
			{
				if ( (MS2SEC(gameLocal.time - memory.lastTimeFriendlyAISeen)) <= MAX_FRIEND_SIGHTING_SECONDS_FOR_ACCOMPANIED_ALERT_BARK )
				{
					bark = "snd_alert3sc";
				}
				else
				{
					bark = "snd_alert3s";
				}
			}
			else if (memory.alertClass == EAlertAudio)
			{
				if ( (MS2SEC(gameLocal.time - memory.lastTimeFriendlyAISeen)) <= MAX_FRIEND_SIGHTING_SECONDS_FOR_ACCOMPANIED_ALERT_BARK )
				{
					bark = "snd_alert3hc";
				}
				else
				{
					bark = "snd_alert3h";
				}
			}
			else if ( (MS2SEC(gameLocal.time - memory.lastTimeFriendlyAISeen)) <= MAX_FRIEND_SIGHTING_SECONDS_FOR_ACCOMPANIED_ALERT_BARK )
			{
				bark = "snd_alert3c";
			}
			else
			{
				bark = "snd_alert3";
			}

			// Allocate a singlebarktask, set the sound and enqueue it

			owner->commSubsystem->AddCommTask(
				CommunicationTaskPtr(new SingleBarkTask(bark))
			);
		}
	}
	else if (memory.alertType == EAlertTypeEnemy)
	{
		// clear the alert type, so we can react to other alert types (such as a dead person)
		memory.alertType = EAlertTypeSuspicious;
	}
	
	if (!owner->HasSeenEvidence())
	{
		owner->SheathWeapon();
		owner->UpdateAttachmentContents(false);
	}
	else
	{
		// Let the AI update their weapons (make them solid)
		owner->UpdateAttachmentContents(true);
	}

	memory.searchFlags |= SRCH_WAS_SEARCHING; // grayman #2603
}

void SearchingState::OnSubsystemTaskFinished(idAI* owner, SubsystemId subSystem)
{
/* grayman #2560 - InvestigateSpotTask is the only task this was needed
   for, and this code has been moved to a new OnFinish() for that task. No longer
   needed here.

	Memory& memory = owner->GetMemory();

	if (memory.hidingSpotInvestigationInProgress && subSystem == SubsysAction)
	{
		// The action subsystem has finished investigating the spot, set the
		// boolean back to false, so that the next spot can be chosen
		memory.hidingSpotInvestigationInProgress = false;
	}
 */
}

// Gets called each time the mind is thinking
void SearchingState::Think(idAI* owner)
{
	UpdateAlertLevel();

	// Ensure we are in the correct alert level
	if (!CheckAlertLevel(owner)) return;

	if (owner->GetMoveType() != MOVETYPE_ANIM)
	{
		owner->GetUp();
		return;
	}


	Memory& memory = owner->GetMemory();

	// Do we have an ongoing hiding spot search?
	if (!memory.hidingSpotSearchDone)
	{
		// Let the hiding spot search do its task
		PerformHidingSpotSearch(owner);

		// Let the AI check its senses
		owner->PerformVisualScan();
/*
		// angua: commented this out, problems with getting up from sitting
		idStr waitState(owner->WaitState());
		if (waitState.IsEmpty())
		{
			// Waitstate is not matching, this means that the animation 
			// can be started.
			owner->SetAnimState(ANIMCHANNEL_TORSO, "Torso_LookAround", 5);
			//owner->SetAnimState(ANIMCHANNEL_LEGS, "Legs_LookAround", 5);

			// Set the waitstate, this gets cleared by 
			// the script function when the animation is done.
			owner->SetWaitState("look_around");
		}
*/

	}
	// Is a hiding spot search in progress?
	else if (!memory.hidingSpotInvestigationInProgress)
	{
		// Spot search and investigation done, what next?

		// Have run out of hiding spots?
		if (memory.noMoreHidingSpots) 
		{
			idVec3 randomOffset(gameLocal.random.RandomFloat()*200.0f - 100, gameLocal.random.RandomFloat()*200.0f - 100, 0);
			memory.currentSearchSpot = memory.alertPos + randomOffset;

			// Choose to investigate spots closely on a random basis
			// grayman #2801 - and only if you weren't hit by a projectile

			memory.investigateStimulusLocationClosely = ( ( gameLocal.random.RandomFloat() < 0.3f ) && ( memory.alertType != EAlertTypeDamage ) );

			owner->actionSubsystem->PushTask(
				TaskPtr(InvestigateSpotTask::CreateInstance())
			);
			//gameRenderWorld->DebugArrow(colorBlue, owner->GetEyePosition(), memory.currentSearchSpot, 1, 2000);

			// Set the flag to TRUE, so that the sensory scan can be performed
			memory.hidingSpotInvestigationInProgress = true;
		}
		// We should have more hiding spots, try to get the next one
		else if (!ChooseNextHidingSpotToSearch(owner))
		{
			// No more hiding spots to search
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("No more hiding spots!\r");

			// Stop moving, the algorithm will choose another spot the next round
			owner->StopMove(MOVE_STATUS_DONE);
			memory.stopRelight = true; // grayman #2603 - abort a relight in progress
			memory.stopExaminingRope = true; // grayman #2872 - stop examining rope
		}
		else
		{
			// ChooseNextHidingSpot returned TRUE, so we have memory.currentSearchSpot set

			//gameRenderWorld->DebugArrow(colorBlue, owner->GetEyePosition(), memory.currentSearchSpot, 1, 2000);

			// Delegate the spot investigation to a new task, this will take the correct action.
			owner->actionSubsystem->PushTask(InvestigateSpotTask::CreateInstance());

			// Prevent falling into the same hole twice
			memory.hidingSpotInvestigationInProgress = true;
		}
	}
	else if (memory.restartSearchForHidingSpots)
	{
		// We should restart the search (probably due to a new incoming stimulus)
		// Setup a new hiding spot search
		StartNewHidingSpotSearch(owner);
	}
	else
	{
		// Move to Hiding spot is ongoing, do additional sensory tasks here

		// Let the AI check its senses
		owner->PerformVisualScan();
	}
}

void SearchingState::StartNewHidingSpotSearch(idAI* owner)
{
	Memory& memory = owner->GetMemory();

	// Clear the flag in any case
	memory.restartSearchForHidingSpots = false;
	memory.noMoreHidingSpots = false;

	// Clear all the ongoing tasks
	owner->senseSubsystem->ClearTasks();
	owner->actionSubsystem->ClearTasks();
	owner->movementSubsystem->ClearTasks();

	// Stop moving
	owner->StopMove(MOVE_STATUS_DONE);
	memory.stopRelight = true; // grayman #2603 - abort a relight in progress
	memory.stopExaminingRope = true; // grayman #2872 - stop examining rope

	// If we are supposed to search the stimulus location do that instead 
	// of just standing around while the search completes
	if (memory.stimulusLocationItselfShouldBeSearched)
	{
		// The InvestigateSpotTask will take this point as first hiding spot
		memory.currentSearchSpot = memory.alertPos;

		// Delegate the spot investigation to a new task, this will take the correct action.
		owner->actionSubsystem->PushTask(
			TaskPtr(new InvestigateSpotTask(memory.investigateStimulusLocationClosely))
		);

		// Prevent overwriting this hiding spot in the upcoming Think() call
		memory.hidingSpotInvestigationInProgress = true;

		// Reset the flags
		memory.stimulusLocationItselfShouldBeSearched = false;
		memory.investigateStimulusLocationClosely = false;
	}
	else
	{
		// AI is not moving, wait for spot search to complete
		memory.hidingSpotInvestigationInProgress = false;
		memory.currentSearchSpot = idVec3(idMath::INFINITY, idMath::INFINITY, idMath::INFINITY);
	}

	idVec3 minBounds(memory.alertPos - memory.alertSearchVolume);
	idVec3 maxBounds(memory.alertPos + memory.alertSearchVolume);

	idVec3 minExclusionBounds(memory.alertPos - memory.alertSearchExclusionVolume);
	idVec3 maxExclusionBounds(memory.alertPos + memory.alertSearchExclusionVolume);
	
	// Close any previous search
	owner->Event_CloseHidingSpotSearch();

	// Hiding spot test now started
	memory.hidingSpotSearchDone = false;
	memory.hidingSpotTestStarted = true;

	// Invalidate the vector, clear the indices
	memory.firstChosenHidingSpotIndex = -1;
	memory.currentChosenHidingSpotIndex = -1;

	// Start search
	int res = owner->StartSearchForHidingSpotsWithExclusionArea(
		owner->GetEyePosition(), 
		minBounds, maxBounds, 
		minExclusionBounds, maxExclusionBounds, 
		255, owner
	);

	if (res == 0)
	{
		// Search completed on first round
		memory.hidingSpotSearchDone = true;
	}
}

void SearchingState::PerformHidingSpotSearch(idAI* owner)
{
	// Shortcut reference
	Memory& memory = owner->GetMemory();

	// Increase the frame count
	memory.hidingSpotThinkFrameCount++;

	if (owner->ContinueSearchForHidingSpots() == 0)
	{
		// search completed
		memory.hidingSpotSearchDone = true;

		// Hiding spot test is done
		memory.hidingSpotTestStarted = false;
		
		// Here we transition to the state for handling the behavior of
		// the AI once it thinks it knows where the stimulus may have
		// come from
		
		// For now, starts searching for the stimulus.  We probably want
		// a way for different AIs to act differently
		// TODO: Morale check etc...

		// Get location
		memory.chosenHidingSpot = owner->GetNthHidingSpotLocation(memory.currentChosenHidingSpotIndex);
	}
}

bool SearchingState::ChooseNextHidingSpotToSearch(idAI* owner)
{
	Memory& memory = owner->GetMemory();

	int numSpots = owner->m_hidingSpots.getNumSpots();
	DM_LOG(LC_AI, LT_INFO)LOGSTRING("Found hidings spots: %d\r", numSpots);

	// Choose randomly
	if (numSpots > 0)
	{
		if (memory.firstChosenHidingSpotIndex == -1)
		{
			// No hiding spot chosen yet, initialise
			/*
			// Get visual acuity
			float visAcuity = getAcuity("vis");
				
			// Since earlier hiding spots are "better" (ie closer to stimulus and darker or more occluded)
			// higher visual acuity should bias toward earlier in the list
			float bias = 1.0 - visAcuity;
			if (bias < 0.0)
			{
				bias = 0.0;
			}
			*/
			// greebo: TODO? This isn't random choosing...

			int spotIndex = 0; 

			// Remember which hiding spot we have chosen at start
			memory.firstChosenHidingSpotIndex = spotIndex;
				
			// Note currently chosen hiding spot
			memory.currentChosenHidingSpotIndex = spotIndex;
			
			// Get location
			memory.chosenHidingSpot = owner->GetNthHidingSpotLocation(spotIndex);
			memory.currentSearchSpot = memory.chosenHidingSpot;

			DM_LOG(LC_AI, LT_INFO)LOGSTRING(
				"First spot chosen is index %d of %d spots.\r", 
				memory.firstChosenHidingSpotIndex, numSpots
			);
		}
		else 
		{
			// Make sure we stay in bounds
			memory.currentChosenHidingSpotIndex++;
			if (memory.currentChosenHidingSpotIndex >= numSpots)
			{
				memory.currentChosenHidingSpotIndex = 0;
			}

			// Have we wrapped around to first one searched?
			if (memory.currentChosenHidingSpotIndex == memory.firstChosenHidingSpotIndex || 
				memory.currentChosenHidingSpotIndex < 0)
			{
				// No more hiding spots
				DM_LOG(LC_AI, LT_INFO)LOGSTRING("No more hiding spots to search.\r");
				memory.hidingSpotSearchDone = false;
				memory.chosenHidingSpot = idVec3(idMath::INFINITY, idMath::INFINITY, idMath::INFINITY);
				memory.currentChosenHidingSpotIndex = -1;
				memory.firstChosenHidingSpotIndex = -1;
				memory.noMoreHidingSpots = true;
				return false;
			}
			else
			{
				// Index is valid, let's acquire the position
				DM_LOG(LC_AI, LT_INFO)LOGSTRING("Next spot chosen is index %d of %d, first was %d.\r", 
					memory.currentChosenHidingSpotIndex, numSpots-1, memory.firstChosenHidingSpotIndex);

				memory.chosenHidingSpot = owner->GetNthHidingSpotLocation(memory.currentChosenHidingSpotIndex);
				memory.currentSearchSpot = memory.chosenHidingSpot;
				memory.hidingSpotSearchDone = true;
			}
		}
	}
	else
	{
		DM_LOG(LC_AI, LT_INFO)LOGSTRING("Didn't find any hiding spots near stimulus.\r");
		memory.firstChosenHidingSpotIndex = -1;
		memory.currentChosenHidingSpotIndex = -1;
		memory.chosenHidingSpot = idVec3(idMath::INFINITY, idMath::INFINITY, idMath::INFINITY);
		memory.currentSearchSpot = idVec3(idMath::INFINITY, idMath::INFINITY, idMath::INFINITY);
		memory.noMoreHidingSpots = true;
		return false;
	}

	return true;
}

void SearchingState::OnAudioAlert()
{
	// First, call the base class
	State::OnAudioAlert();

	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);
	Memory& memory = owner->GetMemory();

	if (!memory.alertPos.Compare(memory.currentSearchSpot, 50))
	{
		// The position of the sound is different to the current search spot, redefine the goal
		TaskPtr curTask = owner->actionSubsystem->GetCurrentTask();
		InvestigateSpotTaskPtr spotTask = boost::dynamic_pointer_cast<InvestigateSpotTask>(curTask);
			
		if (spotTask != NULL)
		{
			// Redirect the owner to a new position
			spotTask->SetNewGoal(memory.alertPos);
			spotTask->SetInvestigateClosely(false);

			//gameRenderWorld->DebugArrow(colorRed, owner->GetEyePosition(), memory.alertPos, 1, 300);
		}
	}

	if (memory.alertSearchCenter != idVec3(idMath::INFINITY, idMath::INFINITY, idMath::INFINITY))
	{
		// We have a valid search center
		float distanceToSearchCenter = (memory.alertPos - memory.alertSearchCenter).LengthSqr();
		if (distanceToSearchCenter > memory.alertSearchVolume.LengthSqr())
		{
			// The alert position is far from the current search center, restart search
			memory.restartSearchForHidingSpots = true;

			//gameRenderWorld->DebugArrow(colorRed, owner->GetEyePosition(), memory.alertPos, 1, 2000);
		}
	}
}

StatePtr SearchingState::CreateInstance()
{
	return StatePtr(new SearchingState);
}

// Register this state with the StateLibrary
StateLibrary::Registrar searchingStateRegistrar(
	STATE_SEARCHING, // Task Name
	StateLibrary::CreateInstanceFunc(&SearchingState::CreateInstance) // Instance creation callback
);

} // namespace ai
