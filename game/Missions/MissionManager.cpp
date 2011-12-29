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

#include "../../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id$", init_version);

#include <time.h>
#include "MissionManager.h"
#include "MissionDB.h"
#include "../ZipLoader/ZipLoader.h"
#include "../Inventory/Inventory.h"

#include "DownloadManager.h"
#include "../Http/HttpConnection.h"
#include "../Http/HttpRequest.h"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace
{
	const char* const TMP_MISSION_LIST_FILENAME = "__missionlist.xml.temp";
	const char* const TMP_MISSION_DETAILS_FILENAME = "__missiondetails.xml.temp";
	const char* const TMP_MISSION_SCREENSHOT_FILENAME = "__missionscreenshot.temp";
}

CMissionManager::CMissionManager() :
	_missionDB(new CMissionDB),
	_curMissionIndex(0),
	_refreshModListDownloadId(-1),
	_modDetailsDownloadId(-1),
	_modScreenshotDownloadId(-1)
{}

CMissionManager::~CMissionManager()
{
	// Clear contents and the list elements themselves
	_downloadableMods.DeleteContents(true);

	Shutdown();
}

void CMissionManager::Init()
{
	// (Re-)generate mod list on start
	ReloadModList();

	// greebo: Now that any new PK4 files have been copied/moved,
	// reload the mission database.
	_missionDB->Init();

	InitStartingMap();
	InitMapSequence();
}

void CMissionManager::Save(idSaveGame* savefile) const
{
	savefile->WriteInt(_curMissionIndex);
}

void CMissionManager::Restore(idRestoreGame* savefile)
{
	savefile->ReadInt(_curMissionIndex);
}

void CMissionManager::Shutdown()
{
	_missionDB->Save();
}

// Returns the number of available missions
int CMissionManager::GetNumMods()
{
	return _availableMods.Num();
}

CModInfoPtr CMissionManager::GetModInfo(int index)
{
	if (index < 0 || index >= _availableMods.Num())
	{
		return CModInfoPtr(); // out of bounds
	}

	// Pass the call to the getbyname method
	return GetModInfo(_availableMods[index]);
}

CModInfoPtr CMissionManager::GetModInfo(const idStr& name)
{
	return _missionDB->GetModInfo(name);
}

void CMissionManager::EraseModFolder(const idStr& name)
{
	CModInfoPtr info = GetModInfo(name);

	if (info == NULL)
	{
		DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Cannot erase mission folder for mod %s, mission info not found\r", name.c_str());
		return;
	}

	// Delete folder contents
	fs::path missionPath = info->GetModFolderPath().c_str();

	if (fs::exists(missionPath))
	{
		fs::remove_all(missionPath);
	}
	else
	{
		DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Cannot erase mission folder for mod %s, mission folder not found\r", missionPath.file_string().c_str());
		return;
	}

	info->ClearModFolderSize();
}

void CMissionManager::OnMissionStart()
{
	CModInfoPtr info = GetCurrentModInfo();

	if (info == NULL)
	{
		DM_LOG(LC_MAINMENU, LT_ERROR)LOGSTRING("Could not find mission info for current mod.\r");
		return;
	}

	time_t seconds;
	tm* timeInfo;

	seconds = time(NULL);
	timeInfo = localtime(&seconds);

	// Mark the current difficulty level as completed
	info->SetKeyValue("last_play_date", va("%d-%02d-%02d", timeInfo->tm_year + 1900, timeInfo->tm_mon + 1, timeInfo->tm_mday));
}

void CMissionManager::OnMissionComplete()
{
	CModInfoPtr info = GetCurrentModInfo();

	if (info == NULL)
	{
		DM_LOG(LC_MAINMENU, LT_ERROR)LOGSTRING("Could not find mission info for current mod.\r");
		return;
	}

	// Ensure that this was the last mission if in campaign mode, otherwise ignore this call
	if (CurrentModIsCampaign())
	{
		if (_curMissionIndex == -1)
		{
			gameLocal.Error("Invalid mission index in OnMissionComplete()");
		}

		if (_curMissionIndex < _mapSequence.Num() - 1)
		{
			// This is not yet the last mission in the campaign, ignore this call
			return;
		}
	}

	// Mark the current difficulty level as completed
	info->SetKeyValue(va("mission_completed_%d", gameLocal.m_DifficultyManager.GetDifficultyLevel()), "1");

	idPlayer* player = gameLocal.GetLocalPlayer();

	if (player != NULL)
	{
		int gold, jewelry, goods;
		int total = player->Inventory()->GetLoot(gold, jewelry, goods);

		info->SetKeyValue(va("mission_loot_collected_%d", gameLocal.m_DifficultyManager.GetDifficultyLevel()), idStr(total));
	}
}

CModInfoPtr CMissionManager::GetCurrentModInfo()
{
	idStr gameBase = cvarSystem->GetCVarString("fs_game_base");

	// We only have a mod if game_base is set correctly, otherwise we're in "darkmod".
	idStr curMission = (!gameBase.IsEmpty()) ? cvarSystem->GetCVarString("fs_game") : "";

	if (curMission.IsEmpty() || curMission == "darkmod") 
	{
		// return NULL when no mission is installed or "darkmod"
		return CModInfoPtr();
	}

	return GetModInfo(curMission);
}

idStr CMissionManager::GetCurrentModName()
{
	CModInfoPtr info = GetCurrentModInfo();

	return (info != NULL) ? idStr( gameLocal.m_I18N->Translate( info->modName ) ) : "";
}

int CMissionManager::GetNumNewMods()
{
	return _newFoundMods.Num();
}

