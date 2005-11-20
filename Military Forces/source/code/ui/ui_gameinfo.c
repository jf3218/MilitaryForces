/*
 * $Id: ui_gameinfo.c,v 1.6 2005-11-20 11:21:38 thebjoern Exp $
*/
//
// gameinfo.c
//

#include "ui_local.h"


//
// arena and bot info
//


//int				ui_numBots;
//static char		*ui_botInfos[MAX_BOTS];

static int		ui_numArenas;
static char		*ui_arenaInfos[MAX_ARENAS];

/*
===============
UI_ParseInfos
===============
*/
int UI_ParseInfos( char *buf, int max, char *infos[] ) 
{
	char	*token;
	int		count;
	char	key[MAX_TOKEN_CHARS];
	char	info[MAX_INFO_STRING];

	count = 0;

	while ( 1 ) 
	{
		token = COM_Parse( &buf );
		if ( !token[0] ) 
			break;
		
		if ( strcmp( token, "{" ) ) 
		{
			Com_Printf( "Missing { in info file\n" );
			break;
		}

		if ( count == max ) 
		{
			Com_Printf( "Max infos exceeded\n" );
			break;
		}

		info[0] = '\0';
		while ( 1 ) 
		{
			token = COM_ParseExt( &buf, true );
			if ( !token[0] ) 
			{
				Com_Printf( "Unexpected end of info file\n" );
				break;
			}
			if ( !strcmp( token, "}" ) ) 
				break;
			
			Q_strncpyz( key, token, sizeof( key ) );

			token = COM_ParseExt( &buf, false );
			if ( !token[0] ) 
				strcpy( token, "<NULL>" );
			
			Info_SetValueForKey( info, key, token );
		}
		//NOTE: extra space for arena number
		infos[count] = reinterpret_cast<char*>(uiInfo.uiUtils.alloc(strlen(info) + strlen("\\num\\") + 
			strlen(va("%d", MAX_ARENAS)) + 1));
		if (infos[count]) 
		{
			strcpy(infos[count], info);
			count++;
		}
	}
	return count;
}

/*
===============
UI_LoadArenasFromFile
===============
*/
static void UI_LoadArenasFromFile( char *filename ) {
	int				len;
	fileHandle_t	f;
	char			buf[MAX_ARENAS_TEXT];

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( !f ) {
		Com_Printf( S_COLOR_RED "file not found: %s\n", filename );
		return;
	}
	if ( len >= MAX_ARENAS_TEXT ) {
		Com_Printf( S_COLOR_RED "file too large: %s is %i, max allowed is %i", filename, len, MAX_ARENAS_TEXT );
		trap_FS_FCloseFile( f );
		return;
	}

	trap_FS_Read( buf, len, f );
	buf[len] = 0;
	trap_FS_FCloseFile( f );

	ui_numArenas += UI_ParseInfos( buf, MAX_ARENAS - ui_numArenas, &ui_arenaInfos[ui_numArenas] );
}

/*
===============
UI_AlphabeticMapNameQsortCompare

Used to sort the maps into alphabetic order
===============
*/
int UI_AlphabeticMapNameQsortCompare( const void * arg1, const void * arg2 )
{
   /* Compare all of both strings: */
   return Q_stricmp( ((mapInfo *) arg1)->mapName, ((mapInfo *) arg2)->mapName );
}

/*
===============
UI_LoadArenas
===============
*/
void UI_LoadArenas( void ) {
	int			numdirs;
	vmCvar_t	arenasFile;
	char		filename[128];
	char		dirlist[1024];
	char*		dirptr;
	int			i, n;
	int			dirlen;
	char		*type, *game;

	ui_numArenas = 0;
	uiInfo.mapCount = 0;

	trap_Cvar_Register( &arenasFile, "g_arenasFile", "", CVAR_INIT|CVAR_ROM );
	if( *arenasFile.string ) {
		UI_LoadArenasFromFile(arenasFile.string);
	}
	else {
		UI_LoadArenasFromFile("scripts/arenas.txt");
	}

	// get all arenas from .arena files
	numdirs = trap_FS_GetFileList("scripts", ".arena", dirlist, 1024 );
	dirptr  = dirlist;
	for (i = 0; i < numdirs; i++, dirptr += dirlen+1) {
		dirlen = strlen(dirptr);
		strcpy(filename, "scripts/");
		strcat(filename, dirptr);
		UI_LoadArenasFromFile(filename);
	}
	Com_Printf( "%i arenas parsed\n", ui_numArenas );
	if (uiInfo.uiUtils.outOfMemory()) {
		Com_Printf(S_COLOR_YELLOW"WARNING: not anough memory in pool to load all arenas\n");
	}

	for( n = 0; n < ui_numArenas; n++ )
	{
		// only allow mfq3 maps
		game = Info_ValueForKey( ui_arenaInfos[n], "game" );
		if( strcmp( game, "mfq3" ) != 0 )
		{
			// not a mfq3 map
			continue;
		}

		// determine type

		uiInfo.mapList[uiInfo.mapCount].cinematic = -1;
		uiInfo.mapList[uiInfo.mapCount].mapLoadName = uiInfo.uiUtils.string_Alloc(Info_ValueForKey(ui_arenaInfos[n], "map"));
		uiInfo.mapList[uiInfo.mapCount].mapName = uiInfo.uiUtils.string_Alloc(Info_ValueForKey(ui_arenaInfos[n], "longname"));
		uiInfo.mapList[uiInfo.mapCount].levelShot = -1;
		uiInfo.mapList[uiInfo.mapCount].imageName = uiInfo.uiUtils.string_Alloc(va("levelshots/%s", uiInfo.mapList[uiInfo.mapCount].mapLoadName));
		uiInfo.mapList[uiInfo.mapCount].typeBits = 0;

		type = Info_ValueForKey( ui_arenaInfos[n], "type" );
		
		// if no type specified, it will be treated as "dm"
		if( *type )
		{
			// deathmatch (Free For All) ('ffa' supported for backwards compatibility)
			if( strstr( type, "dm" ) || strstr( type, "ffa" ) ) {
				uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << GT_FFA);
			}
			// team deathmatch ('team' supported for backwards compatibility)
			if( strstr( type, "tdm" ) || strstr( type, "team" ) ) {
				uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << GT_TEAM);
			}
			// capture the flag
			if( strstr( type, "ctf" ) ) {
				uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << GT_CTF);
			}

