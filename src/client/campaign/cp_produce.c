/**
 * @file cp_produce.c
 * @brief Single player production stuff
 * @note Production stuff functions prefix: PR_
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../cl_game.h"
#include "../menu/m_main.h"
#include "../menu/m_popup.h"
#include "../menu/m_nodes.h"
#include "../mxml/mxml_ufoai.h"
#include "cp_campaign.h"
#include "cp_ufo.h"
#include "cp_produce_callbacks.h"

/** @brief Used in production costs (to allow reducing prices below 1x). */
const int PRODUCE_FACTOR = 1;
const int PRODUCE_DIVISOR = 2;

/** @brief Default amount of workers, the produceTime for technologies is defined. */
/** @note producetime for technology entries is the time for PRODUCE_WORKERS amount of workers. */
static const int PRODUCE_WORKERS = 10;

static cvar_t* mn_production_limit;		/**< Maximum items in queue. */
static cvar_t* mn_production_workers;		/**< Amount of hired workers in base. */
static cvar_t* mn_production_amount;	/**< Amount of the current production; if no production, an invalid value */

/**
 * @brief Calculates the fraction (percentage) of production of an item in 1 hour.
 * @param[in] base Pointer to the base with given production.
 * @param[in] tech Pointer to the technology for given production.
 * @param[in] sufo Pointer to disassembled UFO.
 * @sa PR_ProductionRun
 * @sa PR_ItemProductionInfo
 * @sa PR_DisassemblyInfo
 * @return 0 if the production does not make any progress, 1 if the whole item is built in 1 hour
 */
float PR_CalculateProductionPercentDone (const base_t *base, const technology_t *tech, const storedUFO_t *const sufo)
{
	signed int allWorkers = 0;
	signed int maxWorkers = 0;
	signed int timeDefault = 0;
	float distanceFactor = 0.0f;

	assert(base);
	assert(tech);

	/* Check how many workers hired in this base. */
	allWorkers = E_CountHired(base, EMPL_WORKER);
	/* We will not use more workers than base capacity. */
	maxWorkers = min(allWorkers, base->capacities[CAP_WORKSPACE].max);

	if (!sufo) {
		/* This is the default production time for 10 workers. */
		timeDefault = tech->produceTime;
	} else {
		assert(sufo->comp);
		/* This is the default disassembly time for 10 workers. */
		timeDefault = sufo->comp->time;
		/* Production is 4 times longer when installation is on Antipodes */
		distanceFactor = GetDistanceOnGlobe(sufo->installation->pos, base->pos) / 45.0f;
		assert(distanceFactor >= 0.0f);
		/* Penalty starts when distance is greater than 45 degrees */
		distanceFactor = max(1.0f, distanceFactor);
		Com_DPrintf(DEBUG_CLIENT, "PR_CalculatePercentDone: distanceFactor is %f\n", distanceFactor);
	}
	if (maxWorkers == PRODUCE_WORKERS) {
		/* No need to calculate: timeDefault is for PRODUCE_WORKERS workers. */
		const float fraction =  1.0f / ((NULL != sufo) ? (distanceFactor * timeDefault) : timeDefault);
		Com_DPrintf(DEBUG_CLIENT, "PR_CalculatePercentDone: workers: %i, tech: %s, percent: %f\n",
			maxWorkers, tech->id, fraction);
		return fraction;
	} else {
		/* Calculate the fraction of item produced for our amount of workers. */
		/* NOTE: I changed algorithm for a more realistic one, varying like maxworkers^2 -- Kracken 2007/11/18
		 * now, production time is divided by 4 each time you double the number of worker */
		const float fraction = ((float)maxWorkers / (PRODUCE_WORKERS * ((NULL != sufo) ? (distanceFactor * timeDefault) : timeDefault ))
			* ((float)maxWorkers / PRODUCE_WORKERS));
		Com_DPrintf(DEBUG_CLIENT, "PR_CalculatePercentDone: workers: %i, tech: %s, percent: %f\n",
			maxWorkers, tech->id, fraction);
		/* Don't allow to return fraction greater than 1 (you still need at least 1 hour to produce an item). */
		return min(fraction, 1.0f);
	}
}

/**
 * @brief Remove or add the required items from/to the a base.
 * @param[in] base Pointer to base.
 * @param[in] amount How many items are planned to be added (positive number) or removed (negative number).
 * @param[in] reqs The production requirements of the item that is to be produced. These included numbers are multiplied with 'amount')
 * @todo This doesn't check yet if there are more items removed than are in the base-storage (might be fixed if we used a storage-function with checks, otherwise we can make it a 'condition' in order to run this function.
 */