idStr CMissionManager::GetNewFoundModsText()
{
	if (_newFoundMods.Num() == 0)
	{
		return "";
	}

	idStr text;

	for (int i = 0; i < _newFoundMods.Num(); ++i)
	{
		CModInfoPtr info = GetModInfo(_newFoundMods[i]);

		if (info == NULL) continue;

		text += (text.IsEmpty()) ? "" : "\n";
		text += info->displayName;

		if (i == 1 && _newFoundMods.Num() > 3)
		{
			// Truncate the text
			int rest = _newFoundMods.Num() - (i + 1);
			text += va("\nAnd %d more mission%s.", rest, rest == 1 ? "" : "s");

			break;
		}
	}

	return text;
}

void CMissionManager::ClearNewModList()
{
	_newFoundMods.Clear();
}

void CMissionManager::SearchForNewMods()
{
	// List all PK4s in the fms/ directory
	MoveList moveList = SearchForNewMods(".pk4");
	MoveList zipMoveList = SearchForNewMods(".zip");

	// Merge the zips into the pk4 list
	if (!zipMoveList.empty())
	{
		moveList.merge(zipMoveList);
	}

	DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Found %d new mission packages.\r", static_cast<int>(moveList.size()));
	gameLocal.Printf("Found %d new mission packages.\n", static_cast<int>(moveList.size()));

	// greebo: The D3 engine should no longer hold locks on those files
	// and we can start moving them into their respective locations
	for (MoveList::const_iterator i = moveList.begin(); i != moveList.end(); ++i)
	{
		fs::path targetPath = i->second;

		// Remove any target file first, to overwrite when moving
		DoRemoveFile(targetPath);

		// Move the file
		DoMoveFile(i->first, targetPath);
		
		// Remove the file portion
		targetPath.remove_leaf();

		// Remove any darkmod.txt, splashimage etc. when copying a new PK4. It may contain updated versions of those.
		DoRemoveFile(targetPath / cv_tdm_fm_desc_file.GetString());
		DoRemoveFile(targetPath / cv_tdm_fm_splashimage_file.GetString());
		DoRemoveFile(targetPath / cv_tdm_fm_notes_file.GetString());
	}
}

CMissionManager::MoveList CMissionManager::SearchForNewMods(const idStr& extension)
{
	MoveList moveList;

	fs::path darkmodPath = GetDarkmodPath();

	fs::path fmPath = darkmodPath / cv_tdm_fm_path.GetString();

	DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Looking for %s files in FM root folder: %s\r", extension.c_str(), fmPath.file_string().c_str());

	// greebo: Use boost::filesystem to enumerate new PK4s, idFileSystem::ListFiles might be too unreliable
	// Iterate over all found PK4s and check if they're valid
	for (fs::directory_iterator i = fs::directory_iterator(fmPath); i != fs::directory_iterator(); ++i)
	{
		if (fs::is_directory(*i)) continue;

		fs::path pk4path = *i;

		// Check extension
		idStr extLower = pk4path.extension().c_str();
		extLower.ToLower();

		if (extLower != extension)
		{
			continue;
		}

		DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Found %s in FM root folder: %s\r", extension.c_str(), pk4path.file_string().c_str());

		// Does the PK4 file contain a proper description file?
		CZipFilePtr pk4file = CZipLoader::Instance().OpenFile(pk4path.file_string().c_str());

		if (pk4file == NULL)
		{
			DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Could not open PK4 in root folder: %s\r", pk4path.file_string().c_str());
			continue; // failed to open zip file
		}

		// Check if this is a l10n pack, if yes we need to move it to the same mod folder
		bool isL10nPack = boost::algorithm::iends_with(pk4path.stem(), "_l10n");

		DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("This is a localisation pack: %s\r", isL10nPack ? "yes" : "no");

		// Ordinary missions PK4s require a proper description file in it
		if (!isL10nPack && !pk4file->ContainsFile(cv_tdm_fm_desc_file.GetString()))
		{
			DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Ignoring PK4 file, no 'darkmod.txt' found inside archive: %s\r", pk4path.file_string().c_str());
			continue; // no darkmod.txt
		}

		// Deduce the mod folder name based on the PK4 name
		idStr modName = pk4path.leaf().c_str();
		modName.StripPath();
		modName.StripFileExtension();
		modName.ToLower();

		if (modName.IsEmpty()) continue; // error?

		// l10n packs get moved in the same mod folder as the mission itself
		if (isL10nPack)
		{
			modName.StripTrailingOnce("_l10n");
		}

		// Clean modName string from any weird characters
		for (int i = 0; i < modName.Length(); ++i)
		{
			if (idStr::CharIsAlpha(modName[i]) || idStr::CharIsNumeric(modName[i])) continue;

			modName[i] = '_'; // replace non-ASCII keys with underscores
		}

		// Remember this for the user to display
		if (!isL10nPack)
		{
			_newFoundMods.Append(modName);
		}

		// Assemble the mod folder, e.g. c:/games/doom3/darkmod/fms/outpost
		fs::path modFolder = darkmodPath / cv_tdm_fm_path.GetString() / modName.c_str();

		// Create the fm folder, if necessary
		if (!fs::exists(modFolder))
		{
			DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Mod folder doesn't exist for PK4, creating: %s\r", modFolder.file_string().c_str());
			try
			{
				fs::create_directory(modFolder);
			}
			catch (fs::basic_filesystem_error<fs::path>& e)
			{
				DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Exception while creating folder for PK4: %s\r", e.what());
			}
		}

		// Move the PK4 to that folder
		fs::path targetPath = modFolder;
		
		if (isL10nPack)
		{
			targetPath /= (modName + "_l10n.pk4").c_str();
		}
		else
		{
			targetPath /= (modName + ".pk4").c_str();
		}

		// Remember to move this file as soon as we're done here
		moveList.push_back(MoveList::value_type(pk4path, targetPath));
	}

	return moveList;
}

