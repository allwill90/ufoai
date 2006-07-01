/**
 * @file cl_shader.c
 * @brief Parses and loads shaders from files and passes them to the renderer.
 *
 *
 * Reads in a shader program from a file, puts it in a struct and tell the renderer about the location of this struct.
 * This is to allow the adding of shaders without recompiling the binaries.
 * You can add shaders for each texture; new maps can bring new textures.
 * Without a scriptable shader interface we have to rebuild the renderer to get a shader for a specific texture.
 * They are here and not in the renderer because the file parsers are part of the client.
 * TODO: Add command to re-initialise shaders to allow interactive debugging.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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


#include "client.h"

#define	SHADERFS(x)	(size_t)&(((shader_t *)0)->x)

int r_numshaders;

/* shader_t is in client/ref.h */
/* r_shaders is external and is linked to cl.refdef.shaders in cl_view.c V_RenderView */
shader_t r_shaders[MAX_SHADERS];

value_t shader_values[] = {
	{"filename", V_STRING, SHADERFS(filename)},
	{"frag", V_BOOL, SHADERFS(frag)},
	{"vertex", V_BOOL, SHADERFS(vertex)},
	{NULL, 0, 0}
};

/*
======================
CL_ParseShaders

Called from CL_ParseScriptSecond
======================
*/
void CL_ParseShaders(char *title, char **text)
{
	shader_t *entry;
	value_t *v;
	char *errhead = "CL_ParseShaders: unexptected end of file (names ";
	char *token;

	/* get name list body body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("CL_ParseShaders: shader \"%s\" without body ignored\n", title);
		return;
	}

	/* new entry */
	entry = &r_shaders[r_numshaders++];
	memset(entry, 0, sizeof(shader_t));
	Q_strncpyz(entry->title, title, MAX_VAR);
	do {
		/* get the name type */
		token = COM_EParse(text, errhead, title);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* get values */
		for (v = shader_values; v->string; v++)
			if (!Q_strcmp(token, v->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, title);
				if (!*text)
					return;

				if (v->ofs && v->type != V_NULL)
					Com_ParseValue(entry, token, v->type, v->ofs);
				break;
			}

		if (!v->string)
			Com_Printf("CL_ParseShaders: unknown token \"%s\" ignored (entry %s)\n", token, title);

	} while (*text);
}

void CL_ShaderList_f(void)
{
	int i;

	for (i = 0; i < r_numshaders; i++) {
		Com_Printf("Shader %s\n", r_shaders[i].title);
		Com_Printf("..filename: %s\n", r_shaders[i].filename);
		Com_Printf("..frag %i\n", (int) r_shaders[i].frag);
		Com_Printf("..vertex %i\n", (int) r_shaders[i].vertex);
		Com_Printf("\n");
	}
}