void PR_UpdateRequiredItemsInBasestorage (base_t *base, int amount, requirements_t *reqs)
{
	int i;
	equipDef_t *ed;

	if (!base)
		return;

	ed = &base->storage;
	if (!ed)
		return;

	if (amount == 0)
		return;

	for (i = 0; i < reqs->numLinks; i++) {
		requirement_t *req = &reqs->links[i];
		if (req->type == RS_LINK_ITEM) {
			const objDef_t *item = req->link;
			assert(item);
			if (amount > 0) {
				/* Add items to the base-storage. */
				ed->num[item->idx] += (req->amount * amount);
			} else { /* amount < 0 */
				/* Remove items from the base-storage. */
				ed->num[item->idx] -= (req->amount * -amount);
			}
		}
	}
}

/**
 * @brief Delete the selected entry from the queue.
 * @param[in] base Pointer to base, where the queue is.
 * @param[in] queue Pointer to the queue.
 * @param[in] index Selected index in queue.
 */
void PR_QueueDelete (base_t *base, production_queue_t *queue, int index)
{
	int i;
	production_t *prod = &queue->items[index];

	if (prod->ufo) {
		prod->ufo->disassembly = NULL;
	} else if (prod->itemsCached && !prod->aircraft) {
		/* Get technology of the item in the selected queue-entry. */
		const objDef_t *od = prod->item;
		if (od->tech) {
			assert(base);
			/* Add all items listed in the prod.-requirements /multiplied by amount) to the storage again. */
			PR_UpdateRequiredItemsInBasestorage(base, prod->amount, &od->tech->requireForProduction);
		} else {
			Com_DPrintf(DEBUG_CLIENT, "PR_QueueDelete: Problem getting technology entry for %i\n", index);
		}
		prod->itemsCached = qfalse;
	}

	REMOVE_ELEM_ADJUST_IDX(queue->items, index, queue->numItems);

	/* Adjust ufos' disassembly pointer */
	for (i = index; i < queue->numItems; i++) {
		production_t *disassembly = &queue->items[i];

		if (disassembly->ufo)
			disassembly->ufo->disassembly = disassembly;
	}
}

/**
 * @brief Moves the given queue item in the given direction.
 * @param[in] queue Pointer to the queue.
 * @param[in] index
 * @param[in] dir
 */
void PR_QueueMove (production_queue_t *queue, int index, int dir)
{
	const int newIndex = max(0, min(index + dir, queue->numItems - 1));
	int i;
	production_t saved;

	if (newIndex == index)
		return;

	saved = queue->items[index];

	/* copy up */
	for (i = index; i < newIndex; i++) {
		queue->items[i] = queue->items[i + 1];
		queue->items[i].idx = i;
		if (queue->items[i].ufo)
			queue->items[i].ufo->disassembly = &(queue->items[i]);
	}

	/* copy down */
	for (i = index; i > newIndex; i--) {
		queue->items[i] = queue->items[i - 1];
		queue->items[i].idx = i;
		if (queue->items[i].ufo)
			queue->items[i].ufo->disassembly = &(queue->items[i]);
	}

	/* insert item */
	queue->items[newIndex] = saved;
	queue->items[newIndex].idx = newIndex;
	if (queue->items[newIndex].ufo)
		queue->items[newIndex].ufo->disassembly = &(queue->items[newIndex]);
}

/**
 * @brief Queues the next production in the queue.
 * @param[in] base Pointer to the base.
 */
void PR_QueueNext (base_t *base)
{
	production_queue_t *queue = &ccs.productions[base->idx];

	PR_QueueDelete(base, queue, 0);

	if (queue->numItems == 0) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Production queue for %s is empty"), base->name);
		MSO_CheckAddNewMessage(NT_PRODUCTION_QUEUE_EMPTY, _("Production queue empty"), cp_messageBuffer, qfalse, MSG_PRODUCTION, NULL);
	}
}

/**
 * @brief clears the production queue on a base
 */
static void PR_EmptyQueue (base_t *base)
{
	production_queue_t *queue;

	if (!base)
		return;

	queue = &ccs.productions[base->idx];
	if (!queue)
		return;

	while (queue->numItems)
		PR_QueueDelete(base, queue, 0);
}

/**
 * @brief moves the first production to the bottom of the list
 */
static void PR_ProductionRollBottom_f (void)
{
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	queue = &ccs.productions[base->idx];

	if (queue->numItems < 2)
		return;

	PR_QueueMove(queue, 0, queue->numItems - 1);
}