fs::path CMissionManager::GetDarkmodPath()
{
	return fs::path(g_Global.GetDarkmodPath());
}

void CMissionManager::ReloadModList()
{
	// Search for new mods (PK4s to be moved, etc.)
	SearchForNewMods();

	// Build the mission list again
	GenerateModList();
}

void CMissionManager::GenerateModList()
{
	// Clear the list first
	_availableMods.Clear();

	// List all folders in the fms/ directory
	fs::path darkmodPath = GetDarkmodPath();
	fs::path fmPath = darkmodPath / cv_tdm_fm_path.GetString();

	DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Looking for mods in FM folder: %s\r", fmPath.file_string().c_str());

	for (fs::directory_iterator i = fs::directory_iterator(fmPath); i != fs::directory_iterator(); ++i)
	{
		fs::path modFolder = *i;

		if (!fs::is_directory(modFolder)) continue; // skip non-folders

		DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Looking for description file %s in %s.\r", cv_tdm_fm_desc_file.GetString(), modFolder.file_string().c_str());

		// Take the folder name as mod name
		idStr modName = modFolder.filename().c_str();

		// Check for an uncompressed darkmod.txt file
		fs::path descFileName = modFolder / cv_tdm_fm_desc_file.GetString();
		
		if (fs::exists(descFileName))
		{
			// File exists, add this as available mod
			_availableMods.Alloc() = modName;
			continue;
		}

		// no "darkmod.txt" file found, check in the PK4 files
		DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("%s file not found, looking for PK4s.\r", descFileName.file_string().c_str());

		// Check for PK4s in that folder
		for (fs::directory_iterator pk4Iter = fs::directory_iterator(modFolder); pk4Iter != fs::directory_iterator(); ++pk4Iter)
		{
			fs::path pk4path = *pk4Iter;

			idStr extension = pk4path.extension().c_str();
			extension.ToLower();

			if (extension != ".pk4")
			{
				continue;
			}

			DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Found PK4 file %s.\r", pk4path.file_string().c_str());

			CZipFilePtr pk4file = CZipLoader::Instance().OpenFile(pk4path.file_string().c_str());

			if (pk4file == NULL)
			{
				DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Could not open PK4: %s\r", pk4path.file_string().c_str());
				continue; // failed to open zip file
			}

			// Check if this is a localisation pack, don't extract files from those
			bool isL10nPack = boost::algorithm::iends_with(pk4path.stem(), "_l10n");

			if (!isL10nPack && pk4file->ContainsFile(cv_tdm_fm_desc_file.GetString()))
			{
				// Hurrah, we've found the darkmod.txt file, extract the contents
				// and attempt to save to folder
				_availableMods.Alloc() = modName;

				fs::path destPath = modFolder / cv_tdm_fm_desc_file.GetString();

				pk4file->ExtractFileTo(cv_tdm_fm_desc_file.GetString(), destPath.string().c_str());

				// Check for the other meta-files as well
				if (pk4file->ContainsFile(cv_tdm_fm_splashimage_file.GetString()))
				{
					destPath = modFolder / cv_tdm_fm_splashimage_file.GetString();
					pk4file->ExtractFileTo(cv_tdm_fm_splashimage_file.GetString(), destPath.string().c_str());
				}

				if (pk4file->ContainsFile(cv_tdm_fm_notes_file.GetString()))
				{
					destPath = modFolder / cv_tdm_fm_notes_file.GetString();
					pk4file->ExtractFileTo(cv_tdm_fm_notes_file.GetString(), destPath.string().c_str());
				}
			}
		}
	}

	gameLocal.Printf("Found %d mods in the FM folder.\n", _availableMods.Num());
	DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Found %d mods in the FM folder.\r", _availableMods.Num());

	// Sort the mod list alphabetically
	SortModList();
}

// Compare functor to sort missions by title
int CMissionManager::ModSortCompare(const int* a, const int* b)
{
	// Get the mission titles (fs_game stuff)
	CModInfoPtr aInfo = gameLocal.m_MissionManager->GetModInfo(*a);
	CModInfoPtr bInfo = gameLocal.m_MissionManager->GetModInfo(*b);

	if (aInfo == NULL || bInfo == NULL) return 0;

	idStr aName = gameLocal.m_I18N->Translate( aInfo->displayName );
	idStr bName = gameLocal.m_I18N->Translate( bInfo->displayName );

	gameLocal.m_I18N->MoveArticlesToBack( aName );
	gameLocal.m_I18N->MoveArticlesToBack( bName );

	return aName.Icmp(bName);
}

void CMissionManager::SortModList()
{
	// greebo: idStrList has a specialised algorithm, preventing me
	// from using a custom sort algorithm, hence this ugly thing here
	idList<int> indexList;

	indexList.SetNum(_availableMods.Num());
	for (int i = 0; i < _availableMods.Num(); ++i)
	{
		indexList[i] = i;
	}

	indexList.Sort( CMissionManager::ModSortCompare );

	idStrList temp = _availableMods;

	for (int i = 0; i < indexList.Num(); ++i)
	{
		_availableMods[i] = temp[indexList[i]];
	}
}

void CMissionManager::RefreshMetaDataForNewFoundMods()
{
	// greebo: If we have new found mods, refresh the meta data of the corresponding MissionDB entries
	// otherwise we end up with empty display names after downloading a mod we had on the HDD before
	for (int i = 0; i < _newFoundMods.Num(); ++i)
	{
		CModInfoPtr info = GetModInfo(_newFoundMods[i]);

		if (info != NULL) 
		{
			if (info->LoadMetaData())
			{
				DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Successfully read meta data for newly found mod %s\r", _newFoundMods[i].c_str());
			}
			else
			{
				DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Could not read meta data for newly found mod %s\r", _newFoundMods[i].c_str());
			}
		}
	}
}

