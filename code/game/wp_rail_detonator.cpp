/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#include "g_local.h"
#include "b_local.h"
#include "g_functions.h"
#include "wp_saber.h"
#include "w_local.h"

//-----------------------
//	Rail Detonator
//-----------------------

//---------------------------------------------------------
void railDet_stick(gentity_t *self, gentity_t *other, trace_t *trace)
//---------------------------------------------------------
{
	char sticky_proj[MAX_QPATH] = "models/weapons2/rail_detonator/ammo.glm";

	gi.G2API_InitGhoul2Model(self->ghoul2, sticky_proj, self->s.modelindex, NULL_HANDLE, NULL_HANDLE, 0, 0);
	//set the *flash tag as genericBolt1, so we can then use it over in cgame and play the FX there.
	self->genericBolt1 = gi.G2API_AddBolt(&self->ghoul2[0], "*flash");

	G_Sound(self, G_SoundIndex("sound/weapons/rail_detonator/chargeloop.wav"));

	// make us so we can take damage
	self->classname = "raildet_stick";
	self->s.eType = ET_GENERAL;
	self->clipmask = MASK_SHOT;
	self->contents = CONTENTS_SHOTCLIP;
	self->takedamage = qtrue;
	self->health = 25;

	self->e_DieFunc = dieF_railDet_die;

	VectorSet(self->maxs, 10, 10, 10);
	VectorScale(self->maxs, -1, self->mins);

	self->activator = self->owner;
	self->owner = NULL;

	self->e_TouchFunc = touchF_NULL;
	self->e_ThinkFunc = thinkF_WP_RailDetThink;
	self->nextthink = level.time + RAILDET_THINK_TIME;
	self->delay = level.time + RAILDET_TIME; // How long 'til she blows

	WP_Stick(self, trace, -3.0f);
}

//---------------------------------------------------------
void WP_RailDetThink( gentity_t *ent )
//---------------------------------------------------------
{
	int			count;
	qboolean	blow = qfalse;

	// Thermal detonators for the player do occasional radius checks and blow up if there are entities in the blast radius
	//	This is done so that the main fire is actually useful as an attack.  We explode anyway after delay expires.

	if (ent->delay > level.time)
	{
		//	Finally, we force it to bounce at least once before doing the special checks, otherwise it's just too easy for the player?
		if (ent->has_bounced)
		{
			count = G_RadiusList(ent->currentOrigin, RAILDET_RAD, ent, qtrue, ent_list);

			for (int i = 0; i < count; i++)
			{
				if (ent_list[i]->s.number == 0)
				{
					// avoid deliberately blowing up next to the player, no matter how close any enemy is..
					//	...if the delay time expires though, there is no saving the player...muwhaaa haa ha
					blow = qfalse;
					break;
				}
				else if (ent_list[i]->client
					&& ent_list[i]->health > 0)
				{
					//FIXME! sometimes the ent_list order changes, so we should make sure that the player isn't anywhere in this list
					blow = qtrue;
				}
			}
		}
	}
	else
	{
		// our death time has arrived, even if nothing is near us
		blow = qtrue;
	}

	if (blow)
	{
		ent->e_ThinkFunc = thinkF_railDetExplode;
		ent->nextthink = level.time + 50;
	}
	else
	{
		// we probably don't need to do this thinking logic very often...maybe this is fast enough?
		ent->nextthink = level.time + RAILDET_THINK_TIME;
	}
}

//---------------------------------------------------------
void railDetExplode(gentity_t *ent)
//---------------------------------------------------------
{
	vec3_t	pos;

	VectorSet(pos, ent->currentOrigin[0], ent->currentOrigin[1], ent->currentOrigin[2] + 8);

	ent->takedamage = qfalse; // don't allow double deaths!

	G_RadiusDamage(ent->currentOrigin, ent->owner, weaponData[WP_RAIL_DETONATOR].splashDamage, weaponData[WP_RAIL_DETONATOR].splashRadius, NULL, MOD_EXPLOSIVE_SPLASH);
	
	G_PlayEffect("thermal/explosion", ent->currentOrigin);

	G_FreeEntity(ent);
}

//-------------------------------------------------------------------------------------------------------------
void railDet_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod, int dFlags, int hitLoc)
//-------------------------------------------------------------------------------------------------------------
{
	railDetExplode(self);
}

//---------------------------------------------------------
void WP_FireRailDet( gentity_t *ent, qboolean alt_fire )
//---------------------------------------------------------
{
	vec3_t	start;
	int		damage	= weaponData[WP_RAIL_DETONATOR].damage;
	float	vel = RAILDET_VELOCITY;

	VectorCopy( muzzle, start );
	WP_TraceSetStart( ent, start, vec3_origin, vec3_origin );//make sure our start point isn't on the other side of a wall

	gentity_t *missile = CreateMissile( start, forwardVec, vel, 10000, ent, alt_fire );

	missile->classname = "raildet_proj";
	missile->s.weapon = WP_RAIL_DETONATOR;
	missile->mass = 10;

	// Do the damages
	if ( ent->s.number != 0 )
	{
		if ( g_spskill->integer == 0 )
		{
			damage = weaponData[WP_RAIL_DETONATOR].damage - RAILDET_NPC_DMG_EASY;
		}
		else if ( g_spskill->integer == 1 )
		{
			damage = weaponData[WP_RAIL_DETONATOR].damage - RAILDET_NPC_DMG_NORMAL;
		}
		else
		{
			damage = weaponData[WP_RAIL_DETONATOR].damage - RAILDET_NPC_DMG_HARD;
		}
	}

	if ( alt_fire )
	{
		missile->s.eFlags |= EF_MISSILE_STICK;
		missile->e_TouchFunc = touchF_railDet_stick;
	}
	else
	{
		missile->e_ThinkFunc = thinkF_railDetExplode;
		missile->nextthink = level.time + RAILDET_TIME; // How long 'til she blows
	}

	// Make it easier to hit things
	VectorSet( missile->maxs, RAILDET_SIZE, RAILDET_SIZE, RAILDET_SIZE );
	VectorScale( missile->maxs, -1, missile->mins );

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if ( alt_fire )
	{
		missile->methodOfDeath = MOD_RAIL_DET_ALT;
		missile->splashMethodOfDeath = MOD_RAIL_DET_ALT;// ?SPLASH;
	}
	else
	{
		missile->methodOfDeath = MOD_RAIL_DET;
		missile->splashMethodOfDeath = MOD_RAIL_DET;// ?SPLASH;		
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
		
	missile->splashDamage = weaponData[WP_RAIL_DETONATOR].splashDamage;
	missile->splashRadius = weaponData[WP_RAIL_DETONATOR].splashRadius;

	// we don't want it to ever bounce
	missile->bounceCount = 0;
}
