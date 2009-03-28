/**
 * @file cl_installation.c
 * @brief Handles everything that is located in or accessed through an installation.
 * @note Installation functions prefix: INS_*
 * @todo Allow transfer of items to installations
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../client.h"
#include "../mxml/mxml_ufoai.h"
#include "../../shared/parse.h"
#include "cl_campaign.h"
#include "cl_mapfightequip.h"
#include "cp_aircraft.h"
#include "cp_missions.h"
#include "cl_map.h"
#include "cl_ufo.h"
#include "cl_installation.h"
#include "cp_installation_callbacks.h"

installationType_t INS_GetType (const installation_t *installation)
{
	if (installation->installationTemplate->maxBatteries > 0)
		return INSTALLATION_DEFENCE;
	else if (installation->installationTemplate->maxUFOsStored > 0)
		return INSTALLATION_UFOYARD;

	/* default is radar */
	return INSTALLATION_RADAR;
}

/**
 * @brief Array bound check for the installation index.
 * @param[in] instIdx  Instalation's index
 * @return Pointer to the installation corresponding to instIdx.
 */
installation_t* INS_GetInstallationByIDX (int instIdx)
{
	assert(instIdx < MAX_INSTALLATIONS);
	assert(instIdx >= 0);
	return &ccs.installations[instIdx];
}

/**
 * @brief Array bound check for the installation index.
 * @param[in] instIdx  Instalation's index
 * @return Pointer to the installation corresponding to instIdx if installation is founded, NULL else.
 */
installation_t* INS_GetFoundedInstallationByIDX (int instIdx)
{
	installation_t *installation = INS_GetInstallationByIDX(instIdx);

	if (installation->founded)
		return installation;

	return NULL;
}

/**
 * @brief Returns the installation Template for a given installation ID.
 * @param[in] id ID of the installation template to find.
 * @return corresponding installation Template, @c NULL if not found.
 */
installationTemplate_t* INS_GetInstallationTemplateFromInstallationID (const char *id)
{
	int idx;

	for (idx = 0; idx < ccs.numInstallationTemplates; idx++)
		if (!strcmp(ccs.installationTemplates[idx].id, id))
			return &ccs.installationTemplates[idx];

	return NULL;
}

/**
 * @brief Setup new installation
 * @sa INS_NewInstallation
 */
void INS_SetUpInstallation (installation_t* installation, installationTemplate_t *installationTemplate)
{
	const int newInstallationAlienInterest = 1.0f;
	int i;

	assert(installation);

	installation->idx = ccs.numInstallations - 1;
	installation->founded = qtrue;
	installation->installationStatus = INSTALLATION_UNDER_CONSTRUCTION;
	installation->installationTemplate = installationTemplate;
	installation->buildStart = ccs.date.day;

	/* Reset current capacities. */
	installation->aircraftCapacity.cur = 0;

	/* this cvar is used for disabling the installation build button on geoscape if MAX_INSTALLATIONS was reached */
	Cvar_Set("mn_installation_count", va("%i", ccs.numInstallations));

	/* this cvar is needed by INS_SetBuildingByClick below*/
	Cvar_SetValue("mn_installation_id", installation->idx);

	installation->numUFOsInInstallation = 0;

	/* a new installation is not discovered (yet) */
	installation->alienInterest = newInstallationAlienInterest;

	/* intialise hit points */
	installation->installationDamage = installation->installationTemplate->maxDamage;

	/* initialise Batteries */
	installation->numBatteries = installation->installationTemplate->maxBatteries;

	/* Add defenceweapons to storage */
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *item = &csi.ods[i];
		/* this is a craftitem but also dummy */
		if (INV_IsBaseDefenceItem(item))
			installation->storage.num[item->idx] = installation->installationTemplate->maxBatteries;
	}
	BDEF_InitialiseInstallationSlots(installation);

	Com_DPrintf(DEBUG_CLIENT, "INS_SetUpInstallation: id = %s, range = %i, batteries = %i, ufos = %i\n",
		installation->installationTemplate->id, installation->installationTemplate->radarRange,
		installation->installationTemplate->maxBatteries, installation->installationTemplate->maxUFOsStored);

	/* Reset Radar range */
	RADAR_Initialise(&(installation->radar), 0.0f, 0.0f, 1.0f, qtrue);
	RADAR_UpdateInstallationRadarCoverage(installation, installation->installationTemplate->radarRange, installation->installationTemplate->trackingRange);
}

