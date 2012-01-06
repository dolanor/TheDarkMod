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

#ifndef __AI_MEMORY_H__
#define __AI_MEMORY_H__

#include "precompiled_game.h"
#include "../BinaryFrobMover.h"
#include "../FrobDoor.h"
#include "DoorInfo.h"
#include "AI.h"

namespace ai
{

#define AIUSE_BLOOD_EVIDENCE	"AIUSE_BLOOD_EVIDENCE"
#define AIUSE_BROKEN_ITEM		"AIUSE_BROKEN_ITEM"
#define AIUSE_CARRY				"AIUSE_CARRY"		// for specific animations
#define AIUSE_CATTLE			"AIUSE_CATTLE"
#define AIUSE_COOK				"AIUSE_COOK"
#define AIUSE_DRINK				"AIUSE_DRINK"
#define AIUSE_DOOR				"AIUSE_DOOR"
#define AIUSE_ELEVATOR			"AIUSE_ELEVATOR"
#define AIUSE_EAT				"AIUSE_EAT"
#define AIUSE_KEY				"AIUSE_KEY"			// Tels: for specific animations
#define AIUSE_LIGHTSOURCE		"AIUSE_LIGHTSOURCE"
#define AIUSE_MISSING_ITEM_MARKER "AIUSE_MISSING_ITEM_MARKER"
#define AIUSE_MONSTER			"AIUSE_MONSTER"		// a random or caged monster, not a pet
#define AIUSE_PERSON			"AIUSE_PERSON"
#define AIUSE_PEST				"AIUSE_PEST"
#define AIUSE_PET				"AIUSE_PET"
#define AIUSE_READ				"AIUSE_READ"		// Tels: for specific animations
#define AIUSE_SEAT				"AIUSE_SEAT"
#define AIUSE_STEAMBOT			"AIUSE_STEAMBOT"	// steambots
#define AIUSE_UNDEAD			"AIUSE_UNDEAD"		// An undead creature
#define AIUSE_WEAPON			"AIUSE_WEAPON"
#define AIUSE_SUSPICIOUS		"AIUSE_SUSPICIOUS"	// grayman #1327
#define AIUSE_ROPE				"AIUSE_ROPE"		// grayman #2872

//----------------------------------------------------------------------------------------
// The following key and values are used for identifying types of lights
#define AIUSE_LIGHTTYPE_KEY		"lightType"
#define AIUSE_LIGHTTYPE_TORCH	"AIUSE_LIGHTTYPE_TORCH"
#define AIUSE_LIGHTTYPE_GASLAMP	"AIUSE_LIGHTTYPE_GASLAMP"
#define AIUSE_LIGHTTYPE_ELECTRIC "AIUSE_LIGHTTYPE_ELECTRIC"
#define AIUSE_LIGHTTYPE_MAGIC	"AIUSE_LIGHTTYPE_MAGIC"
#define AIUSE_LIGHTTYPE_AMBIENT	"AIUSE_LIGHTTYPE_AMBIENT"

//----------------------------------------------------------------------------------------
// The following key is used to identify the name of the switch entity used to turn on
// a AIUSE_LIGHTTYPE_ELECTRIC light.
// grayman #2603 - not in use; switches are found at map start
#define AIUSE_LIGHTSWITCH_NAME_KEY	"switchName"

//----------------------------------------------------------------------------------------
// The following defines the response type when a light is off
#define AIUSE_SHOULDBEON_LEVEL		"shouldBeOn" // grayman #2603

// SZ: Minimum count evidence of intruders to turn on all lights encountered
// grayman #2603 - this is no longer used
#define MIN_EVIDENCE_OF_INTRUDERS_TO_TURN_ON_ALL_LIGHTS 5
// angua: The AI starts searching after encountering a switched off light 
// only if it is already suspicious
#define MIN_EVIDENCE_OF_INTRUDERS_TO_SEARCH_ON_LIGHT_OFF 3
// SZ: Minimum count of evidence of intruders to communicate suspicion to others
#define MIN_EVIDENCE_OF_INTRUDERS_TO_COMMUNICATE_SUSPICION 3

// SZ: Someone hearing a distress call won't bother to shout that they are coming to their assistance unless
// it is at least this far away. This is to simulate more natural human behavior.
#define MIN_DISTANCE_TO_ISSUER_TO_SHOUT_COMING_TO_ASSISTANCE 200

// Considered cause radius around a tactile event
#define TACTILE_ALERT_RADIUS 10.0f
#define TACTILE_SEARCH_VOLUME idVec3(40,40,40)

// Considered cause radius around a visual event
#define VISUAL_ALERT_RADIUS 25.0f
#define VISUAL_SEARCH_VOLUME idVec3(100,100,100)

// Considered cause radius around an audio event
#define AUDIO_ALERT_RADIUS 50.0f
#define AUDIO_ALERT_FUZZINESS 100.0f
#define AUDIO_SEARCH_VOLUME idVec3(300,300,200)

// Area searched around last sighting after losing an enemy
#define LOST_ENEMY_ALERT_RADIUS 200.0
#define LOST_ENEMY_SEARCH_VOLUME idVec3(200, 200, 100) // grayman #2603 - was (200,200,200)

enum EAlertClass 
{
	EAlertNone,
	EAlertVisual_1,
	EAlertVisual_2, // grayman #2603
	EAlertTactile,
	EAlertAudio,
	EAlertClassCount
};

enum EAlertType
{
	EAlertTypeNone,
	EAlertTypeSuspicious,
	EAlertTypeEnemy,
	EAlertTypeWeapon,
	EAlertTypeDeadPerson,
	EAlertTypeUnconsciousPerson,
	EAlertTypeBlood,
	EAlertTypeLightSource,
	EAlertTypeMissingItem,
	EAlertTypeBrokenItem,
	EAlertTypeDoor,
	EAlertTypeDamage,
	EAlertTypeSuspiciousItem,	// grayman #1327
	EAlertTypeRope,				// grayman #2872
	EAlertTypeCount
};

// The alert index the AI is in
enum EAlertState
{
	ERelaxed = 0,
	EObservant,
	ESuspicious,
	EInvestigating,
	EAgitatedSearching,
	ECombat,
	EAlertStateNum
};

const char* const AlertStateNames[EAlertStateNum] = 
{
	"Relaxed",
	"Observant",
	"Suspicious",
	"Investigating",
	"AgitatedSearching",
	"Combat"
};

#define MINIMUM_SECONDS_BETWEEN_STIMULUS_BARKS 15000 // milliseconds

// SZ: Maximum amount of time since last visual or audio contact with a friendly person to use
// group stimulous barks, in seconds
#define MAX_FRIEND_SIGHTING_SECONDS_FOR_ACCOMPANIED_ALERT_BARK 10.0f

// TODO: Parameterize these as darkmod globals
#define HIDING_OBJECT_HEIGHT 0.35f
#define MAX_SPOTS_PER_SEARCH_CALL 100

// The maximum time the AI is able to follow the enemy although it's invisible
#define MAX_BLIND_CHASE_TIME 3000

// grayman #2603 - how long to wait until barking again about a light that's out
#define REBARK_DELAY 15000

// grayman #2603 - search flags
// grayman #1327 - broke single warning flag into specific warning flags
#define SRCH_WAS_SEARCHING			1	// set when searching occurred while alert
//#define SRCH_WARNED_ENEMY			2	// set when warned by another AI that an enemy was seen
//#define SRCH_WARNED_CORPSE			4	// set when warned by another AI that someone died
//#define SRCH_WARNED_MISSING_ITEM	8	// set when warned by another AI that something was stolen
//#define SRCH_WARNED_EVIDENCE		16	// set when warned by another AI that evidence is mounting
//#define SRCH_WARNED ( SRCH_WARNED_ENEMY | SRCH_WARNED_CORPSE | SRCH_WARNED_MISSING_ITEM | SRCH_WARNED_EVIDENCE )

/**
 * greebo: This class acts as container for all kinds of state variables.
 */
class Memory
{
public:
	// The owning AI
	idAI* owner;

