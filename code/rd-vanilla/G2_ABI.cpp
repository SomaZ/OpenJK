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

#include "tr_local.h"

mdxmHeader_t *G2ABI_GetMdxmByHandle(qhandle_t modelIndex)
{
	model_t *mod_m = R_GetModelByHandle(modelIndex);
	return mod_m->mdxm;
}

mdxaHeader_t *G2ABI_GetMdxaByHandle(qhandle_t modelIndex)
{
	model_t *mod_a = R_GetModelByHandle(modelIndex);
	return mod_a->mdxa;
}

mdxmHeader_t *G2ABI_GetMdxmByModel(const model_t *mod_m)
{
	return mod_m->mdxm;
}

mdxaHeader_t *G2ABI_GetMdxaByModel(const model_t *mod_a)
{
	return mod_a->mdxa;
}

int G2ABI_GetNumLods(const model_t *mod_m)
{
	return mod_m->numLods;
}