/**
 * @brief Get the lower IDX of unfounded installation.
 * @return instIdx of first Installation Unfounded, or MAX_INSTALLATIONS is maximum installation number is reached.
 */
int INS_GetFirstUnfoundedInstallation (void)
{
	int instIdx;

	for (instIdx = 0; instIdx < MAX_INSTALLATIONS; instIdx++) {
		const installation_t const *installation = INS_GetFoundedInstallationByIDX(instIdx);
		if (!installation)
			return instIdx;
	}

	return MAX_INSTALLATIONS;
}

/**
 * @brief Cleans all installations but restore the installation names
 * @sa CL_GameNew
 */

void INS_NewInstallations (void)
{
	/* reset installations */
	int i;
	char title[MAX_VAR];
	installation_t *installation;

	for (i = 0; i < MAX_INSTALLATIONS; i++) {
		installation = INS_GetInstallationByIDX(i);
		Q_strncpyz(title, installation->name, sizeof(title));
/*		INS_ClearInstallation(installation); */
		Q_strncpyz(installation->name, title, sizeof(title));
	}
}


#ifdef DEBUG

/**
 * @brief Just lists all installations with their data
 * @note called with debug_listinstallation
 * @note Just for debugging purposes - not needed in game
 * @todo To be extended for load/save purposes
 */
static void INS_InstallationList_f (void)
{
	int i;
	installation_t *installation;

	for (i = 0, installation = ccs.installations; i < MAX_INSTALLATIONS; i++, installation++) {
		if (!installation->founded) {
			Com_Printf("Installation idx %i not founded\n\n", i);
			continue;
		}

		Com_Printf("Installation idx %i\n", installation->idx);
		Com_Printf("Installation name %s\n", installation->name);
		Com_Printf("Installation founded %i\n", installation->founded);
		Com_Printf("Installation numUFOsInInstallation %i\n", installation->numUFOsInInstallation);
		Com_Printf("Installation sensorWidth %i\n", installation->radar.range);
		Com_Printf("Installation numSensoredAircraft %i\n", installation->radar.numUFOs);
		Com_Printf("Installation Alien interest %f\n", installation->alienInterest);
		Com_Printf("\nInstallation aircraft %i\n", installation->numUFOsInInstallation);
		Com_Printf("Installation pos %.02f:%.02f\n", installation->pos[0], installation->pos[1]);
		Com_Printf("\n\n");
	}
}
#endif

/**
 * @brief Destroys an installation
 * @param[in] pointer to the installation to be destroyed
 */