	// The path entity we're supposed to be heading to
	idEntityPtr<idPathCorner> currentPath;

	// Our next path entity
	idEntityPtr<idPathCorner> nextPath;

	// Our last path entity
	idEntityPtr<idPathCorner> lastPath;

	// The game time, the AlertLevel was last increased.
	int lastAlertRiseTime;

	// Time in msecs to pass before alert level starts decreasing
	int deadTimeAfterAlertRise;

	// The last time the AI has been barking when patrolling
	int lastPatrolChatTime;

	int	lastTimeFriendlyAISeen;

	// This is the last time the enemy was visible
	int	lastTimeEnemySeen;

	// The last time a visual stim made the AI bark
	int lastTimeVisualStimBark;

	// grayman #2603 - The next time a light stim can make the AI bark
	int nextTimeLightStimBark;

	// grayman #2603 - flags relevant to searching
	int searchFlags;

	/*!
	* This variable indicates the number of out of place things that the
	* AI has witness, such as sounds, missing items, open doors, torches gone
	* out etc..
	*/
	int countEvidenceOfIntruders;

	// Random head turning
	int nextHeadTurnCheckTime;
	bool currentlyHeadTurning;
	int headTurnEndTime;

	idVec3 idlePosition;
	float idleYaw;

	// angua: whether the AI should play idle animations
	bool playIdleAnimations;