bool CMissionManager::DoCopyFile(const fs::path& source, const fs::path& dest, bool overwrite)
{
	if (overwrite)
	{
		try
		{
			// According to docs, remove() doesn't throw if file doesn't exist
			fs::remove(dest);
			DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Destination file %s already exists, has been removed before copying.\r", dest.string().c_str());
		}
		catch (fs::basic_filesystem_error<fs::path>& e)
		{
			// Don't care about removal error
			DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Caught exception while removing destination file %s: %s\r", dest.string().c_str(), e.what());
		}
	}

	// Copy the source file to the destination
	try
	{
		fs::copy_file(source, dest);
		DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("File successfully copied to %s.\r", dest.string().c_str());

		return true;
	}
	catch (fs::basic_filesystem_error<fs::path>& e)
	{
		DM_LOG(LC_MAINMENU, LT_ERROR)LOGSTRING("Exception while coyping file from %s to %s: %s\r", 
			source.string().c_str(), dest.string().c_str(), e.what());

		return false;
	}
}

bool CMissionManager::DoRemoveFile(const fs::path& fileToRemove)
{
	try
	{
		fs::remove(fileToRemove);
		DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Removed file in %s\r", fileToRemove.file_string().c_str());

		return true;
	}
	catch (fs::basic_filesystem_error<fs::path>& e)
	{
		DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Exception while removing file: %s\r", e.what());
		return false;
	}
}

bool CMissionManager::DoMoveFile(const fs::path& fromPath, const fs::path& toPath)
{
	try
	{
		fs::rename(fromPath, toPath);
		DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Moved %s to %s\r", fromPath.file_string().c_str(), toPath.file_string().c_str());

		return true;
	}
	catch (fs::basic_filesystem_error<fs::path>& e)
	{
		DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Exception while moving file: %s\r", e.what());

		return false;
	}
}

void CMissionManager::InitStartingMap()
{
	_curStartingMap.Empty();

	idStr curModName = GetCurrentModName();

	if (curModName.IsEmpty())
	{
		return;
	}

	// Find out which is the starting map of the current mod
	char* buffer = NULL;

	if (fileSystem->ReadFile(cv_tdm_fm_startingmap_file.GetString(), reinterpret_cast<void**>(&buffer)) != -1)
	{
		// We have a startingmap
		_curStartingMap = buffer;
		fileSystem->FreeFile(reinterpret_cast<void*>(buffer));
	}
	else
	{
		gameLocal.Warning("No '%s' file for the current mod: %s", cv_tdm_fm_startingmap_file.GetString(), GetCurrentModName().c_str());
	}
}

void CMissionManager::InitMapSequence()
{
	_curMissionIndex = 0;
	_mapSequence.Clear();

	idStr curModName = GetCurrentModName();

	if (curModName.IsEmpty())
	{
		return;
	}

	// Find out which is the starting map of the current mod
	idLexer lexer(cv_tdm_fm_mapsequence_file.GetString());

	punctuation_t punct[] =
	{
		{ ":", P_COLON },
		{ NULL, 0 }
	};
	lexer.SetPunctuations(punct);

	if (lexer.IsLoaded())
	{
		idToken token;

		// Read until EOF
		while (lexer.ReadToken(&token))
		{
			if (token.type == TT_NAME && token == "Mission")
			{
				// Get the mission number
				if (!lexer.ReadToken(&token))
				{
					lexer.Warning("Expected number after 'Mission' keyword in map sequence file.");
					break;
				}

				// Parse the mission number
				int missionNumber = token.GetIntValue();

				if (missionNumber == 0)
				{
					lexer.Warning("Cannot parse integer value after 'Mission' keyword in map sequence file.");
					break;
				}

				// 0-based index into the sequence structure
				int missionIndex = missionNumber - 1;

				// Make sure the sequence has enough items 
				if (_mapSequence.Num() < missionNumber)
				{
					_mapSequence.SetNum(missionNumber);
				}

				if (!lexer.ExpectTokenType(TT_PUNCTUATION, P_COLON, &token))
				{
					lexer.Warning("Expected colon ':' after Mission N declaration.");
					break;
				}

				token.Clear();

				lexer.ReadRestOfLine(token);

				idLexer mapLexer(token.c_str(), token.Length(), "mapnames");

				idToken mapToken;

				while (mapLexer.ReadToken(&mapToken))
				{
					_mapSequence[missionIndex].mapNames.Append(mapToken);
				}

				DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Mapsequence: Parsed %d maps for mission %d\r", _mapSequence[missionIndex].mapNames.Num(), missionNumber);
			}
			else
			{
				lexer.Warning("Unrecognized token: %s", token.c_str());
				continue;
			}
		}

		gameLocal.Printf("Parsed map sequence file: %d missions found.\n", _mapSequence.Num());

		// Do some safety checks and emit warnings for easier debugging
		for (int i = 0; i < _mapSequence.Num(); ++i)
		{
			if (_mapSequence[i].mapNames.Num() == 0)
			{
				gameLocal.Warning("Mission #%d in %s doesn't define any maps!", i, cv_tdm_fm_mapsequence_file.GetString());
				DM_LOG(LC_MAINMENU, LT_WARNING)LOGSTRING("Mission #%d in %s doesn't define any maps!\r", i, cv_tdm_fm_mapsequence_file.GetString());
			}
		}
	}
	else
	{
		gameLocal.Printf("No '%s' file found for the current mod: %s\n", cv_tdm_fm_mapsequence_file.GetString(), GetCurrentModName().c_str());
	}
}

const idStr& CMissionManager::GetCurrentStartingMap() const
{
	if (CurrentModIsCampaign())
	{
		return _mapSequence[_curMissionIndex].mapNames[0];
	}

	return _curStartingMap;
}