void INS_DestroyInstallation (installation_t *installation)
{
	if (!installation)
		return;
	if (!installation->founded)
		return;

	RADAR_UpdateInstallationRadarCoverage(installation, 0, 0);
	CP_MissionNotifyInstallationDestroyed(installation);

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Installation %s was destroyed."), _(installation->name));
	MSO_CheckAddNewMessage(NT_INSTALLATION_DESTROY, _("Installation destroyed"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);

	REMOVE_ELEM_ADJUST_IDX(ccs.installations, installation->idx, ccs.numInstallations);

	ccs.installationCurrent = NULL;
}

installation_t *INS_GetCurrentSelectedInstallation (void)
{
	return ccs.installationCurrent;
}

/**
 * @brief Check conditions for new installation and build it.
 * @param[in] pos Position on the geoscape.
 * @return True if the installation has been build.
 * @sa INS_BuildInstallation
 */
qboolean INS_NewInstallation (installation_t* installation, installationTemplate_t *installationTemplate, vec2_t pos)
{
	byte *colorTerrain;

	assert(installation);

	if (installation->founded) {
		Com_DPrintf(DEBUG_CLIENT, "INS_NewInstallation: installation already founded: %i\n", installation->idx);
		return qfalse;
	} else if (ccs.numInstallations >= B_GetInstallationLimit()) {
		Com_DPrintf(DEBUG_CLIENT, "INS_NewInstallation: max installation limit hit\n");
		return qfalse;
	}

	colorTerrain = MAP_GetColor(pos, MAPTYPE_TERRAIN);

	if (MapIsWater(colorTerrain)) {
		/* This should already have been catched in MAP_MapClick (cl_menu.c), but just in case. */
		MS_AddNewMessage(_("Notice"), _("Could not set up your installation at this location"), qfalse, MSG_INFO, NULL);
		return qfalse;
	} else {
		Com_DPrintf(DEBUG_CLIENT, "INS_NewInstallation: zoneType: '%s'\n", MAP_GetTerrainType(colorTerrain));
	}

	Com_DPrintf(DEBUG_CLIENT, "Colorvalues for installation terrain: R:%i G:%i B:%i\n", colorTerrain[0], colorTerrain[1], colorTerrain[2]);

	/* build installation */
	Vector2Copy(pos, installation->pos);

	ccs.numInstallations++;

	return qtrue;
}

/**
 * @brief Resets console commands.
 * @sa MN_ResetMenus
 */
void INS_InitStartup (void)
{
	int idx;

	Com_DPrintf(DEBUG_CLIENT, "Reset installation\n");

	for (idx = 0; idx < ccs.numInstallationTemplates; idx++) {
		ccs.installationTemplates[idx].id = NULL;
		ccs.installationTemplates[idx].name = NULL;
		ccs.installationTemplates[idx].cost = 0;
		ccs.installationTemplates[idx].radarRange = 0.0f;
		ccs.installationTemplates[idx].trackingRange = 0.0f;
		ccs.installationTemplates[idx].maxUFOsStored = 0;
		ccs.installationTemplates[idx].maxBatteries = 0;
	}

	/* add commands and cvars */
#ifdef DEBUG
	Cmd_AddCommand("debug_listinstallation", INS_InstallationList_f, "Print installation information to the game console");
#endif
}

/**
 * @brief Counts the number of installations.
 * @return The number of founded installations.
 */
int INS_GetFoundedInstallationCount (void)
{
	int i, cnt = 0;

	for (i = 0; i < MAX_INSTALLATIONS; i++) {
		if (!ccs.installations[i].founded)
			continue;
		cnt++;
	}

	return cnt;
}

/**
 * @brief Check if some installation are build.
 * @note Daily called.
 */
void INS_UpdateInstallationData (void)
{
	int instIdx;

	for (instIdx = 0; instIdx < MAX_INSTALLATIONS; instIdx++) {
		installation_t *installation = INS_GetFoundedInstallationByIDX(instIdx);
		if (!installation)
			continue;

		if ((installation->installationStatus == INSTALLATION_UNDER_CONSTRUCTION)
		 && installation->buildStart
		 && installation->buildStart + installation->installationTemplate->buildTime <= ccs.date.day) {
			installation->installationStatus = INSTALLATION_WORKING;
			RADAR_UpdateInstallationRadarCoverage(installation, installation->installationTemplate->radarRange, installation->installationTemplate->trackingRange);

			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Construction of installation %s finished."), _(installation->name));
				MSO_CheckAddNewMessage(NT_INSTALLATION_BUILDFINISH, _("Installation finished"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
		}
	}
}

static const value_t installation_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(installationTemplate_t, name), 0},
	{"radar_range", V_INT, offsetof(installationTemplate_t, radarRange), MEMBER_SIZEOF(installationTemplate_t, radarRange)},
	{"radar_tracking_range", V_INT, offsetof(installationTemplate_t, trackingRange), MEMBER_SIZEOF(installationTemplate_t, trackingRange)},
	{"max_batteries", V_INT, offsetof(installationTemplate_t, maxBatteries), MEMBER_SIZEOF(installationTemplate_t, maxBatteries)},
	{"max_ufo_stored", V_INT, offsetof(installationTemplate_t, maxUFOsStored), MEMBER_SIZEOF(installationTemplate_t, maxUFOsStored)},
	{"max_damage", V_INT, offsetof(installationTemplate_t, maxDamage), MEMBER_SIZEOF(installationTemplate_t, maxDamage)},
	{"model", V_CLIENT_HUNK_STRING, offsetof(installationTemplate_t, model), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief Copies an entry from the installation description file into the list of installation templates.
 * @note Parses one "installation" entry in the installation.ufo file and writes
 * it into the next free entry in installationTemplates.
 * @param[in] name Unique test-id of a installationTemplate_t.
 * @param[in] text @todo document this ... It appears to be the whole following text that is part of the "building" item definition in .ufo.
 */
void INS_ParseInstallations (const char *name, const char **text)
{
	installationTemplate_t *installation;
	const char *errhead = "INS_ParseInstallations: unexpected end of file (names ";
	const char *token;
	const value_t *vp;
	int i;

	/* get id list body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("INS_ParseInstallations: installation \"%s\" without body ignored\n", name);
		return;
	}

	if (!name) {
		Com_Printf("INS_ParseInstallations: installation name not specified.\n");
		return;
	}

	if (ccs.numInstallationTemplates >= MAX_INSTALLATION_TEMPLATES) {
		Com_Printf("INS_ParseInstallations: too many installation templates\n");
		ccs.numInstallationTemplates = MAX_INSTALLATION_TEMPLATES;	/* just in case it's bigger. */
		return;
	}

	for (i = 0; i < ccs.numInstallationTemplates; i++) {
		if (!strcmp(ccs.installationTemplates[i].name, name)) {
			Com_Printf("INS_ParseInstallations: Second installation with same name found (%s) - second ignored\n", name);
			return;
		}
	}

	/* new entry */
	installation = &ccs.installationTemplates[ccs.numInstallationTemplates];
	memset(installation, 0, sizeof(*installation));
	installation->id = Mem_PoolStrDup(name, cl_campaignPool, 0);

	Com_DPrintf(DEBUG_CLIENT, "...found installation %s\n", installation->id);

	ccs.numInstallationTemplates++;
	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = installation_vals; vp->string; vp++)
			if (!strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				case V_TRANSLATION_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)installation + (int)vp->ofs), cl_campaignPool, 0);
					break;
				default:
					if (Com_EParseValue(installation, token, vp->type, vp->ofs, vp->size) == -1)
						Com_Printf("INS_ParseInstallations: Wrong size for value %s\n", vp->string);
					break;
				}
				break;
			}

		/* other values */
		if (!vp->string) {
			if (!strcmp(token, "cost")) {
				char cvarname[MAX_VAR] = "mn_installation_";

				Q_strcat(cvarname, installation->id, MAX_VAR);
				Q_strcat(cvarname, "_cost", MAX_VAR);

				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				installation->cost = atoi(token);

				Cvar_Set(cvarname, va(_("%d c"), atoi(token)));
			} else if (!strcmp(token, "buildtime")) {
				char cvarname[MAX_VAR];


				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				installation->buildTime = atoi(token);

				Com_sprintf(cvarname, sizeof(cvarname), "mn_installation_%s_buildtime", installation->id);
				Cvar_Set(cvarname, va(ngettext("%d day\n", "%d days\n", atoi(token)), atoi(token)));
			}
		}
	} while (*text);
}