/**
 * @brief Disassembles item, adds components to base storage and calculates all components size.
 * @param[in] base Pointer to base where the disassembling is being made.
 * @param[in] comp Pointer to components definition.
 * @param[in] condition condition of the item/UFO being disassembled, objects gathered from disassembly decreased to that factor
 * @param[in] calculate True if this is only calculation of item size, false if this is real disassembling.
 * @return Size of all components in this disassembling.
 */
static int PR_DisassembleItem (base_t *base, components_t *comp, float condition, qboolean calculate)
{
	int i;
	int size = 0;

	if (!calculate && !base)	/* We need base only if this is real disassembling. */
		Com_Error(ERR_DROP, "PR_DisassembleItem: No base given");

	assert(comp);
	for (i = 0; i < comp->numItemtypes; i++) {
		const objDef_t *compOd = comp->items[i];
		const int amount = (condition < 1 && comp->itemAmount2[i] != COMP_ITEMCOUNT_SCALED) ? comp->itemAmount2[i] : round(comp->itemAmount[i] * condition);

		assert(compOd);
		size += compOd->size * amount;
		/* Add to base storage only if this is real disassembling, not calculation of size. */
		if (!calculate) {
			if (!strcmp(compOd->id, ANTIMATTER_TECH_ID))
				B_ManageAntimatter(base, amount, qtrue);
			else
				B_UpdateStorageAndCapacity(base, compOd, amount, qfalse, qfalse);
			Com_DPrintf(DEBUG_CLIENT, "PR_DisassembleItem: added %i amounts of %s\n", amount, compOd->id);
		}
	}
	return size;
}

/**
 * @brief Runs the production of an item or an aircraft
 * @sa PR_DisassemblingFrame
 * @sa PR_ProductionRun
 * @param base The base to produce in
 * @param prod The production that is running
 */