int CMissionManager::GetCurrentMissionIndex() const
{
	if (CurrentModIsCampaign())
	{
		return _curMissionIndex;
	}

	return 0; // single-mission
}

void CMissionManager::SetCurrentMissionIndex(int index)
{
	_curMissionIndex = index;
}

bool CMissionManager::ProceedToNextMission()
{
	if (NextMissionAvailable())
	{
		_curMissionIndex++;
		return true;
	}

	return false; // no campaign or no next mission available
}

bool CMissionManager::NextMissionAvailable() const
{
	if (CurrentModIsCampaign())
	{
		return _curMissionIndex + 1 < _mapSequence.Num();
	}

	return false; // no campaign
}

bool CMissionManager::CurrentModIsCampaign() const
{
	// A non-empty map sequence indicates we have a campaign
	return _mapSequence.Num() > 0;
}

CMissionManager::InstallResult CMissionManager::InstallMod(int index)
{
	if (index < 0 || index >= _availableMods.Num())
	{
		gameLocal.Warning("Index out of bounds in MissionManager::InstallMission().");
		return INDEX_OUT_OF_BOUNDS; // out of bounds
	}

	// Pass the call to the getbyname method
	return InstallMod(_availableMods[index]);
}

CMissionManager::InstallResult CMissionManager::InstallMod(const idStr& name)
{
	// Path to the parent directory
	fs::path parentPath(fileSystem->RelativePathToOSPath("", "fs_savepath"));
	parentPath = parentPath.remove_leaf().remove_leaf();

	CModInfoPtr info = GetModInfo(name); // result is always non-NULL

	const idStr& modDirName = info->modName;

	// Ensure that the target folder exists
	fs::path targetFolder = parentPath / modDirName.c_str();

	if (!fs::create_directory(targetFolder))
	{
		// Directory exists, not a problem, but log this
		DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("FM targetFolder already exists: %s\r", targetFolder.string().c_str());
	}

	// Path to the darkmod directory
	fs::path darkmodPath = GetDarkmodPath();

	// Copy all PK4s from the FM folder (and all subdirectories)
	idFileList* pk4Files = fileSystem->ListFilesTree(info->pathToFMPackage, ".pk4", false);

	for (int i = 0; i < pk4Files->GetNumFiles(); ++i)
	{
		// Source file (full OS path)
		fs::path pk4fileOsPath = GetDarkmodPath() / pk4Files->GetFile(i);

		// Target location
		fs::path targetFile = targetFolder / pk4fileOsPath.leaf();

		DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Copying file %s to %s\r", pk4fileOsPath.string().c_str(), targetFile.string().c_str());

		// Use boost::filesystem instead of id's (comments state that copying large files can be problematic)
		//fileeSystem->CopyFile(pk4fileOsPath, targetFile.string().c_str());

		// Copy the PK4 file and make sure any target file with the same name is removed beforehand
		if (!DoCopyFile(pk4fileOsPath, targetFile, true))
		{
			// Failed copying
			return COPY_FAILURE;
		}
	}

	fileSystem->FreeFileList(pk4Files);

	// Path to file that holds the current FM name
	fs::path currentFMPath(darkmodPath / cv_tdm_fm_current_file.GetString());

	DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Trying to save current FM name to %s\r", currentFMPath.file_string().c_str());

	// Save the name of the new mod
	FILE* currentFM = fopen(currentFMPath.file_string().c_str(), "w+");

	if (currentFM != NULL)
	{
		fputs(modDirName, currentFM);
		fclose(currentFM);
	}
	else
	{
		DM_LOG(LC_MAINMENU, LT_ERROR)LOGSTRING("Could not save current FM name to %s\r", currentFMPath.file_string().c_str());
	}

	DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Successfully saved current FM name to %s\r", currentFMPath.file_string().c_str());

	// Assemble the path to the FM's DoomConfig.cfg
	fs::path doomConfigPath = targetFolder / "DoomConfig.cfg";
	
	// Check if we should synchronise DoomConfig.cfg files
	if (cv_tdm_fm_sync_config_files.GetBool())
	{
		// Yes, sync DoomConfig.cfg

		// Always copy the DoomConfig.cfg from darkmod/ to the new mod/
		// Remove any DoomConfig.cfg that might exist there beforehand
		if (!DoCopyFile(darkmodPath / "DoomConfig.cfg", doomConfigPath, true))
		{
			// Failed copying
			return COPY_FAILURE;
		}
	}
	else
	{
		// No, don't sync DoomConfig.cfg, but at least copy a basic one over there if it doesn't exist
		if (!fs::exists(doomConfigPath))
		{
			DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("DoomConfig.cfg not found in FM folder, copy over from darkmod.\r");

			if (!DoCopyFile(darkmodPath / "DoomConfig.cfg", doomConfigPath))
			{
				// Failed copying
				return COPY_FAILURE;
			}
		}
	}

	// Check if the config.spec file already exists in the mod folder
	fs::path configSpecPath = targetFolder / "config.spec";
	if (!fs::exists(configSpecPath))
	{
		// Copy the config.spec file from darkmod/ to the new mod/
		if (!DoCopyFile(darkmodPath / "config.spec", configSpecPath))
		{
			// Failed copying
			return COPY_FAILURE;
		}
	}

	return INSTALLED_OK;
}

