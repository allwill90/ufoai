/**
 * @file m_node_todo.c
 * @brief A node allowing to tag a GUI with comment (only visible on debug mode).
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../m_nodes.h"
#include "../m_font.h"
#include "../m_parse.h"
#include "../../client.h"
#include "../m_tooltip.h"
#include "m_node_todo.h"
#include "m_node_abstractnode.h"

/**
 * @brief Custom tooltip of todo node
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 */
static void MN_TodoNodeDrawTooltip (menuNode_t *node, int x, int y)
{
	const int tooltipWidth = 250;
	static char tooltiptext[1024];

	const char* text = MN_GetReferenceString(node->menu, node->text);
	if (!text)
		return;

	tooltiptext[0] = '\0';
	/** @todo remove me, just debug code */
	if (!strncmp("It ", text, 3)) {
		tooltiptext[0] = '\0';
	}
	/** @todo remove me, just debug code */
	if (!strncmp("Not", text, 3)) {
		tooltiptext[0] = '\0';
	}
	Q_strcat(tooltiptext, text, sizeof(tooltiptext));
	MN_DrawTooltip("f_small", tooltiptext, x, y, tooltipWidth, 0);
}

static void MN_TodoNodeDraw (menuNode_t *node)
{
	static vec4_t red = {1.0, 0.0, 0.0, 1.0};
	vec2_t pos;

	MN_GetNodeAbsPos(node, pos);
	R_DrawFill(pos[0], pos[1], node->size[0], node->size[1], ALIGN_UL, red);

	if (node->state)
		MN_TodoNodeDrawTooltip(node, mousePosX, mousePosY);
}

static void MN_TodoNodeLoading (menuNode_t *node)
{
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
}

static void MN_TodoNodeLoaded (menuNode_t *node)
{
#if DEBUG
#else
	node->invis = qtrue;
#endif
	node->size[0] = 10;
	node->size[1] = 10;
}

void MN_RegisterTodoNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "todo";
	behaviour->extends = "string";
	behaviour->draw = MN_TodoNodeDraw;
	behaviour->loading = MN_TodoNodeLoading;
	behaviour->loaded = MN_TodoNodeLoaded;
}