static void PR_ProductionFrame (base_t* base, production_t *prod)
{
	const objDef_t *od;
	const aircraft_t *aircraft;

	if (!prod->production)
		return;

	if (!prod->item && !prod->aircraft)
		return;

	if (prod->item) {
		od = prod->item;
		aircraft = NULL;
	} else if (prod->aircraft) {
		od = NULL;
		aircraft = prod->aircraft;
	}

	if (od) {
		/* Not enough money to produce more items in this base. */
		if (od->price * PRODUCE_FACTOR / PRODUCE_DIVISOR > ccs.credits) {
			if (!prod->creditMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough credits to finish production in %s.\n"), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->creditMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		/* Not enough free space in base storage for this item. */
		if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < od->size) {
			if (!prod->spaceMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free storage space in %s. Production postponed.\n"), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->spaceMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		prod->percentDone += (PR_CalculateProductionPercentDone(base, od->tech, NULL) / MINUTES_PER_HOUR);
	} else if (aircraft) {
		/* Not enough money to produce more items in this base. */
		if (aircraft->price * PRODUCE_FACTOR / PRODUCE_DIVISOR > ccs.credits) {
			if (!prod->creditMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough credits to finish production in %s.\n"), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->creditMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		/* Not enough free space in hangars for this aircraft. */
		if (AIR_CalculateHangarStorage(prod->aircraft, base, 0) <= 0) {
			if (!prod->spaceMessage) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free hangar space in %s. Production postponed.\n"), base->name);
				MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
				prod->spaceMessage = qtrue;
			}
			PR_ProductionRollBottom_f();
			return;
		}
		prod->percentDone += (PR_CalculateProductionPercentDone(base, aircraft->tech, NULL) / MINUTES_PER_HOUR);
	}

	if (prod->percentDone >= 1.0f) {
		const char *name;
		technology_t *tech;

		prod->percentDone = 0.0f;
		prod->amount--;

		if (od) {
			CL_UpdateCredits(ccs.credits - (od->price * PRODUCE_FACTOR / PRODUCE_DIVISOR));
			/* Now add it to equipment and update capacity. */
			B_UpdateStorageAndCapacity(base, prod->item, 1, qfalse, qfalse);
			name = _(od->name);
			tech = od->tech;
		} else if (aircraft) {
			CL_UpdateCredits(ccs.credits - (aircraft->price * PRODUCE_FACTOR / PRODUCE_DIVISOR));
			/* Now add new aircraft. */
			AIR_NewAircraft(base, aircraft->id);
			name = _(aircraft->tpl->name);
			tech = NULL;
		}

		/* queue the next production */
		if (prod->amount <= 0) {
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("The production of %s at %s has finished."), name, base->name);
			MSO_CheckAddNewMessage(NT_PRODUCTION_FINISHED, _("Production finished"), cp_messageBuffer, qfalse, MSG_PRODUCTION, tech);
			PR_QueueNext(base);
		}
	}
}

/**
 * @brief Runs the disassembling of a ufo
 * @sa PR_ProductionFrame
 * @sa PR_ProductionRun
 * @param base The base to produce in
 * @param prod The production that is running
 */
static void PR_DisassemblingFrame (base_t* base, production_t* prod)
{
	storedUFO_t *ufo;

	if (prod->production)
		return;

	if (!prod->ufo)
		return;

	ufo = prod->ufo;

	if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < PR_DisassembleItem(NULL, ufo->comp, ufo->condition, qtrue)) {
		if (!prod->spaceMessage) {
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Not enough free storage space in %s. Disassembling postponed.\n"), base->name);
			MSO_CheckAddNewMessage(NT_PRODUCTION_FAILED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
			prod->spaceMessage = qtrue;
		}
		PR_ProductionRollBottom_f();
		return;
	}
	prod->percentDone += (PR_CalculateProductionPercentDone(base, ufo->ufoTemplate->tech, ufo) / MINUTES_PER_HOUR);

	if (prod->percentDone >= 1.0f) {
		base->capacities[CAP_ITEMS].cur += PR_DisassembleItem(base, ufo->comp, ufo->condition, qfalse);

		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("The disassembling of %s at %s has finished."), UFO_TypeToName(ufo->ufoTemplate->ufotype), base->name);
		MSO_CheckAddNewMessage(NT_PRODUCTION_FINISHED, _("Production finished"), cp_messageBuffer, qfalse, MSG_PRODUCTION, ufo->ufoTemplate->tech);

		/* Removing UFO will remove the production too */
		US_RemoveStoredUFO(ufo);
	}
}

/**
 * @brief Checks whether an item is finished.
 * @sa CL_CampaignRun
 * @sa PR_DisassemblingFrame
 * @sa PR_ProductionFrame
 */
void PR_ProductionRun (void)
{
	int i;
	production_t *prod;

	/* Loop through all founded bases. Then check productions
	 * in global data array. Then increase prod->percentDone and check
	 * whether an item is produced. Then add to base storage. */
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;

		/* not actually any active productions */
		if (ccs.productions[i].numItems <= 0)
			continue;

		/* Workshop is disabled because their dependences are disabled */
		if (!PR_ProductionAllowed(base))
			continue;

		prod = &ccs.productions[i].items[0];

		PR_ProductionFrame(base, prod);
		PR_DisassemblingFrame(base, prod);
	}
}

/**
 * @brief Returns true if the current base is able to produce items
 * @param[in] base Pointer to the base.
 * @sa B_BaseInit_f
 */
qboolean PR_ProductionAllowed (const base_t* base)
{
	assert(base);
	if (base->baseStatus != BASE_UNDER_ATTACK
	 && B_GetBuildingStatus(base, B_WORKSHOP)
	 && E_CountHired(base, EMPL_WORKER) > 0) {
		return qtrue;
	} else {
		return qfalse;
	}
}

void PR_ProductionInit (void)
{
	mn_production_limit = Cvar_Get("mn_production_limit", "0", 0, NULL);
	mn_production_workers = Cvar_Get("mn_production_workers", "0", 0, NULL);
	mn_production_amount = Cvar_Get("mn_production_amount", "0", 0, NULL);
}

/**
 * @brief Update the current capacity of Workshop
 * @param[in] base Pointer to the base containing workshop.
 */
void PR_UpdateProductionCap (base_t *base)
{
	assert(base);

	if (base->capacities[CAP_WORKSPACE].max <= 0) {
		PR_EmptyQueue(base);
	}

	if (base->capacities[CAP_WORKSPACE].max >= E_CountHired(base, EMPL_WORKER)) {
		base->capacities[CAP_WORKSPACE].cur = E_CountHired(base, EMPL_WORKER);
	} else {
		base->capacities[CAP_WORKSPACE].cur = base->capacities[CAP_WORKSPACE].max;
	}
}

/**
 * @brief check if an item is producable.
 * @param[in] item Pointer to the item that should be checked.
 */
qboolean PR_ItemIsProduceable (const objDef_t const *item)
{
	assert(item);

	return !(item->tech && item->tech->produceTime == -1);
}

/**
 * @brief Returns the base pointer the production belongs to
 * @param[in] production pointer to the production entry
 * @returns base_t pointer to the base
 */
base_t *PR_ProductionBase (production_t *production) {
	int i;
	for (i = 0; i < ccs.numBases; i++) {
		base_t *base = B_GetBaseByIDX(i);
		const ptrdiff_t diff = ((ptrdiff_t)((production) - ccs.productions[i].items));

		if (diff >= 0 && diff < MAX_PRODUCTIONS)
			return base;
	}
	return NULL;
}

/**
 * @brief Save callback for savegames in XML Format
 * @sa PR_LoadXML
 * @sa SAV_GameSaveXML
 */
qboolean PR_SaveXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *node = mxml_AddNode(p, "production");
	for (i = 0; i < MAX_BASES; i++) {
		const production_queue_t *pq = &ccs.productions[i];
		int j;
		mxml_node_t *snode = mxml_AddNode(node, "queue");

		mxml_AddInt(snode, "numitems", pq->numItems);

		for (j = 0; j < pq->numItems; j++) {
			const objDef_t *item = pq->items[j].item;
			const aircraft_t *aircraft = pq->items[j].aircraft;
			const storedUFO_t *ufo = pq->items[j].ufo;

			mxml_node_t * ssnode = mxml_AddNode(snode, "item");
			assert(item || aircraft || ufo);
			if (item)
				mxml_AddString(ssnode, "itemid", item->id);
			else if (aircraft)
				mxml_AddString(ssnode, "aircraftid", aircraft->id);
			else if (ufo)
				mxml_AddInt(ssnode, "ufoidx", ufo->idx);
			mxml_AddInt(ssnode, "amount", pq->items[j].amount);
			mxml_AddFloat(ssnode, "percentdone", pq->items[j].percentDone);
			mxml_AddBool(ssnode, "items_cached", pq->items[j].itemsCached);
		}
	}
	return qtrue;
}