void CMissionManager::UninstallMod()
{
	// To uninstall the current FM, just clear the FM name in currentfm.txt	

	// Path to the darkmod directory
	fs::path darkmodPath = GetDarkmodPath();

	// Path to file that holds the current FM name
	fs::path currentFMPath(darkmodPath / cv_tdm_fm_current_file.GetString());

	DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Trying to clear current FM name in %s\r", currentFMPath.file_string().c_str());

	if (DoRemoveFile(currentFMPath))
	{
		DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Current FM file removed: %s.\r", currentFMPath.string().c_str());
	}
	else
	{
		// Log removal error
		DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Caught exception while removing current FM file %s.\r", currentFMPath.string().c_str());
	}
}

int CMissionManager::StartReloadDownloadableMods()
{
	// Clear contents and the list elements themselves
	_downloadableMods.DeleteContents(true);

	if (gameLocal.m_HttpConnection == NULL) return -1;

	// Split the CVAR into parts
	std::string list = cv_tdm_mission_list_urls.GetString();

	std::vector<std::string> urls;
	boost::algorithm::split(urls, list, boost::algorithm::is_any_of(";"));

	if (urls.empty())
	{
		return -1;
	}

	idStringList missionListUrls;

	for (std::size_t i = 0; i < urls.size(); ++i)
	{
		missionListUrls.Alloc() = urls[i].c_str();
	}

	fs::path tempFilename = g_Global.GetDarkmodPath();
	tempFilename /= TMP_MISSION_LIST_FILENAME;

	CDownloadPtr download(new CDownload(missionListUrls, tempFilename.file_string().c_str()));

	_refreshModListDownloadId = gameLocal.m_DownloadManager->AddDownload(download);

	return _refreshModListDownloadId;
}

bool CMissionManager::IsDownloadableModsRequestInProgress()
{
	return _refreshModListDownloadId != -1;
}

CMissionManager::RequestStatus CMissionManager::ProcessReloadDownloadableModsRequest()
{
	if (!IsDownloadableModsRequestInProgress()) 
	{
		return NOT_IN_PROGRESS;
	}

	RequestStatus status = GetRequestStatusForDownloadId(_refreshModListDownloadId);

	// Clean up the result if the request is complete
	if (status == FAILED || status == SUCCESSFUL)
	{
		fs::path tempFilename = g_Global.GetDarkmodPath();
		tempFilename /= TMP_MISSION_LIST_FILENAME;

		if (status == SUCCESSFUL)
		{
			XmlDocumentPtr doc(new pugi::xml_document);
		
			pugi::xml_parse_result result = doc->load_file(tempFilename.file_string().c_str());
			
			if (result)
			{
				LoadModListFromXml(doc);
			}
			else
			{
				status = FAILED; 
			}
		}

		// Remove the temporary file
		DoRemoveFile(tempFilename);

		// Clear the download
		gameLocal.m_DownloadManager->RemoveDownload(_refreshModListDownloadId);
		_refreshModListDownloadId = -1;
	}

	return status;
}

int CMissionManager::StartDownloadingModDetails(int modNum)
{
	// Index out of bounds?
	if (modNum < 0 || modNum >= _downloadableMods.Num()) return -1;

	// HTTP requests allowed?
	if (gameLocal.m_HttpConnection == NULL) return -1;

	const DownloadableMod& mod = *_downloadableMods[modNum];

	idStr url = va(cv_tdm_mission_details_url.GetString(), mod.id);

	fs::path tempFilename = g_Global.GetDarkmodPath();
	tempFilename /= TMP_MISSION_DETAILS_FILENAME;

	CDownloadPtr download(new CDownload(url, tempFilename.file_string().c_str()));

	// Store the mod number in the download class
	download->GetUserData().id = modNum;

	_modDetailsDownloadId = gameLocal.m_DownloadManager->AddDownload(download);

	return _modDetailsDownloadId;
}

bool CMissionManager::IsModDetailsRequestInProgress()
{
	return _modDetailsDownloadId != -1;
}

CMissionManager::RequestStatus CMissionManager::ProcessReloadModDetailsRequest()
{
	if (!IsModDetailsRequestInProgress()) 
	{
		return NOT_IN_PROGRESS;
	}

	RequestStatus status = GetRequestStatusForDownloadId(_modDetailsDownloadId);

	// Clean up the result if the request is complete
	if (status == FAILED || status == SUCCESSFUL)
	{
		fs::path tempFilename = g_Global.GetDarkmodPath();
		tempFilename /= TMP_MISSION_DETAILS_FILENAME;

		if (status == SUCCESSFUL)
		{
			XmlDocumentPtr doc(new pugi::xml_document);
		
			pugi::xml_parse_result result = doc->load_file(tempFilename.file_string().c_str());

			if (result)
			{
				CDownloadPtr download = gameLocal.m_DownloadManager->GetDownload(_modDetailsDownloadId);
				assert(download != NULL);

				// Mod number was stored as userdata in the download object
				int modNum = download->GetUserData().id;

				LoadModDetailsFromXml(doc, modNum);
			}
			else
			{
				// Failed to parse XML
				status = FAILED; 
			}
		}

		// Remove the temporary file
		DoRemoveFile(tempFilename);

		// Clear the download
		gameLocal.m_DownloadManager->RemoveDownload(_modDetailsDownloadId);
		_modDetailsDownloadId = -1;
	}

	return status;
}

bool CMissionManager::IsMissionScreenshotRequestInProgress()
{
	return _modScreenshotDownloadId != -1;
}