// not MFQ3!
/*
			if( strstr( type, "tourney" ) ) {
				uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << GT_TOURNAMENT);
			}
			if( strstr( type, "oneflag" ) ) {
				uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << GT_1FCTF);
			}
			if( strstr( type, "overload" ) ) {
				uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << GT_OBELISK);
			}
			if( strstr( type, "harvester" ) ) {
				uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << GT_HARVESTER);
			}
*/
		}
		else
		{
			// default
			uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << GT_FFA);
		}

		uiInfo.mapCount++;
		if (uiInfo.mapCount >= MAX_MAPS) {
			break;
		}
	}

	// perform an alphabetic sort on the map names
	qsort( &uiInfo.mapList[0], uiInfo.mapCount, sizeof( mapInfo ), UI_AlphabeticMapNameQsortCompare );
}

//
///*
//===============
//UI_LoadBotsFromFile
//===============
//*/
//static void UI_LoadBotsFromFile( char *filename ) {
//	int				len;
//	fileHandle_t	f;
//	char			buf[MAX_BOTS_TEXT];
//
//	len = trap_FS_FOpenFile( filename, &f, FS_READ );
//	if ( !f ) {
//		trap_Print( va( S_COLOR_RED "file not found: %s\n", filename ) );
//		return;
//	}
//	if ( len >= MAX_BOTS_TEXT ) {
//		trap_Print( va( S_COLOR_RED "file too large: %s is %i, max allowed is %i", filename, len, MAX_BOTS_TEXT ) );
//		trap_FS_FCloseFile( f );
//		return;
//	}
//
//	trap_FS_Read( buf, len, f );
//	buf[len] = 0;
//	trap_FS_FCloseFile( f );
//
//	COM_Compress(buf);
//
//	ui_numBots += UI_ParseInfos( buf, MAX_BOTS - ui_numBots, &ui_botInfos[ui_numBots] );
//}

///*
//===============
//UI_LoadBots
//===============
//*/
//void UI_LoadBots( void ) {
//	vmCvar_t	botsFile;
//	int			numdirs;
//	char		filename[128];
//	char		dirlist[1024];
//	char*		dirptr;
//	int			i;
//	int			dirlen;
//
//	ui_numBots = 0;
//
//	trap_Cvar_Register( &botsFile, "g_botsFile", "", CVAR_INIT|CVAR_ROM );
//	if( *botsFile.string ) {
//		UI_LoadBotsFromFile(botsFile.string);
//	}
//	else {
//		UI_LoadBotsFromFile("scripts/bots.txt");
//	}
//
//	// get all bots from .bot files
//	numdirs = trap_FS_GetFileList("scripts", ".bot", dirlist, 1024 );
//	dirptr  = dirlist;
//	for (i = 0; i < numdirs; i++, dirptr += dirlen+1) {
//		dirlen = strlen(dirptr);
//		strcpy(filename, "scripts/");
//		strcat(filename, dirptr);
//		UI_LoadBotsFromFile(filename);
//	}
//	trap_Print( va( "%i bots parsed\n", ui_numBots ) );
//}


///*
//===============
//UI_GetBotInfoByNumber
//===============
//*/
//char *UI_GetBotInfoByNumber( int num ) {
//	if( num < 0 || num >= ui_numBots ) {
//		trap_Print( va( S_COLOR_RED "Invalid bot number: %i\n", num ) );
//		return NULL;
//	}
//	return ui_botInfos[num];
//}


///*
//===============
//UI_GetBotInfoByName
//===============
//*/
//char *UI_GetBotInfoByName( const char *name ) {
//	int		n;
//	char	*value;
//
//	for ( n = 0; n < ui_numBots ; n++ ) {
//		value = Info_ValueForKey( ui_botInfos[n], "name" );
//		if ( !Q_stricmp( value, name ) ) {
//			return ui_botInfos[n];
//		}
//	}
//
//	return NULL;
//}

//int UI_GetNumBots() {
//	return ui_numBots;
//}


//char *UI_GetBotNameByNumber( int num ) {
//	char *info = UI_GetBotInfoByNumber(num);
//	if (info) {
//		return Info_ValueForKey( info, "name" );
//	}
//	return "Sarge";
//}