/**
 * @brief Save callback for savegames in xml
 * @sa INS_LoadXML
 * @sa SAV_GameSaveXML
 */
qboolean INS_SaveXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *n;
	n = mxml_AddNode(p, "installations");
	for (i = 0; i < MAX_INSTALLATIONS; i++) {
		const installation_t *inst = INS_GetInstallationByIDX(i);
		mxml_node_t *s, *ss;
		if (!inst->founded)
			continue;
		s = mxml_AddNode(n, "installation");
		mxml_AddInt(s, "idx", inst->idx);
		mxml_AddBool(s, "founded", inst->founded);
		mxml_AddString(s, "templateid", inst->installationTemplate->id);
		mxml_AddString(s, "name", inst->name);
		mxml_AddPos3(s, "pos", inst->pos);
		mxml_AddInt(s, "status", inst->installationStatus);
		mxml_AddInt(s, "damage", inst->installationDamage);
		mxml_AddFloat(s, "alieninterest", inst->alienInterest);
		mxml_AddInt(s, "buildstart", inst->buildStart);

		ss = mxml_AddNode(s, "batteries");
		mxml_AddInt(ss, "num", inst->numBatteries);
		B_SaveBaseSlotsXML(inst->batteries, inst->numBatteries, ss);

		/* store equipments */
		/* reducing redundant code */
		B_SaveStorageXML(s, inst->storage);

		/** @todo aircraft (don't save capacities, they should
		 * be recalculated after loading) */
	}
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa INS_SaveXML
 * @sa SAV_GameLoadXML
 * @sa INS_LoadItemSlots
 */
