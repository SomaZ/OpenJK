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

// Rail Detonator Weapon
#include "cg_headers.h"

#include "cg_media.h"
#include "FxScheduler.h"

/*
---------------------------
FX_RailDetProjectileThink
---------------------------
*/

void FX_RailDetProjectileThink( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
	{
		forward[2] = 1.0f;
	}

	theFxScheduler.PlayEffect( "rail_detonator/shot", cent->lerpOrigin, forward );
}

/*
---------------------------
FX_RailDetHitWall
---------------------------
*/

void FX_RailDetHitWall(vec3_t origin, vec3_t normal)
{
	theFxScheduler.PlayEffect("thermal/explosion", origin, normal);
}

/*
---------------------------
FX_RailDetHitPlayer
---------------------------
*/

void FX_RailDetHitPlayer(vec3_t origin, vec3_t normal, qboolean humanoid)
{
	theFxScheduler.PlayEffect("thermal/explosion", origin, normal);
}

/*
---------------------------
FX_RailDetAltProjectileThink
---------------------------
*/

void FX_RailDetAltProjectileThink( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
	{
		forward[2] = 1.0f;
	}

	theFxScheduler.PlayEffect( "rail_detonator/shot", cent->lerpOrigin, forward );
}
