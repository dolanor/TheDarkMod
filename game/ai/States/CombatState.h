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

#ifndef __AI_COMBAT_STATE_H__
#define __AI_COMBAT_STATE_H__

#include "../AI.h"
#include "State.h"

namespace ai
{

#define STATE_COMBAT "Combat"

class CombatState :
	public State
{
protected:
	// The AI's enemy
	idEntityPtr<idActor> _enemy;
	int _criticalHealth;
	bool _meleePossible;
	bool _rangedPossible;

	ECombatType _combatType;

	// When end time is set, the state is just waiting to be finished
	// and is not performing any routines anymore
	int _endTime;

	int _reactionEndTime; // grayman #3063

	// grayman #3063 - break Combat State into smaller bits

	enum ECombatSubState {
		EStateReaction,
		EStateInit,
		EStateThink
	} _combatSubState;

public:
	// Get the name of this state
	virtual const idStr& GetName() const;

	// This is called when the state is first attached to the AI's Mind.
	virtual void Init(idAI* owner);

	// Gets called each time the mind is thinking
	virtual void Think(idAI* owner);

	// Override the alert functions
	virtual void OnTactileAlert(idEntity* tactEnt);
	virtual void OnVisualAlert(idActor* enemy);
	virtual void OnAudioAlert();

	virtual void OnPersonEncounter(idEntity* stimSource, idAI* owner);
	virtual void OnFailedKnockoutBlow(idEntity* attacker, const idVec3& direction, bool hitHead);

	// grayman #3317 - It's possible that an AI will enter Combat mode after encountering a
	// dead or KO'ed AI, but before the post processing for those events occurs. If that happens,
	// these methods will catch the post processing and abort it.
	virtual void Post_OnDeadPersonEncounter(idActor* person, idAI* owner);
	virtual void Post_OnUnconsciousPersonEncounter(idActor* person, idAI* owner);
	
	// Save/Restore methods
	virtual void Save(idSaveGame* savefile) const;
	virtual void Restore(idRestoreGame* savefile);

	static StatePtr CreateInstance();

protected:
	// Override base class method
	virtual bool CheckAlertLevel(idAI* owner);

	// Checks enemy status (dead, visible, not an enemy anymore).
	// Returns false if the enemy is not applicable anymore and the state has ended
	bool CheckEnemyStatus(idActor* enemy, idAI* owner);
};

} // namespace ai

#endif /* __AI_COMBAT_STATE_H__ */