qboolean INS_LoadXML (mxml_node_t *p)
{
	mxml_node_t *s;
	mxml_node_t *n = mxml_GetNode(p, "installations");
	if (!n)
		return qfalse;

	for (s = mxml_GetNode(n, "installation"); s ; s = mxml_GetNextNode(s,n, "installation")) {
		mxml_node_t *ss;
		const int idx = mxml_GetInt(s, "idx", 0);
		installation_t *inst = INS_GetInstallationByIDX(idx);
		inst->idx = idx;
		inst->founded = mxml_GetBool(s, "founded", inst->founded);
		/* should never happen, we only save founded installations */
		if (!inst->founded)
			continue;
		inst->installationTemplate = INS_GetInstallationTemplateFromInstallationID(mxml_GetString(s, "templateid"));
		if (!inst->installationTemplate) {
			Com_Printf("Could not find installation template\n");
			return qfalse;
		}
		ccs.numInstallations++;
		Q_strncpyz(inst->name, mxml_GetString(s, "name"), sizeof(inst->name));

		mxml_GetPos3(s, "pos", inst->pos);

		inst->installationStatus = mxml_GetInt(s, "status", 0);
		inst->installationDamage = mxml_GetInt(s, "damage", 0);
		inst->alienInterest = mxml_GetFloat(s, "alieninterest", 0.0);

		RADAR_InitialiseUFOs(&inst->radar);
		RADAR_Initialise(&(inst->radar), 0.0f, 0.0f, 1.0f, qtrue);
		RADAR_UpdateInstallationRadarCoverage(inst, inst->installationTemplate->radarRange, inst->installationTemplate->trackingRange);

		inst->buildStart = mxml_GetInt(s, "buildstart", 0);

		/* read battery slots */
		BDEF_InitialiseInstallationSlots(inst);

		ss = mxml_GetNode(s, "batteries");
		if (!ss) {
			Com_Printf("INS_LoadXML: Batteries not defined!\n");
			return qfalse;
		}
		inst->numBatteries = mxml_GetInt(ss, "num", 0);
		if (inst->numBatteries > inst->installationTemplate->maxBatteries) {
			Com_Printf("Installation has more batteries than possible, using upper bound\n");
			inst->numBatteries = inst->installationTemplate->maxBatteries;
		}
		B_LoadBaseSlotsXML(inst->batteries, inst->numBatteries, ss);

		B_LoadStorageXML(s, &inst->storage);
		/** @todo aircraft */
		/** @todo don't forget to recalc the capacities like we do for bases */
	}
	Cvar_SetValue("mn_installation_count", ccs.numInstallations);
	return qtrue;
}
