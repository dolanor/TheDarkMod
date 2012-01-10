// vim:ts=4:sw=4:cindent
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

#include "precompiled_engine.h"
#pragma hdrstop

static bool versioned = RegisterVersionedFile("$Id$");

#include "I18N.h"

// uncomment to have debug printouts
//#define M_DEBUG 1
// uncomment to have each Translate() call printed
//#define T_DEBUG 1

class I18NLocal :
	public I18N
{
public:
	I18NLocal();

	~I18NLocal();

	/**
	* Called by idCommon at game init time.
	*/
	void				Init();

	// Called at game shutdown time
	void				Shutdown();

	/**
	* Attempt to translate a string template in the form of "#str_12345" into
	* the current user selected language, using the FM specific dict first.
	*/
	const char*			Translate( const idStr &in );
	/**
	* The same, but with a const char*
	*/
	const char*			Translate( const char* in );

	/**
	* Returns the current active language.
	*/
	const idStr&		GetCurrentLanguage() const;

	/**
	* Print memory usage info.
    */
	void				Print() const;

	/**
	* Load a new character mapping based on the new language. Returns the
	* number of characters that should be remapped upon dictionary and
	* readable load time.
	*/
	int					LoadCharacterMapping( idStr& lang );

	/**
	* Set a new laguage (example: "english").
	*/
	void				SetLanguage( const char* lang, bool firstTime = false );

	/**
	* Given an English string like "Maps", returns the "#str_xxxxx" template
	* string that would result back in "Maps" under English. Can be used to
	* make translation work even for hard-coded English strings.
	*/
	const char*			TemplateFromEnglish( const char* in);

	/**
	* Changes the given string from "A little House" to "Little House, A",
	* supporting multiple languages like English, German, French etc.
	*/
	void				MoveArticlesToBack(idStr& title);

	/** 
	* To Intercepts calls to common->GetLanguageDict():
	*/
	const idLangDict*	GetLanguageDict() const;

private:
	// current language
	idStr				m_lang;
	// depending on current language, move articles to back of Fm name for display?
	bool				m_bMoveArticles;

	// A dictionary consisting of the current language + the current FM dict.
	idLangDict			m_Dict;

	// reverse dictionary for TemplateFromEnglish
	idDict				m_ReverseDict;
	// dictionary to map "A ..." to "..., A" for MoveArticlesToBack()
	idDict				m_ArticlesDict;

	// A table remapping between characters. The string contains two bytes
	// for each remapped character, Length()/2 is the count.
	idStr				m_Remap;
};

I18NLocal	i18nLocal;
I18N*		i18n = &i18nLocal;

/*
===============
I18NLocal::I18NLocal
===============
*/
I18NLocal::I18NLocal()
{}

I18NLocal::~I18NLocal()
{}

/*
===============
I18NLocal::Init
===============
*/
void I18NLocal::Init()
{
	// some default values, the object becomes only fully usable after Init(), tho:
	m_lang = cvarSystem->GetCVarString( "tdm_lang" );
	m_bMoveArticles = (m_lang != "polish" && m_lang != "italian") ? true : false;

	m_Dict.Clear();

	// build the reverse dictionary for TemplateFromEnglish
	// TODO: Do this by looking them up in lang/english.lang?
	// inventory categories
	m_ReverseDict.Set( "Lockpicks",	"#str_02389" );
	m_ReverseDict.Set( "Maps", 		"#str_02390" );
	m_ReverseDict.Set( "Readables",	"#str_02391" );
	m_ReverseDict.Set( "Keys",		"#str_02392" );
	m_ReverseDict.Set( "Potions",	"#str_02393" );

	// inventory item names used in keybindings
	m_ReverseDict.Set( "Mine",			"#str_02202" );
	m_ReverseDict.Set( "Lantern",		"#str_02395" );
	m_ReverseDict.Set( "Spyglass",		"#str_02396" );
	m_ReverseDict.Set( "Compass",		"#str_02397" );
	m_ReverseDict.Set( "Health Potion",	"#str_02398" );
	m_ReverseDict.Set( "Breath Potion",	"#str_02399" );
	m_ReverseDict.Set( "Holy Water",	"#str_02400" );
	m_ReverseDict.Set( "Flashbomb",		"#str_02438" );
	m_ReverseDict.Set( "Flashmine",		"#str_02439" );
	m_ReverseDict.Set( "Explosive Mine","#str_02440" );
	
	// The article prefixes, with the suffix to use instead
	m_ArticlesDict.Set( "A ",	", A" );	// English, Portuguese
	m_ArticlesDict.Set( "An ",	", An" );	// English
	m_ArticlesDict.Set( "Der ",	", Der" );	// German
	m_ArticlesDict.Set( "Die ",	", Die" );	// German
	m_ArticlesDict.Set( "Das ",	", Das" );	// German
	m_ArticlesDict.Set( "De ",	", De" );	// Dutch, Danish
	m_ArticlesDict.Set( "El ",	", El" );	// Spanish
	m_ArticlesDict.Set( "Het ",	", Het" );	// Dutch
	m_ArticlesDict.Set( "Il ",	", Il" );	// Italian
	m_ArticlesDict.Set( "La ",	", La" );	// French, Italian
	m_ArticlesDict.Set( "Las ",	", Las" );	// Spanish
	m_ArticlesDict.Set( "Le ",	", Le" );	// French
	m_ArticlesDict.Set( "Les ",	", Les" );	// French
	m_ArticlesDict.Set( "Los ",	", Los" );	// Spanish
	m_ArticlesDict.Set( "Os ",	", Os" );	// Portuguese
	m_ArticlesDict.Set( "The ",	", The" );	// English

	m_Remap.Empty();						// by default, no remaps

	// Create the correct dictionary
	SetLanguage( cvarSystem->GetCVarString( "tdm_lang" ), true );
}