	// TRUE if enemies have been seen
	bool enemiesHaveBeenSeen;

	// TRUE if the enemy has shot/hurt/attempted blackjacking
	bool hasBeenAttackedByEnemy;

	// TRUE if the AI knows that items have been stolen
	bool itemsHaveBeenStolen;

	// TRUE if the AI has found something broken
	bool itemsHaveBeenBroken;

	// TRUE if the AI has found a dead or unconscious person
	bool unconsciousPeopleHaveBeenFound;
	bool deadPeopleHaveBeenFound;

	// position of alert causing stimulus
	idVec3 alertPos;

	// grayman #2903 - positions of AI when there's an alert that can lead to warnings between AI
	idVec3 posEnemySeen;
	idVec3 posCorpseFound;
	idVec3 posMissingItem;
	idVec3 posEvidenceIntruders;

	// grayman #2903 - timestamps of alerts that can lead to warnings between AI
	int timeEnemySeen;
	int timeCorpseFound;
	int timeMissingItem;
	int timeEvidenceIntruders;

	// grayman #2603 - abort an ongoing light relight?
	bool stopRelight;

	// grayman #2872 - abort a rope examination?
	bool stopExaminingRope;

	// grayman #2603 - a light that's being relit
	idEntityPtr<idLight> relightLight;

	// Type of alert (visual, tactile, audio)
	EAlertClass alertClass;

	// Source of the alert (enemy, weapon, blood, dead person, etc.)
	EAlertType alertType;

	// radius of alert causing stimulus (depends on the type and distance)
	float alertRadius;

	// The last time we had an incoming audio alert
	int lastAudioAlertTime;

	// This is true if the original alert position is to be searched
	bool stimulusLocationItselfShouldBeSearched;

	// Set this to TRUE if stimulus location itself should be closely investigated (kneel down)
	bool investigateStimulusLocationClosely;

	// This flag indicates if the last alert is due to a communication message
	bool alertedDueToCommunication;

	// Position of the last alert causing stimulus which was searched.
    // This is used to compare new stimuli to the previous stimuli searched
    // to determine if a new search is necessary
	idVec3 lastAlertPosSearched;

	// greebo: This is the position of the alert that was used to set up a hiding spot search.
	idVec3 alertSearchCenter;

	// A search area vector that is m_alertRadius on each side
	idVec3 alertSearchVolume;

	// An area within the search volume that is to be ignored. It is used for expanding
	// radius searches that don't re-search the inner points.
	idVec3 alertSearchExclusionVolume;

	// The last position the enemy was seen
	// greebo: Note: Currently this is filled in before fleeing only.
	idVec3 lastEnemyPos;

	// This is set to TRUE by the sensory routines to indicate whether
	// the AI is in the position to damage the player.
	// This flag is mostly for caching purposes so that the subsystem tasks
	// don't need to query idAI::CanHitEnemy() independently.
	bool canHitEnemy;
	// ishtvan: Whether we will be able to hit the enemy in the future if we
	// start a melee attack right now.
	// If the AI checks this, the CanHitEnemy query sets it as well as canHitEnemy
	bool willBeAbleToHitEnemy;

	/**
	* When true, an enemy can potentially hit us with a melee attack in the near future
	**/
	bool canBeHitByEnemy;