int CMissionManager::StartDownloadingMissionScreenshot(int missionIndex, int screenshotNum)
{
	assert(_modScreenshotDownloadId == -1); // ensure no download is in progress when this is called

	// Index out of bounds?
	if (missionIndex < 0 || missionIndex >= _downloadableMods.Num()) return -1;

	// HTTP requests allowed?
	if (gameLocal.m_HttpConnection == NULL) return -1;

	const DownloadableMod& mission = *_downloadableMods[missionIndex];

	assert(screenshotNum >= 0 && screenshotNum < mission.screenshots.Num());

	idStr url = va(cv_tdm_mission_screenshot_url.GetString(), mission.screenshots[screenshotNum]->serverRelativeUrl.c_str());

	DM_LOG(LC_MAINMENU, LT_INFO)LOGSTRING("Downloading screenshot from %s\r", url.c_str());

	fs::path tempFilename = g_Global.GetDarkmodPath();
	tempFilename /= cv_tdm_fm_path.GetString();
	tempFilename /= TMP_MISSION_SCREENSHOT_FILENAME;

	CDownloadPtr download(new CDownload(url, tempFilename.file_string().c_str()));

	// Store the mission and screenshot number in the download class
	download->GetUserData().id = missionIndex;
	download->GetUserData().id2 = screenshotNum;

	_modScreenshotDownloadId = gameLocal.m_DownloadManager->AddDownload(download);

	return _modScreenshotDownloadId;
}

CMissionManager::RequestStatus CMissionManager::ProcessMissionScreenshotRequest()
{
	if (!IsMissionScreenshotRequestInProgress()) 
	{
		return NOT_IN_PROGRESS;
	}

	RequestStatus status = GetRequestStatusForDownloadId(_modScreenshotDownloadId);

	// Clean up the result if the request is complete
	if (status == FAILED || status == SUCCESSFUL)
	{
		fs::path tempFilename = g_Global.GetDarkmodPath();
		tempFilename /= cv_tdm_fm_path.GetString();
		tempFilename /= TMP_MISSION_SCREENSHOT_FILENAME;

		if (status == SUCCESSFUL)
		{
			CDownloadPtr download = gameLocal.m_DownloadManager->GetDownload(_modScreenshotDownloadId);
			assert(download != NULL);

			// Mission was stored as userdata in the download object
			int missionIndex = download->GetUserData().id;
			int screenshotNum = download->GetUserData().id2;

			assert(missionIndex >= 0 && missionIndex < _downloadableMods.Num());

			DownloadableMod& mission = *_downloadableMods[missionIndex];

			assert(screenshotNum >= 0 && screenshotNum < mission.screenshots.Num());

			// Open, convert and save the image
			if (!ProcessMissionScreenshot(tempFilename, mission, screenshotNum))
			{
				gameLocal.Warning("Failed to process downloaded screenshot, mission %s, screenshot #%d", 
					mission.modName.c_str(), screenshotNum);
				DM_LOG(LC_MAINMENU, LT_ERROR)LOGSTRING("Failed to process downloaded screenshot, mission %s, screenshot #%d\r", 
					mission.modName.c_str(), screenshotNum);

				status = FAILED;
			}
		}

		// Remove the temporary file
		DoRemoveFile(tempFilename);

		// Clear the download
		gameLocal.m_DownloadManager->RemoveDownload(_modScreenshotDownloadId);
		_modScreenshotDownloadId = -1;
	}

	return status;
}

CMissionManager::RequestStatus CMissionManager::GetRequestStatusForDownloadId(int downloadId)
{
	CDownloadPtr download = gameLocal.m_DownloadManager->GetDownload(downloadId);

	if (download == NULL) return NOT_IN_PROGRESS;

	switch (download->GetStatus())
	{
	case CDownload::NOT_STARTED_YET:	
		return IN_PROGRESS;

	case CDownload::IN_PROGRESS:
		return IN_PROGRESS;

	case CDownload::FAILED:
		return FAILED;

	case CDownload::SUCCESS:
		return SUCCESSFUL;

	default: 
		gameLocal.Printf("Unknown download status encountered in GetRequestStatusForDownloadId()\n");
		return NOT_IN_PROGRESS;
	};
}

void CMissionManager::LoadModDetailsFromXml(const XmlDocumentPtr& doc, int modNum)
{
	assert(doc != NULL);

	/* Example XML Snippet
	
	<?xml version="1.0"?>
	<tdm>
		<mission id="10">
			<id>10</id>
			<title>Trapped!</title>
			<releaseDate>2009-12-30</releaseDate>
			<size>6.3</size>
			<version>1</version>
			<internalName>trapped</internalName>
			<author>RailGun</author>
			<description>I was hired ...</description>
			<downloadLocations>
				<downloadLocation language="English" url="http://www.bloodgate.com/mirrors/tdm/pub/pk4/fms/trapped.pk4" />
			</downloadLocations>
			<screenshots>
				<screenshot thumbpath="/darkmod/screenshots/tp2-1442_thumb.png" path="/darkmod/screenshots/tp2-1442.jpg" />
				<screenshot thumbpath="/darkmod/screenshots/tp10317_thumb.png" path="/darkmod/screenshots/tp10317.jpg" />
			</screenshots>
		</mission>
	</tdm>
	
	*/

	pugi::xpath_node node = doc->select_single_node("//tdm/mission");
	
	assert(modNum >= 0 && modNum < _downloadableMods.Num());

	DownloadableMod& mod = *_downloadableMods[modNum];

	mod.detailsLoaded = true;

	// Description
	mod.description = ReplaceXmlEntities(node.node().child("description").child_value());

	// Screenshots
	pugi::xpath_node_set nodes = doc->select_nodes("//tdm/mission/screenshots//screenshot");

	for (pugi::xpath_node_set::const_iterator i = nodes.begin(); i != nodes.end(); ++i)	
	{
		pugi::xml_node node = i->node();

		MissionScreenshotPtr screenshot(new MissionScreenshot);
		screenshot->serverRelativeUrl = node.attribute("path").value();

		int screenshotNum = mod.screenshots.Append(screenshot);

		fs::path localPath = GetDarkmodPath() / mod.GetLocalScreenshotPath(screenshotNum).c_str();

		if (fs::exists(localPath))
		{
			DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Found existing local screenshot copy %s\r", localPath.file_string().c_str());

			// File exists, store that in the screenshot filename
			screenshot->filename = mod.GetLocalScreenshotPath(screenshotNum);
		}
	}
}