/**
 * @brief Load callback for xml savegames
 * @sa PR_SaveXML
 * @sa SAV_GameLoadXML
 */
qboolean PR_LoadXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *node, *snode;

	node = mxml_GetNode(p, "production");

	for (i = 0, snode = mxml_GetNode(node, "queue"); i < MAX_BASES && snode;
			i++, snode = mxml_GetNextNode(snode, node, "queue")) {
		int j;
		mxml_node_t *ssnode;
		production_queue_t *pq = &ccs.productions[i];

		pq->numItems = mxml_GetInt(snode, "numitems", 0);

		if (pq->numItems > MAX_PRODUCTIONS) {
			Com_Printf("PR_Load: Too much productions (%i), last %i dropped (baseidx=%i).\n", pq->numItems, pq->numItems - MAX_PRODUCTIONS, i);
			pq->numItems = MAX_PRODUCTIONS;
		}

		for (j = 0, ssnode = mxml_GetNode(snode, "item"); j < pq->numItems && ssnode;
				j++, ssnode = mxml_GetNextNode(ssnode, snode, "item")) {
			const char *s1 = mxml_GetString(ssnode, "itemid");
			const char *s2;
			int ufoIDX;

			pq->items[j].idx = j;
			pq->items[j].amount = mxml_GetInt(ssnode, "amount", 0);
			pq->items[j].percentDone = mxml_GetFloat(ssnode, "percentdone", 0.0);

			if (s1[0] != '\0')
				pq->items[j].item = INVSH_GetItemByID(s1);

			if (pq->items[j].amount <= 0) {
				Com_Printf("PR_Load: Production with amount <= 0 dropped (baseidx=%i, production idx=%i).\n", i, j);
				j--;
				pq->numItems--;
				continue;
			}

			ufoIDX = mxml_GetInt(ssnode, "ufoidx", MAX_STOREDUFOS);
			if (ufoIDX != MAX_STOREDUFOS) {
				storedUFO_t *ufo = US_GetStoredUFOByIDX(ufoIDX);

				if (!ufo) {
					Com_Printf("PR_Load: Could not find ufo idx: %i\n", ufoIDX);
					return qfalse;
				}

				pq->items[j].ufo = ufo;
				pq->items[j].production = qfalse;
				pq->items[j].ufo->disassembly = &(pq->items[j]);
			} else {
				pq->items[j].production = qtrue;
			}

			s2 = mxml_GetString(ssnode, "aircraftid");
			if (s2[0] != '\0')
				pq->items[j].aircraft = AIR_GetAircraft(s2);
			pq->items[j].itemsCached = mxml_GetBool(ssnode, "items_cached", qfalse);
			if (!pq->items[j].item && s1[0] != '\0')
				Com_Printf("PR_Load: Could not find item '%s'\n", s1);
			if (!pq->items[j].aircraft && s2[0] != '\0')
				Com_Printf("PR_Load: Could not find aircraft sample '%s'\n", s2);
		}
	}
	return qtrue;
}