	/*!
	* These hold the current spot search target, regardless of whether
	* or not it is a hiding spot search or some other sort of spot search
	*/
	idVec3 currentSearchSpot;

	/*!
	* This flag indicates if a hiding spot test was started
	* @author SophisticatedZombie
	*/
	bool hidingSpotTestStarted;

	/*!
	* This flag idnicates if a hiding spot was chosen
	*/
	bool hidingSpotSearchDone;

	/**
	 * greebo: TRUE if a hiding spot search has been started and 
	 * the AI has searched all of them already. 
	 */
	bool noMoreHidingSpots;

	/**
	 * greebo: This is queried by the SearchStates and indicates a new
	 *         stimulus to be considered.
	 */
	bool restartSearchForHidingSpots;

	// This counts the number of frames we have been thinking, in case
	// we have a problem with hiding spot searches not returning
	int hidingSpotThinkFrameCount;

	int firstChosenHidingSpotIndex;
	int currentChosenHidingSpotIndex;
	idVec3 chosenHidingSpot;

	// True if the AI is currently investigating a hiding spot (walking to it, for instance).
	bool hidingSpotInvestigationInProgress;

	// True if fleeing is done, false if fleeing is in progress
	bool fleeingDone;

	// angua: The last position of the AI before it takes cover, so it can return to it later.
	idVec3 positionBeforeTakingCover;

	// TRUE when the AI is currently trying to resolve a block
	bool resolvingMovementBlock;

	// grayman #2712 - last door handled
	idEntityPtr<CFrobDoor> lastDoorHandled;

	// grayman #2866 - start of changes
	idEntityPtr<CFrobDoor> closeMe;	// a door that should be closed
	idEntityPtr<idEntity> frontPos;	// the door handling position closest to the AI
	idEntityPtr<idEntity> backPos;	// the door handling position closest to the AI
	bool closeSuspiciousDoor;		// need to close a suspicious door
	bool doorSwingsToward;			// does the door swing toward me?
	bool closeFromAwayPos;			// do we close a suspicious door from the near side or far side? 
	bool susDoorSameAsCurrentDoor;	// the door you're handling sends you a visual stim
	float savedAlertLevelDecreaseRate; // used w/door handling in Observant state
	// end of #2866 changes

	// Maps doors to info structures
	typedef std::map<CFrobDoor*, DoorInfoPtr> DoorInfoMap;
	// This maps AAS area numbers to door info structures
	typedef std::map<int, DoorInfoPtr> AreaToDoorInfoMap;

	// Variables related to door opening/closing process
	struct DoorRelatedVariables
	{
		// The door we're currently handling (e.g. during HandleDoorTask)
		idEntityPtr<CFrobDoor> currentDoor;

		// This maps CFrobDoor* pointers to door info structures
		DoorInfoMap doorInfo;

		// This allows for quick lookup of info structures via area number
		AreaToDoorInfoMap areaDoorInfoMap;
	} doorRelated;

	struct GreetingInfo
	{
		// The last time the associated AI was greeted by the owner
		int lastGreetingTime;

		// The last time the actor was met and considered for greeting
		// the actual greeting doesn't have to be performed
		// This is used to "ignore" visual stims for greeting for a while
		int lastConsiderTime;

		GreetingInfo() :
			lastGreetingTime(-1),
			lastConsiderTime(-1)
		{}
	};

	typedef std::map<idActor*, GreetingInfo> ActorGreetingInfoMap;
	ActorGreetingInfoMap greetingInfo;

	// Pass the owning AI to the constructor
	Memory(idAI* owningAI);

	// Save/Restore routines
	void Save(idSaveGame* savefile) const;
	void Restore(idRestoreGame* savefile);

	/**
	 * greebo: Returns the door info structure for the given door. 
	 * This will create a new info structure if it doesn't exist yet, so the reference is always valid.
	 */
	DoorInfo& GetDoorInfo(CFrobDoor* door);

	// Similar to above, but use the area number as input argument, can return NULL
	DoorInfoPtr GetDoorInfo(int areaNum);

	// Returns the greeting info structure for the given actor
	GreetingInfo& GetGreetingInfo(idActor* actor);
};

} // namespace ai

#endif /*__AI_MEMORY_H__*/