idStr CMissionManager::ReplaceXmlEntities(const idStr& input)
{
	idStr output = input;
	output.Replace("&#13;&#10;", "\n");

	return output;
}

void CMissionManager::LoadModListFromXml(const XmlDocumentPtr& doc)
{
	assert(doc != NULL);

	/* Example XML Snippet
	
	<mission id="11" title="Living Expenses" releaseDate="2010-01-02" size="5.9" author="Sonosuke">
		<downloadLocation language="English" url="http://www.bloodgate.com/mirrors/tdm/pub/pk4/fms/living_expenses.pk4"/>
		<downloadLocation language="German" url="http://www.bloodgate.com/mirrors/tdm/pub/pk4/fms/living_expenses_de.pk4"/>
	</mission>
	
	*/

	pugi::xpath_node_set nodes = doc->select_nodes("//tdm/availableMissions//mission");

	const char* fs_game = cvarSystem->GetCVarString("fs_game");

	for (pugi::xpath_node_set::const_iterator i = nodes.begin(); i != nodes.end(); ++i)	
	{
		pugi::xml_node node = i->node();

		DownloadableMod mission;

		mission.title = node.attribute("title").value();

		mission.id = node.attribute("id").as_int();
		mission.sizeMB = node.attribute("size").as_float();
		mission.author = node.attribute("author").value();
		mission.releaseDate = node.attribute("releaseDate").value();
		mission.type = idStr::Icmp(node.attribute("type").value(), "multi") == 0 ? DownloadableMod::Multi : DownloadableMod::Single;
		mission.modName = node.attribute("internalName").value();
		mission.version = node.attribute("version").as_int();
		mission.isUpdate = false;

		if (idStr::Cmp(mission.modName.c_str(), fs_game) == 0)
		{
			DM_LOG(LC_MAINMENU, LT_DEBUG)LOGSTRING("Removing currently installed mission %s from the list of downloadable missions.\r", fs_game);
			continue;
		}

		bool missionExists = false;

		// Check if this mission is already downloaded
		for (int j = 0; j < _availableMods.Num(); ++j)
		{
			if (idStr::Icmp(_availableMods[j], mission.modName) == 0)
			{
				missionExists = true;
				break;
			}
		}

		if (missionExists)
		{
			// Check mod version, there might be an update available
			if (_missionDB->ModInfoExists(mission.modName))
			{
				CModInfoPtr missionInfo = _missionDB->GetModInfo(mission.modName);

				idStr versionStr = missionInfo->GetKeyValue("downloaded_version", "1");
				int existingVersion = atoi(versionStr.c_str());

				if (existingVersion >= mission.version)
				{
					continue; // Our version is up to date
				}
				else
				{
					mission.isUpdate = true;
				}
			}
		}

		// Mission download links
		pugi::xpath_node_set downloadLocations = node.select_nodes("downloadLocation");

		for (pugi::xpath_node_set::const_iterator loc = downloadLocations.begin(); loc != downloadLocations.end(); ++loc)	
		{
			pugi::xml_node locNode = loc->node();

			// Only accept English downloadlinks
			if (idStr::Icmp(locNode.attribute("language").value(), "english") != 0) continue;

			mission.missionUrls.Append(locNode.attribute("url").value());
		}

		// Localisation packs
		pugi::xpath_node_set l10PackNodes = node.select_nodes("localisationPack");

		for (pugi::xpath_node_set::const_iterator loc = l10PackNodes.begin(); loc != l10PackNodes.end(); ++loc)	
		{
			pugi::xml_node locNode = loc->node();

			mission.l10nPackUrls.Append(locNode.attribute("url").value());
		}

		// Only add missions with valid locations
		if (mission.missionUrls.Num() > 0)
		{
			// Copy-construct the local mission struct into the heap-allocated one
			_downloadableMods.Append(new DownloadableMod(mission));
		}
	}

	SortDownloadableMods();
}

void CMissionManager::SortDownloadableMods()
{
	_downloadableMods.Sort(DownloadableMod::SortCompareTitle);
}

const DownloadableModList& CMissionManager::GetDownloadableMods() const
{
	return _downloadableMods;
}

bool CMissionManager::ProcessMissionScreenshot(const fs::path& tempFilename, DownloadableMod& mod, int screenshotNum)
{
	CImage image(tempFilename.file_string().c_str());

	if (!image.LoadImageFromFile(tempFilename))
	{
		DM_LOG(LC_MAINMENU, LT_ERROR)LOGSTRING("Failed to load image: %s\r", tempFilename.file_string().c_str());
		return false;
	}

	assert(screenshotNum >= 0 && screenshotNum < mod.screenshots.Num());
	
	MissionScreenshot& screenshot = *mod.screenshots[screenshotNum];

	// Build the target path
	fs::path targetPath = GetDarkmodPath();
	targetPath /= cv_tdm_fm_path.GetString();
	targetPath /= TMP_MISSION_SCREENSHOT_FOLDER;

	if (!fs::exists(targetPath))
	{
		fs::create_directories(targetPath);
	}

	targetPath = GetDarkmodPath() / mod.GetLocalScreenshotPath(screenshotNum).c_str();
	
	// Save the file locally as JPEG
	if (!image.SaveImageToFile(targetPath, CImage::JPG))
	{
		gameLocal.Printf("Could not save image to %s\n", targetPath.file_string().c_str());
		return false;
	}
	else
	{
		// Store the filename into the screenshot object, this indicates it's ready for use
		screenshot.filename = mod.GetLocalScreenshotPath(screenshotNum);
	}

	return true;
}