/*
===============
I18NLocal::GetLanguageDict

Returns the language dict, so that D3 can use it instead of the system dict. Should
not be called directly.
===============
*/
const idLangDict* I18NLocal::GetLanguageDict() const
{
	return &m_Dict;
}

/*
===============
I18NLocal::Shutdown
===============
*/
void I18NLocal::Shutdown()
{
	common->Printf( "I18NLocal: Shutdown.\n" );
	m_lang = "";
	m_ReverseDict.Clear();
	m_ArticlesDict.Clear();
	m_Dict.Clear();
}

/*
===============
I18NLocal::Print
===============
*/
void I18NLocal::Print() const
{
	common->Printf("I18NLocal: Current language: %s\n", m_lang.c_str() );
	common->Printf("I18NLocal: Move articles to back: %s\n", m_bMoveArticles ? "Yes" : "No");
	common->Printf(" Main " );
	m_Dict.Print();
	common->Printf(" Reverse dict   : " );
	m_ReverseDict.PrintMemory();
	common->Printf(" Articles dict  : " );
	m_ArticlesDict.PrintMemory();
	common->Printf(" Remapped chars : %i\n", m_Remap.Length() / 2 );
}

/*
===============
I18NLocal::Translate
===============
*/
const char* I18NLocal::Translate( const char* in )
{
#ifdef T_DEBUG
	common->Printf("I18NLocal: Translating '%s'.\n", in == NULL ? "(NULL)" : in);
#endif
	return m_Dict.GetString( in );				// if not found here, do warn
}

/*
===============
I18NLocal::Translate
===============
*/
const char* I18NLocal::Translate( const idStr &in ) {
#ifdef T_DEBUG
	common->Printf("I18NLocal: Translating '%s'.\n", in == NULL ? "(NULL)" : in.c_str());
#endif
	return m_Dict.GetString( in.c_str() );			// if not found here, do warn
}

/*
===============
I18NLocal::TemplateFromEnglish

If the string is not a template, but an English string, returns a template
like "#str_01234" from the input. Works only for a limited number of strings
that appear in the reverse dict.
===============
*/
const char* I18NLocal::TemplateFromEnglish( const char* in ) {
#ifdef M_DEBUG
//	common->Printf( "I18NLocal::TemplateFromEnglish(%s)", in );
#endif
	return m_ReverseDict.GetString( in, in );
}

/*
===============
I18NLocal::GetCurrentLanguage
===============
*/
const idStr& I18NLocal::GetCurrentLanguage() const
{
	return m_lang;
}

/*
===============
I18NLocal::LoadCharacterMapping

Loads the character remap table, defaulting to "default.map" if "LANGUAGE.map"
is not found. This is used to fix bug #2812.
===============
*/
int I18NLocal::LoadCharacterMapping( idStr& lang ) {

	m_Remap.Empty();		// clear the old mapping

	const char *buffer = NULL;
	idLexer src( LEXFL_NOFATALERRORS | LEXFL_NOSTRINGCONCAT | LEXFL_ALLOWMULTICHARLITERALS | LEXFL_ALLOWBACKSLASHSTRINGCONCAT );

	idStr file = "strings/"; file += lang + ".map";
	int len = fileSystem->ReadFile( file, (void**)&buffer );

	// does not exist
	if (len <= 0)
	{
		file = "strings/default.map";
		len = fileSystem->ReadFile( file, (void**)&buffer );
	}
	if (len <= 0)
	{
		common->Warning("Cannot find character remapping for language %s.", lang.c_str() );
		return 0;
	}
	
	src.LoadMemory( buffer, strlen( buffer ), file );
	if ( !src.IsLoaded() ) {
		common->Warning("Cannot load character remapping %s.", file.c_str() );
		return 0;
	}

	idToken tok, tok2;
	// format:
	/*
	{
		"ff"	"b2"			// remap character 0xff to 0xb2
	}
	*/
	
	src.ExpectTokenString( "{" );
	while ( src.ReadToken( &tok ) ) {
		if ( tok == "}" ) {
			break;
		}
		if ( src.ReadToken( &tok2 ) ) {
			if ( tok2 == "}" ) {
				common->Warning("Expected a byte, but got } while parsing %s", file.c_str() );
				break;
			}
			// add the two numbers
			//	common->Warning("got '%s' '%s'\n", tok.c_str(), tok2.c_str() );
			m_Remap.Append( (char) tok.GetIntValue() );
			m_Remap.Append( (char) tok2.GetIntValue() );
//			common->Printf("Mapping %i (0x%02x) to %i (0x%02x)\n", tok.GetIntValue(), tok.GetIntValue(), tok2.GetIntValue(), tok2.GetIntValue() );
		}
	}

	return m_Remap.Length() / 2;
}

/*
===============
I18NLocal::SetLanguage

Change the language. Does not check the language here, as to not restrict
ourselves to a limited support of languages.
===============
*/
void I18NLocal::SetLanguage( const char* lang, bool firstTime ) {
	if (lang == NULL)
	{
		return;
	}
#ifdef M_DEBUG
	common->Printf("I18NLocal: SetLanguage: '%s'.\n", lang);
#endif

	// store the new setting
	idStr oldLang = m_lang; 
	m_lang = lang;

	// set sys_lang
	cvarSystem->SetCVarString("tdm_lang", lang);
	m_bMoveArticles = (m_lang != "polish" && m_lang != "italian") ? true : false;

	idStr newLang = idStr(lang);

	// If we need to remap some characters upon loading one of these languages:
	LoadCharacterMapping(newLang);

	// For some reason, "english", "german", "french" and "spanish" share
	// the same font, but "polish" and "russian" get their own font. But
	// since "polish" is actually a copy of the normal western font, use
	// "english" instead to trick D3 into loading the correct font. The
	// dictionary below will be polish, regardless.
	if (newLang == "polish")
	{
		newLang = "english";
	}
	// set sysvar sys_lang (if not possible, D3 will revert to english)
	cvarSystem->SetCVarString( "sys_lang", newLang.c_str() );

	// If sys_lang differs from lang, the language was not supported, so
	// we will load it ourselves.
	if ( newLang != cvarSystem->GetCVarString( "sys_lang" ) )
	{
		common->Printf("I18NLocal: Language '%s' not supported by D3, forcing it.\n", lang);
	}

	// build our combined dictionary, first the TDM base dict
	idStr file = "strings/"; file += m_lang + ".lang";
	m_Dict.Load( file, true, m_Remap.Length() / 2, m_Remap.c_str() );				// true => clear before load

	idLangDict* fmDict = new idLangDict;
	file = "strings/fm/"; file += m_lang + ".lang";

	if (!fmDict->Load(file, false, m_Remap.Length() / 2, m_Remap.c_str()))
	{
		common->Printf("I18NLocal: '%s' not found.\n", file.c_str() );
	}
	else
	{
		// else fold the newly loaded strings into the system dict
		int num = fmDict->GetNumKeyVals( );
		const idLangKeyValue*  kv;
		for (int i = 0; i < num; i++)
		{	
			kv = fmDict->GetKeyVal( i );
			if (kv != NULL)
			{
#ifdef M_DEBUG
				common->Printf("I18NLocal: Folding '%s' ('%s') into main dictionary.\n", kv->key.c_str(), kv->value.c_str() );
#endif
				m_Dict.AddKeyVal( kv->key.c_str(), kv->value.c_str() );
			}
		}
	}

	// With FM strings it can happen that one translation is missing or incomplete,
	// so fall back to the english version by folding these in, too:

	file = "strings/fm/english.lang";
	if (!fmDict->Load(file, true, m_Remap.Length() / 2, m_Remap.c_str()))
	{
		common->Printf("I18NLocal: '%s' not found, skipping it.\n", file.c_str() );
	}
	else
	{
		// else fold the newly loaded strings into the system dict unless they exist already
		int num = fmDict->GetNumKeyVals( );
		const idLangKeyValue*  kv;
		for (int i = 0; i < num; i++)
		{	
			kv = fmDict->GetKeyVal( i );
			if (kv != NULL)
			{
				const char *oldEntry = m_Dict.GetString( kv->key.c_str(), false);
				// if equal, the entry was not found
				if (oldEntry == kv->key.c_str())
				{
#ifdef M_DEBUG
					common->Printf("I18NLocal: Folding '%s' ('%s') into main dictionary as fallback.\n", kv->key.c_str(), kv->value.c_str() );
#endif
					m_Dict.AddKeyVal( kv->key.c_str(), kv->value.c_str() );
				}
			}
		}
	}

	idUserInterface *gui = uiManager->FindGui( "guis/mainmenu.gui", false, true, true );

	if ( gui && (!firstTime) && (oldLang != m_lang && (oldLang == "russian" || m_lang == "russian")))
	{
		// Restarting the game does not really work, the fonts are still broken
		// (for some reason) and if the user was in a game, this would destroy his session.
	    // this does not reload the fonts, either: cmdSystem->BufferCommandText( CMD_EXEC_NOW, "ReloadImages" );

		// So instead just pop-up a message box:
		gui->SetStateBool("MsgBoxVisible", true);

		// TODO: Switching to Russian will show these strings in Russian, but the font is not yet there
		//		 So translate these before loading the new dictionary, and the display them?
		gui->SetStateString("MsgBoxTitle", Translate("#str_02206") );	// Language changed
		gui->SetStateString("MsgBoxText", Translate("#str_02207") );	// You might need to manually restart the game to see the right characters.

		gui->SetStateBool("MsgBoxLeftButtonVisible", false);
		gui->SetStateBool("MsgBoxRightButtonVisible", false);
		gui->SetStateBool("MsgBoxMiddleButtonVisible", true);
		gui->SetStateString("MsgBoxMiddleButtonText", Translate("#str_04339"));

		gui->SetStateString("MsgBoxMiddleButtonCmd", "close_msg_box");
	}

	// finally reload the GUI so it appears in the new language
	uiManager->Reload( true );		// true => reload all

	// and switch back to the General Settings page
	if (gui)
	{
		// Tell the GUI that it was reloaded, so when it gets initialized the next frame,
		// it will land in the Video Settings page
		common->Printf("Setting reload\n");
		gui->SetStateInt("reload", 1);
	}
	else
	{
		common->Warning("Cannot find guis/mainmenu.gui");
	}

	// TODO: Notify the game about the language change
	/*
	if (game != NULL)
	{
		game->OnLanguageChanged();
	}
	*/

#if 0
	// From game code: Cycle through all active entities and call "onLanguageChanged" on them
	// some scriptobjects may implement this function to react on language switching
	for (idEntity* ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next())
	{
		idThread* thread = ent->CallScriptFunctionArgs("onLanguageChanged", true, 0, "e", ent);

		if (thread != NULL)
		{
			thread->Execute();
		}
	}
#endif
}

/*
===============
I18NLocal::MoveArticlesToBack

Changes "A little House" to "Little House, A", supporting multiple languages
like English, German, French etc.
===============
*/
void I18NLocal::MoveArticlesToBack(idStr& title)
{
	// Do not move articles if the language is italian or polish:
	if ( !m_bMoveArticles )
	{
		return;
	}

	// find index of first " "
	int spaceIdx = title.Find(' ');
	// no space, nothing to do
	if (spaceIdx == -1) { return; }

	idStr prefix = title.Left( spaceIdx + 1 );

	// see if we have Prefix in the dictionary
	const char* suffix = m_ArticlesDict.GetString( prefix.c_str(), NULL );
	if (suffix != NULL)
	{
		// found, remove prefix and append suffix
		title.StripLeadingOnce( prefix.c_str() );
		title += suffix;
	}
}

