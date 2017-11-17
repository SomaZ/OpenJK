/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_models.c -- model loading and caching

#include "tr_local.h"
#include "qcommon/matcomp.h"
#include "tr_cache.h"
#include <qcommon/sstring.h>

#define	LL(x) x=LittleLong(x)


static qboolean R_LoadMD3(model_t *mod, int lod, void *buffer, const char *modName);
static qboolean R_LoadMDR(model_t *mod, void *buffer, int filesize, const char *name);

/*
====================
R_RegisterMD3
====================
*/
qhandle_t R_RegisterMD3(const char *name, model_t *mod)
{
	unsigned	*buf;
	int			lod;
	int			ident;
	qboolean	loaded = qfalse;
	int			numLoaded;
	char filename[MAX_QPATH], namebuf[MAX_QPATH + 20];
	char *fext, defex[] = "md3";

	numLoaded = 0;

	strcpy(filename, name);

	fext = strchr(filename, '.');
	if (!fext)
		fext = defex;
	else
	{
		*fext = '\0';
		fext++;
	}

	for (lod = MD3_MAX_LODS - 1; lod >= 0; lod--)
	{
		if (lod)
			Com_sprintf(namebuf, sizeof(namebuf), "%s_%d.%s", filename, lod, fext);
		else
			Com_sprintf(namebuf, sizeof(namebuf), "%s.%s", filename, fext);

		qboolean bAlreadyCached = qfalse;
		if (!CModelCache->LoadFile(namebuf, (void**)&buf, &bAlreadyCached))
			continue;

		ident = *(unsigned *)buf;
		if (!bAlreadyCached)
			LL(ident);

		switch (ident)
		{
		case MD3_IDENT:
			loaded = R_LoadMD3(mod, lod, buf, namebuf);
			break;
		case MDXA_IDENT:
			loaded = R_LoadMDXA(mod, buf, namebuf, bAlreadyCached);
			break;
		case MDXM_IDENT:
			loaded = R_LoadMDXM(mod, buf, name, bAlreadyCached);
			break;
		default:
			ri.Printf(PRINT_WARNING, "R_RegisterMD3: unknown ident for %s\n", name);
			break;
		}

		if (loaded)
		{
			mod->numLods++;
			numLoaded++;
		}
		else
			break;
	}

	if (numLoaded)
	{
		// duplicate into higher lod spots that weren't
		// loaded, in case the user changes r_lodbias on the fly
		for (lod--; lod >= 0; lod--)
		{
			mod->numLods++;
			mod->data.mdv[lod] = mod->data.mdv[lod + 1];
		}

		return mod->index;
	}

#ifdef _DEBUG
	ri.Printf(PRINT_WARNING, "R_RegisterMD3: couldn't load %s\n", name);
#endif

	mod->type = MOD_BAD;
	return 0;
}

/*
====================
R_RegisterMDR
====================
*/
qhandle_t R_RegisterMDR(const char *name, model_t *mod)
{
	union {
		unsigned *u;
		void *v;
	} buf;
	int	ident;
	qboolean loaded = qfalse;
	int filesize;

	filesize = ri.FS_ReadFile(name, (void **)&buf.v);
	if (!buf.u)
	{
		mod->type = MOD_BAD;
		return 0;
	}

	ident = LittleLong(*(unsigned *)buf.u);
	if (ident == MDR_IDENT)
		loaded = R_LoadMDR(mod, buf.u, filesize, name);

	ri.FS_FreeFile(buf.v);

	if (!loaded)
	{
		ri.Printf(PRINT_WARNING, "R_RegisterMDR: couldn't load mdr file %s\n", name);
		mod->type = MOD_BAD;
		return 0;
	}

	return mod->index;
}

/*
====================
R_RegisterIQM
====================
*/
qhandle_t R_RegisterIQM(const char *name, model_t *mod)
{
	union {
		unsigned *u;
		void *v;
	} buf;
	qboolean loaded = qfalse;
	int filesize;

	filesize = ri.FS_ReadFile(name, (void **)&buf.v);
	if (!buf.u)
	{
		mod->type = MOD_BAD;
		return 0;
	}

	loaded = R_LoadIQM(mod, buf.u, filesize, name);

	ri.FS_FreeFile(buf.v);

	if (!loaded)
	{
		ri.Printf(PRINT_WARNING, "R_RegisterIQM: couldn't load iqm file %s\n", name);
		mod->type = MOD_BAD;
		return 0;
	}

	return mod->index;
}


typedef struct
{
	const char *ext;
	qhandle_t(*ModelLoader)(const char *, model_t *);
} modelExtToLoaderMap_t;

// Note that the ordering indicates the order of preference used
// when there are multiple models of different formats available
static modelExtToLoaderMap_t modelLoaders[] =
{
	{ "iqm", R_RegisterIQM },
	{ "mdr", R_RegisterMDR },
	{ "md3", R_RegisterMD3 },
	/*
	Ghoul 2 Insert Start
	*/
	{ "glm", R_RegisterMD3 },
	{ "gla", R_RegisterMD3 },
	/*
	Ghoul 2 Insert End
	*/
};

static int numModelLoaders = ARRAY_LEN(modelLoaders);

/*
** R_GetModelByHandle
*/
model_t	*R_GetModelByHandle(qhandle_t index) {
	model_t		*mod;

	// out of range gets the defualt model
	if (index < 1 || index >= tr.numModels) {
		return tr.models[0];
	}

	mod = tr.models[index];

	return mod;
}

//===============================================================================

/*
** R_AllocModel
*/
model_t *R_AllocModel(void) {
	model_t		*mod;

	if (tr.numModels == MAX_MOD_KNOWN) {
		return NULL;
	}

	mod = (model_t *)R_Hunk_Alloc(sizeof(*tr.models[tr.numModels]), qtrue);
	mod->index = tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}

static qhandle_t RE_RegisterBSP(const char *name)
{
	char bspFilePath[MAX_QPATH];
	Com_sprintf(bspFilePath, sizeof(bspFilePath), "maps/%s.bsp", name + 1);

	int bspIndex;
	world_t *world = R_LoadBSP(bspFilePath, &bspIndex);
	if (world == nullptr)
	{
		return 0;
	}

	char bspModelIdent[MAX_QPATH];
	Com_sprintf(bspModelIdent, sizeof(bspModelIdent), "*%d-0", bspIndex);

	qhandle_t modelHandle = CModelCache->GetModelHandle(bspModelIdent);
	if (modelHandle == -1)
	{
		return 0;
	}

	return modelHandle;
}

/*
====================
RE_RegisterModel

Loads in a model for the given name

Zero will be returned if the model fails to load.
An entry will be retained for failed models as an
optimization to prevent disk rescanning if they are
asked for again.
====================
*/
qhandle_t RE_RegisterModel(const char *name) {
	model_t		*mod;
	qhandle_t	hModel;
	qboolean	orgNameFailed = qfalse;
	int			orgLoader = -1;
	int			i;
	char		localName[MAX_QPATH];
	const char	*ext;
	char		altName[MAX_QPATH];

	if (!name || !name[0]) {
		ri.Printf(PRINT_ALL, "RE_RegisterModel: NULL name\n");
		return 0;
	}

	if (strlen(name) >= MAX_QPATH) {
		ri.Printf(PRINT_ALL, "Model name exceeds MAX_QPATH\n");
		return 0;
	}

	// search the currently loaded models
	if ((hModel = CModelCache->GetModelHandle(name)) != -1)
		return hModel;

	if (name[0] == '*')
	{
		if (strcmp(name, "*default.gla") != 0)
		{
			return 0;
		}
	}

	if (name[0] == '#')
	{
		return RE_RegisterBSP(name);
	}

	// allocate a new model_t
	if ((mod = R_AllocModel()) == NULL) {
		ri.Printf(PRINT_WARNING, "RE_RegisterModel: R_AllocModel() failed for '%s'\n", name);
		return 0;
	}

	// only set the name after the model has been successfully loaded
	Q_strncpyz(mod->name, name, sizeof(mod->name));

	R_IssuePendingRenderCommands();

	mod->type = MOD_BAD;
	mod->numLods = 0;

	//
	// load the files
	//
	Q_strncpyz(localName, name, MAX_QPATH);

	ext = COM_GetExtension(localName);

	if (*ext)
	{
		// Look for the correct loader and use it
		for (i = 0; i < numModelLoaders; i++)
		{
			if (!Q_stricmp(ext, modelLoaders[i].ext))
			{
				// Load
				hModel = modelLoaders[i].ModelLoader(localName, mod);
				break;
			}
		}

		// A loader was found
		if (i < numModelLoaders)
		{
			if (!hModel)
			{
				// Loader failed, most likely because the file isn't there;
				// try again without the extension
				orgNameFailed = qtrue;
				orgLoader = i;
				COM_StripExtension(name, localName, MAX_QPATH);
			}
			else
			{
				// Something loaded
				CModelCache->InsertModelHandle(name, hModel);
				return mod->index;
			}
		}
	}

	// Try and find a suitable match using all
	// the model formats supported
	for (i = 0; i < numModelLoaders; i++)
	{
		if (i == orgLoader)
			continue;

		Com_sprintf(altName, sizeof(altName), "%s.%s", localName, modelLoaders[i].ext);

		// Load
		hModel = modelLoaders[i].ModelLoader(altName, mod);

		if (hModel)
		{
			if (orgNameFailed)
			{
				ri.Printf(PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n",
					name, altName);
			}

			break;
		}
	}

	CModelCache->InsertModelHandle(name, hModel);
	return hModel;
}

//rww - Please forgive me for all of the below. Feel free to destroy it and replace it with something better.
//You obviously can't touch anything relating to shaders or ri-> functions here in case a dedicated
//server is running, which is the entire point of having these seperate functions. If anything major
//is changed in the non-server-only versions of these functions it would be wise to incorporate it
//here as well.

/*
=================
R_LoadMDXA_Server - load a Ghoul 2 animation file
=================
*/
qboolean R_LoadMDXA_Server(model_t *mod, void *buffer, const char *mod_name, qboolean &bAlreadyCached) {

	mdxaHeader_t		*pinmodel, *mdxa;
	int					version;
	int					size;

	pinmodel = (mdxaHeader_t *)buffer;
	//
	// read some fields from the binary, but only LittleLong() them when we know this wasn't an already-cached model...
	//	
	version = (pinmodel->version);
	size = (pinmodel->ofsEnd);

	if (!bAlreadyCached)
	{
		LL(version);
		LL(size);
	}

	if (version != MDXA_VERSION) {
		return qfalse;
	}

	mod->type = MOD_MDXA;
	mod->dataSize += size;

	qboolean bAlreadyFound = qfalse;
	mdxa = (mdxaHeader_t*)CModelCache->Allocate(size, buffer, mod_name, &bAlreadyFound, TAG_MODEL_GLA);
	mod->data.gla = mdxa;

	assert(bAlreadyCached == bAlreadyFound);	// I should probably eliminate 'bAlreadyFound', but wtf?

	if (!bAlreadyFound)
	{
		// horrible new hackery, if !bAlreadyFound then we've just done a tag-morph, so we need to set the 
		//	bool reference passed into this function to true, to tell the caller NOT to do an ri->FS_Freefile since
		//	we've hijacked that memory block...
		//
		// Aaaargh. Kill me now...
		//
		bAlreadyCached = qtrue;
		assert(mdxa == buffer);
		//		memcpy( mdxa, buffer, size );	// and don't do this now, since it's the same thing

		LL(mdxa->ident);
		LL(mdxa->version);
		LL(mdxa->numFrames);
		LL(mdxa->numBones);
		LL(mdxa->ofsFrames);
		LL(mdxa->ofsEnd);
	}

	if (mdxa->numFrames < 1) {
		return qfalse;
	}

	if (bAlreadyFound)
	{
		return qtrue;	// All done, stop here, do not LittleLong() etc. Do not pass go...
	}

	return qtrue;
}

/*
=================
R_LoadMD3
=================
*/
static qboolean R_LoadMD3(model_t * mod, int lod, void *buffer, const char *modName)
{
	int             f, i, j;

	md3Header_t    *md3Model;
	md3Frame_t     *md3Frame;
	md3Surface_t   *md3Surf;
	md3Shader_t    *md3Shader;
	md3Triangle_t  *md3Tri;
	md3St_t        *md3st;
	md3XyzNormal_t *md3xyz;
	md3Tag_t       *md3Tag;

	mdvModel_t     *mdvModel;
	mdvFrame_t     *frame;
	mdvSurface_t   *surf;//, *surface;
	int            *shaderIndex;
	glIndex_t	   *tri;
	mdvVertex_t    *v;
	mdvSt_t        *st;
	mdvTag_t       *tag;
	mdvTagName_t   *tagName;

	int             version;
	int             size;

	md3Model = (md3Header_t *)buffer;

	version = LittleLong(md3Model->version);
	if (version != MD3_VERSION)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has wrong version (%i should be %i)\n", modName, version, MD3_VERSION);
		return qfalse;
	}

	mod->type = MOD_MESH;
	size = LittleLong(md3Model->ofsEnd);
	mod->dataSize += size;
	//mdvModel = mod->mdv[lod] = (mdvModel_t *)ri->Hunk_Alloc(sizeof(mdvModel_t), h_low);
	qboolean bAlreadyFound = qfalse;
	mdvModel = mod->data.mdv[lod] = (mdvModel_t *)CModelCache->Allocate(size, buffer, modName, &bAlreadyFound, TAG_MODEL_MD3);

	//  Com_Memcpy(mod->md3[lod], buffer, LittleLong(md3Model->ofsEnd));
	if (!bAlreadyFound)
	{	// HACK
		LL(md3Model->ident);
		LL(md3Model->version);
		LL(md3Model->numFrames);
		LL(md3Model->numTags);
		LL(md3Model->numSurfaces);
		LL(md3Model->ofsFrames);
		LL(md3Model->ofsTags);
		LL(md3Model->ofsSurfaces);
		LL(md3Model->ofsEnd);
	}
	else
	{
		CModelCache->AllocateShaders(modName);
	}

	if (md3Model->numFrames < 1)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has no frames\n", modName);
		return qfalse;
	}

	// swap all the frames
	mdvModel->numFrames = md3Model->numFrames;
	mdvModel->frames = frame = (mdvFrame_t *)R_Hunk_Alloc(sizeof(*frame) * md3Model->numFrames, qtrue);

	md3Frame = (md3Frame_t *)((byte *)md3Model + md3Model->ofsFrames);
	for (i = 0; i < md3Model->numFrames; i++, frame++, md3Frame++)
	{
		frame->radius = LittleFloat(md3Frame->radius);
		for (j = 0; j < 3; j++)
		{
			frame->bounds[0][j] = LittleFloat(md3Frame->bounds[0][j]);
			frame->bounds[1][j] = LittleFloat(md3Frame->bounds[1][j]);
			frame->localOrigin[j] = LittleFloat(md3Frame->localOrigin[j]);
		}
	}

	// swap all the tags
	mdvModel->numTags = md3Model->numTags;
	mdvModel->tags = tag = (mdvTag_t *)R_Hunk_Alloc(sizeof(*tag) * (md3Model->numTags * md3Model->numFrames), qtrue);

	md3Tag = (md3Tag_t *)((byte *)md3Model + md3Model->ofsTags);
	for (i = 0; i < md3Model->numTags * md3Model->numFrames; i++, tag++, md3Tag++)
	{
		for (j = 0; j < 3; j++)
		{
			tag->origin[j] = LittleFloat(md3Tag->origin[j]);
			tag->axis[0][j] = LittleFloat(md3Tag->axis[0][j]);
			tag->axis[1][j] = LittleFloat(md3Tag->axis[1][j]);
			tag->axis[2][j] = LittleFloat(md3Tag->axis[2][j]);
		}
	}


	mdvModel->tagNames = tagName = (mdvTagName_t *)R_Hunk_Alloc(sizeof(*tagName) * (md3Model->numTags), qtrue);

	md3Tag = (md3Tag_t *)((byte *)md3Model + md3Model->ofsTags);
	for (i = 0; i < md3Model->numTags; i++, tagName++, md3Tag++)
	{
		Q_strncpyz(tagName->name, md3Tag->name, sizeof(tagName->name));
	}

	// swap all the surfaces
	mdvModel->numSurfaces = md3Model->numSurfaces;
	mdvModel->surfaces = surf = (mdvSurface_t *)R_Hunk_Alloc(sizeof(*surf) * md3Model->numSurfaces, qtrue);

	md3Surf = (md3Surface_t *)((byte *)md3Model + md3Model->ofsSurfaces);
	for (i = 0; i < md3Model->numSurfaces; i++)
	{
		LL(md3Surf->ident);
		LL(md3Surf->flags);
		LL(md3Surf->numFrames);
		LL(md3Surf->numShaders);
		LL(md3Surf->numTriangles);
		LL(md3Surf->ofsTriangles);
		LL(md3Surf->numVerts);
		LL(md3Surf->ofsShaders);
		LL(md3Surf->ofsSt);
		LL(md3Surf->ofsXyzNormals);
		LL(md3Surf->ofsEnd);

		if (md3Surf->numVerts >= SHADER_MAX_VERTEXES)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has more than %i verts on %s (%i).\n",
				modName, SHADER_MAX_VERTEXES - 1, md3Surf->name[0] ? md3Surf->name : "a surface",
				md3Surf->numVerts);
			return qfalse;
		}
		if (md3Surf->numTriangles * 3 >= SHADER_MAX_INDEXES)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has more than %i triangles on %s (%i).\n",
				modName, (SHADER_MAX_INDEXES / 3) - 1, md3Surf->name[0] ? md3Surf->name : "a surface",
				md3Surf->numTriangles);
			return qfalse;
		}

		// change to surface identifier
		surf->surfaceType = SF_MDV;

		// give pointer to model for Tess_SurfaceMDX
		surf->model = mdvModel;

		// copy surface name
		Q_strncpyz(surf->name, md3Surf->name, sizeof(surf->name));

		// lowercase the surface name so skin compares are faster
		Q_strlwr(surf->name);

		// strip off a trailing _1 or _2
		// this is a crutch for q3data being a mess
		j = strlen(surf->name);
		if (j > 2 && surf->name[j - 2] == '_')
		{
			surf->name[j - 2] = 0;
		}

		// register the shaders
		surf->numShaderIndexes = md3Surf->numShaders;
		surf->shaderIndexes = shaderIndex = (int *)R_Hunk_Alloc(sizeof(*shaderIndex) * md3Surf->numShaders, qtrue);

		md3Shader = (md3Shader_t *)((byte *)md3Surf + md3Surf->ofsShaders);
		for (j = 0; j < md3Surf->numShaders; j++, shaderIndex++, md3Shader++)
		{
			shader_t       *sh;

			sh = R_FindShader(md3Shader->name, lightmapsNone, stylesDefault, qtrue);
			if (sh->defaultShader)
			{
				*shaderIndex = 0;
			}
			else
			{
				*shaderIndex = sh->index;
			}
		}

		// swap all the triangles
		surf->numIndexes = md3Surf->numTriangles * 3;
		surf->indexes = tri = (glIndex_t *)R_Hunk_Alloc(sizeof(*tri) * 3 * md3Surf->numTriangles, qtrue);

		md3Tri = (md3Triangle_t *)((byte *)md3Surf + md3Surf->ofsTriangles);
		for (j = 0; j < md3Surf->numTriangles; j++, tri += 3, md3Tri++)
		{
			tri[0] = LittleLong(md3Tri->indexes[0]);
			tri[1] = LittleLong(md3Tri->indexes[1]);
			tri[2] = LittleLong(md3Tri->indexes[2]);
		}

		// swap all the XyzNormals
		surf->numVerts = md3Surf->numVerts;
		surf->verts = v = (mdvVertex_t *)R_Hunk_Alloc(sizeof(*v) * (md3Surf->numVerts * md3Surf->numFrames), qtrue);

		md3xyz = (md3XyzNormal_t *)((byte *)md3Surf + md3Surf->ofsXyzNormals);
		for (j = 0; j < md3Surf->numVerts * md3Surf->numFrames; j++, md3xyz++, v++)
		{
			unsigned lat, lng;
			unsigned short normal;

			v->xyz[0] = LittleShort(md3xyz->xyz[0]) * MD3_XYZ_SCALE;
			v->xyz[1] = LittleShort(md3xyz->xyz[1]) * MD3_XYZ_SCALE;
			v->xyz[2] = LittleShort(md3xyz->xyz[2]) * MD3_XYZ_SCALE;

			normal = LittleShort(md3xyz->normal);

			lat = (normal >> 8) & 0xff;
			lng = (normal & 0xff);
			lat *= (FUNCTABLE_SIZE / 256);
			lng *= (FUNCTABLE_SIZE / 256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			v->normal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			v->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			v->normal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4))&FUNCTABLE_MASK];
		}

		// swap all the ST
		surf->st = st = (mdvSt_t *)R_Hunk_Alloc(sizeof(*st) * md3Surf->numVerts, qtrue);

		md3st = (md3St_t *)((byte *)md3Surf + md3Surf->ofsSt);
		for (j = 0; j < md3Surf->numVerts; j++, md3st++, st++)
		{
			st->st[0] = LittleFloat(md3st->st[0]);
			st->st[1] = LittleFloat(md3st->st[1]);
		}

		// calc tangent spaces
		{
			for (j = 0, v = surf->verts; j < (surf->numVerts * mdvModel->numFrames); j++, v++)
			{
				VectorClear(v->tangent);
				VectorClear(v->bitangent);
			}

			for (f = 0; f < mdvModel->numFrames; f++)
			{
				for (j = 0, tri = surf->indexes; j < surf->numIndexes; j += 3, tri += 3)
				{
					vec3_t sdir, tdir;
					const float *v0, *v1, *v2, *t0, *t1, *t2;
					glIndex_t index0, index1, index2;

					index0 = surf->numVerts * f + tri[0];
					index1 = surf->numVerts * f + tri[1];
					index2 = surf->numVerts * f + tri[2];

					v0 = surf->verts[index0].xyz;
					v1 = surf->verts[index1].xyz;
					v2 = surf->verts[index2].xyz;

					t0 = surf->st[tri[0]].st;
					t1 = surf->st[tri[1]].st;
					t2 = surf->st[tri[2]].st;

					R_CalcTexDirs(sdir, tdir, v0, v1, v2, t0, t1, t2);

					VectorAdd(sdir, surf->verts[index0].tangent, surf->verts[index0].tangent);
					VectorAdd(sdir, surf->verts[index1].tangent, surf->verts[index1].tangent);
					VectorAdd(sdir, surf->verts[index2].tangent, surf->verts[index2].tangent);
					VectorAdd(tdir, surf->verts[index0].bitangent, surf->verts[index0].bitangent);
					VectorAdd(tdir, surf->verts[index1].bitangent, surf->verts[index1].bitangent);
					VectorAdd(tdir, surf->verts[index2].bitangent, surf->verts[index2].bitangent);
				}
			}

			for (j = 0, v = surf->verts; j < (surf->numVerts * mdvModel->numFrames); j++, v++)
			{
				vec3_t sdir, tdir;

				VectorCopy(v->tangent, sdir);
				VectorCopy(v->bitangent, tdir);

				VectorNormalize(sdir);
				VectorNormalize(tdir);

				R_CalcTbnFromNormalAndTexDirs(v->tangent, v->bitangent, v->normal, sdir, tdir);
			}
		}

		// find the next surface
		md3Surf = (md3Surface_t *)((byte *)md3Surf + md3Surf->ofsEnd);
		surf++;
	}

	{
		srfVBOMDVMesh_t *vboSurf;

		mdvModel->numVBOSurfaces = mdvModel->numSurfaces;
		mdvModel->vboSurfaces = (srfVBOMDVMesh_t *)R_Hunk_Alloc(sizeof(*mdvModel->vboSurfaces) * mdvModel->numSurfaces, qtrue);

		vboSurf = mdvModel->vboSurfaces;
		surf = mdvModel->surfaces;
		for (i = 0; i < mdvModel->numSurfaces; i++, vboSurf++, surf++)
		{
			vec3_t *verts;
			vec2_t *texcoords;
			uint32_t *normals;
			uint32_t *tangents;

			byte *data;
			int dataSize;

			int ofs_xyz, ofs_normal, ofs_st;
			int ofs_tangent;

			dataSize = 0;

			ofs_xyz = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*verts);

			ofs_normal = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*normals);

			ofs_tangent = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*tangents);

			ofs_st = dataSize;
			dataSize += surf->numVerts * sizeof(*texcoords);

			data = (byte *)R_Malloc(dataSize, TAG_MODEL_MD3, qfalse);

			verts = (vec3_t *)(data + ofs_xyz);
			normals = (uint32_t *)(data + ofs_normal);
			tangents = (uint32_t *)(data + ofs_tangent);
			texcoords = (vec2_t *)(data + ofs_st);

			v = surf->verts;
			for (j = 0; j < surf->numVerts * mdvModel->numFrames; j++, v++)
			{
				vec3_t nxt;
				vec4_t tangent;

				VectorCopy(v->xyz, verts[j]);

				normals[j] = R_VboPackNormal(v->normal);
				CrossProduct(v->normal, v->tangent, nxt);
				VectorCopy(v->tangent, tangent);
				tangent[3] = (DotProduct(nxt, v->bitangent) < 0.0f) ? -1.0f : 1.0f;

				tangents[j] = R_VboPackTangent(tangent);
			}

			st = surf->st;
			for (j = 0; j < surf->numVerts; j++, st++) {
				texcoords[j][0] = st->st[0];
				texcoords[j][1] = st->st[1];
			}

			vboSurf->surfaceType = SF_VBO_MDVMESH;
			vboSurf->mdvModel = mdvModel;
			vboSurf->mdvSurface = surf;
			vboSurf->numIndexes = surf->numIndexes;
			vboSurf->numVerts = surf->numVerts;

			vboSurf->minIndex = 0;
			vboSurf->maxIndex = surf->numVerts;

			vboSurf->vbo = R_CreateVBO(data, dataSize, VBO_USAGE_STATIC);

			vboSurf->vbo->offsets[ATTR_INDEX_POSITION] = ofs_xyz;
			vboSurf->vbo->offsets[ATTR_INDEX_NORMAL] = ofs_normal;
			vboSurf->vbo->offsets[ATTR_INDEX_TEXCOORD0] = ofs_st;
			vboSurf->vbo->offsets[ATTR_INDEX_TANGENT] = ofs_tangent;

			vboSurf->vbo->strides[ATTR_INDEX_POSITION] = sizeof(*verts);
			vboSurf->vbo->strides[ATTR_INDEX_NORMAL] = sizeof(*normals);
			vboSurf->vbo->strides[ATTR_INDEX_TEXCOORD0] = sizeof(*st);
			vboSurf->vbo->strides[ATTR_INDEX_TANGENT] = sizeof(*tangents);

			vboSurf->vbo->sizes[ATTR_INDEX_POSITION] = sizeof(*verts);
			vboSurf->vbo->sizes[ATTR_INDEX_NORMAL] = sizeof(*normals);
			vboSurf->vbo->sizes[ATTR_INDEX_TEXCOORD0] = sizeof(*texcoords);
			vboSurf->vbo->sizes[ATTR_INDEX_TANGENT] = sizeof(*tangents);

			R_Free(data);

			vboSurf->ibo = R_CreateIBO((byte *)surf->indexes, sizeof(glIndex_t) * surf->numIndexes, VBO_USAGE_STATIC);
		}
	}

	return qtrue;
}



/*
=================
R_LoadMDR
=================
*/
static qboolean R_LoadMDR(model_t *mod, void *buffer, int filesize, const char *mod_name)
{
	int					i, j, k, l;
	mdrHeader_t			*pinmodel, *mdr;
	mdrFrame_t			*frame;
	mdrLOD_t			*lod, *curlod;
	mdrSurface_t			*surf, *cursurf;
	mdrTriangle_t			*tri, *curtri;
	mdrVertex_t			*v, *curv;
	mdrWeight_t			*weight, *curweight;
	mdrTag_t			*tag, *curtag;
	int					size;
	shader_t			*sh;

	pinmodel = (mdrHeader_t *)buffer;

	pinmodel->version = LittleLong(pinmodel->version);
	if (pinmodel->version != MDR_VERSION)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has wrong version (%i should be %i)\n", mod_name, pinmodel->version, MDR_VERSION);
		return qfalse;
	}

	size = LittleLong(pinmodel->ofsEnd);

	if (size > filesize)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: Header of %s is broken. Wrong filesize declared!\n", mod_name);
		return qfalse;
	}

	mod->type = MOD_MDR;

	LL(pinmodel->numFrames);
	LL(pinmodel->numBones);
	LL(pinmodel->ofsFrames);

	// This is a model that uses some type of compressed Bones. We don't want to uncompress every bone for each rendered frame
	// over and over again, we'll uncompress it in this function already, so we must adjust the size of the target mdr.
	if (pinmodel->ofsFrames < 0)
	{
		// mdrFrame_t is larger than mdrCompFrame_t:
		size += pinmodel->numFrames * sizeof(frame->name);
		// now add enough space for the uncompressed bones.
		size += pinmodel->numFrames * pinmodel->numBones * ((sizeof(mdrBone_t) - sizeof(mdrCompBone_t)));
	}

	// simple bounds check
	if (pinmodel->numBones < 0 ||
		sizeof(*mdr) + pinmodel->numFrames * (sizeof(*frame) + (pinmodel->numBones - 1) * sizeof(*frame->bones)) > size)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
		return qfalse;
	}

	mod->dataSize += size;
	mod->data.mdr = mdr = (mdrHeader_t*)R_Hunk_Alloc(size, qtrue);

	// Copy all the values over from the file and fix endian issues in the process, if necessary.

	mdr->ident = LittleLong(pinmodel->ident);
	mdr->version = pinmodel->version;	// Don't need to swap byte order on this one, we already did above.
	Q_strncpyz(mdr->name, pinmodel->name, sizeof(mdr->name));
	mdr->numFrames = pinmodel->numFrames;
	mdr->numBones = pinmodel->numBones;
	mdr->numLODs = LittleLong(pinmodel->numLODs);
	mdr->numTags = LittleLong(pinmodel->numTags);
	// We don't care about the other offset values, we'll generate them ourselves while loading.

	mod->numLods = mdr->numLODs;

	if (mdr->numFrames < 1)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has no frames\n", mod_name);
		return qfalse;
	}

	/* The first frame will be put into the first free space after the header */
	frame = (mdrFrame_t *)(mdr + 1);
	mdr->ofsFrames = (int)((byte *)frame - (byte *)mdr);

	if (pinmodel->ofsFrames < 0)
	{
		mdrCompFrame_t *cframe;

		// compressed model...				
		cframe = (mdrCompFrame_t *)((byte *)pinmodel - pinmodel->ofsFrames);

		for (i = 0; i < mdr->numFrames; i++)
		{
			for (j = 0; j < 3; j++)
			{
				frame->bounds[0][j] = LittleFloat(cframe->bounds[0][j]);
				frame->bounds[1][j] = LittleFloat(cframe->bounds[1][j]);
				frame->localOrigin[j] = LittleFloat(cframe->localOrigin[j]);
			}

			frame->radius = LittleFloat(cframe->radius);
			frame->name[0] = '\0';	// No name supplied in the compressed version.

			for (j = 0; j < mdr->numBones; j++)
			{
				for (k = 0; k < (sizeof(cframe->bones[j].Comp) / 2); k++)
				{
					// Do swapping for the uncompressing functions. They seem to use shorts
					// values only, so I assume this will work. Never tested it on other
					// platforms, though.

					((unsigned short *)(cframe->bones[j].Comp))[k] =
						LittleShort(((unsigned short *)(cframe->bones[j].Comp))[k]);
				}

				/* Now do the actual uncompressing */
				MC_UnCompress(frame->bones[j].matrix, cframe->bones[j].Comp);
			}

			// Next Frame...
			cframe = (mdrCompFrame_t *)&cframe->bones[j];
			frame = (mdrFrame_t *)&frame->bones[j];
		}
	}
	else
	{
		mdrFrame_t *curframe;

		// uncompressed model...
		//

		curframe = (mdrFrame_t *)((byte *)pinmodel + pinmodel->ofsFrames);

		// swap all the frames
		for (i = 0; i < mdr->numFrames; i++)
		{
			for (j = 0; j < 3; j++)
			{
				frame->bounds[0][j] = LittleFloat(curframe->bounds[0][j]);
				frame->bounds[1][j] = LittleFloat(curframe->bounds[1][j]);
				frame->localOrigin[j] = LittleFloat(curframe->localOrigin[j]);
			}

			frame->radius = LittleFloat(curframe->radius);
			Q_strncpyz(frame->name, curframe->name, sizeof(frame->name));

			for (j = 0; j < (int)(mdr->numBones * sizeof(mdrBone_t) / 4); j++)
			{
				((float *)frame->bones)[j] = LittleFloat(((float *)curframe->bones)[j]);
			}

			curframe = (mdrFrame_t *)&curframe->bones[mdr->numBones];
			frame = (mdrFrame_t *)&frame->bones[mdr->numBones];
		}
	}

	// frame should now point to the first free address after all frames.
	lod = (mdrLOD_t *)frame;
	mdr->ofsLODs = (int)((byte *)lod - (byte *)mdr);

	curlod = (mdrLOD_t *)((byte *)pinmodel + LittleLong(pinmodel->ofsLODs));

	// swap all the LOD's
	for (l = 0; l < mdr->numLODs; l++)
	{
		// simple bounds check
		if ((byte *)(lod + 1) >(byte *) mdr + size)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
			return qfalse;
		}

		lod->numSurfaces = LittleLong(curlod->numSurfaces);

		// swap all the surfaces
		surf = (mdrSurface_t *)(lod + 1);
		lod->ofsSurfaces = (int)((byte *)surf - (byte *)lod);
		cursurf = (mdrSurface_t *)((byte *)curlod + LittleLong(curlod->ofsSurfaces));

		for (i = 0; i < lod->numSurfaces; i++)
		{
			// simple bounds check
			if ((byte *)(surf + 1) >(byte *) mdr + size)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
				return qfalse;
			}

			// first do some copying stuff

			surf->ident = SF_MDR;
			Q_strncpyz(surf->name, cursurf->name, sizeof(surf->name));
			Q_strncpyz(surf->shader, cursurf->shader, sizeof(surf->shader));

			surf->ofsHeader = (byte *)mdr - (byte *)surf;

			surf->numVerts = LittleLong(cursurf->numVerts);
			surf->numTriangles = LittleLong(cursurf->numTriangles);
			// numBoneReferences and BoneReferences generally seem to be unused

			// now do the checks that may fail.
			if (surf->numVerts >= SHADER_MAX_VERTEXES)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has more than %i verts on %s (%i).\n",
					mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
					surf->numVerts);
				return qfalse;
			}
			if (surf->numTriangles * 3 >= SHADER_MAX_INDEXES)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has more than %i triangles on %s (%i).\n",
					mod_name, (SHADER_MAX_INDEXES / 3) - 1, surf->name[0] ? surf->name : "a surface",
					surf->numTriangles);
				return qfalse;
			}
			// lowercase the surface name so skin compares are faster
			Q_strlwr(surf->name);

			// register the shaders
			sh = R_FindShader(surf->shader, lightmapsNone, stylesDefault, qtrue);
			if (sh->defaultShader) {
				surf->shaderIndex = 0;
			}
			else {
				surf->shaderIndex = sh->index;
			}

			// now copy the vertexes.
			v = (mdrVertex_t *)(surf + 1);
			surf->ofsVerts = (int)((byte *)v - (byte *)surf);
			curv = (mdrVertex_t *)((byte *)cursurf + LittleLong(cursurf->ofsVerts));

			for (j = 0; j < surf->numVerts; j++)
			{
				LL(curv->numWeights);

				// simple bounds check
				if (curv->numWeights < 0 || (byte *)(v + 1) + (curv->numWeights - 1) * sizeof(*weight) >(byte *) mdr + size)
				{
					ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
					return qfalse;
				}

				v->normal[0] = LittleFloat(curv->normal[0]);
				v->normal[1] = LittleFloat(curv->normal[1]);
				v->normal[2] = LittleFloat(curv->normal[2]);

				v->texCoords[0] = LittleFloat(curv->texCoords[0]);
				v->texCoords[1] = LittleFloat(curv->texCoords[1]);

				v->numWeights = curv->numWeights;
				weight = &v->weights[0];
				curweight = &curv->weights[0];

				// Now copy all the weights
				for (k = 0; k < v->numWeights; k++)
				{
					weight->boneIndex = LittleLong(curweight->boneIndex);
					weight->boneWeight = LittleFloat(curweight->boneWeight);

					weight->offset[0] = LittleFloat(curweight->offset[0]);
					weight->offset[1] = LittleFloat(curweight->offset[1]);
					weight->offset[2] = LittleFloat(curweight->offset[2]);

					weight++;
					curweight++;
				}

				v = (mdrVertex_t *)weight;
				curv = (mdrVertex_t *)curweight;
			}

			// we know the offset to the triangles now:
			tri = (mdrTriangle_t *)v;
			surf->ofsTriangles = (int)((byte *)tri - (byte *)surf);
			curtri = (mdrTriangle_t *)((byte *)cursurf + LittleLong(cursurf->ofsTriangles));

			// simple bounds check
			if (surf->numTriangles < 0 || (byte *)(tri + surf->numTriangles) >(byte *) mdr + size)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
				return qfalse;
			}

			for (j = 0; j < surf->numTriangles; j++)
			{
				tri->indexes[0] = LittleLong(curtri->indexes[0]);
				tri->indexes[1] = LittleLong(curtri->indexes[1]);
				tri->indexes[2] = LittleLong(curtri->indexes[2]);

				tri++;
				curtri++;
			}

			// tri now points to the end of the surface.
			surf->ofsEnd = (byte *)tri - (byte *)surf;
			surf = (mdrSurface_t *)tri;

			// find the next surface.
			cursurf = (mdrSurface_t *)((byte *)cursurf + LittleLong(cursurf->ofsEnd));
		}

		// surf points to the next lod now.
		lod->ofsEnd = (int)((byte *)surf - (byte *)lod);
		lod = (mdrLOD_t *)surf;

		// find the next LOD.
		curlod = (mdrLOD_t *)((byte *)curlod + LittleLong(curlod->ofsEnd));
	}

	// lod points to the first tag now, so update the offset too.
	tag = (mdrTag_t *)lod;
	mdr->ofsTags = (int)((byte *)tag - (byte *)mdr);
	curtag = (mdrTag_t *)((byte *)pinmodel + LittleLong(pinmodel->ofsTags));

	// simple bounds check
	if (mdr->numTags < 0 || (byte *)(tag + mdr->numTags) >(byte *) mdr + size)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
		return qfalse;
	}

	for (i = 0; i < mdr->numTags; i++)
	{
		tag->boneIndex = LittleLong(curtag->boneIndex);
		Q_strncpyz(tag->name, curtag->name, sizeof(tag->name));

		tag++;
		curtag++;
	}

	// And finally we know the real offset to the end.
	mdr->ofsEnd = (int)((byte *)tag - (byte *)mdr);

	// phew! we're done.

	return qtrue;
}



//=============================================================================

/*
** RE_BeginRegistration
*/
void RE_BeginRegistration(glconfig_t *glconfigOut) {

	R_Init();

	*glconfigOut = glConfig;

	R_IssuePendingRenderCommands();

	tr.visIndex = 0;
	memset(tr.visClusters, -2, sizeof(tr.visClusters));	// force markleafs to regenerate

	R_ClearFlares();
	RE_ClearScene();

	tr.registered = qtrue;

	// NOTE: this sucks, for some reason the first stretch pic is never drawn
	// without this we'd see a white flash on a level load because the very
	// first time the level shot would not be drawn
	//	RE_StretchPic(0, 0, 0, 0, 0, 0, 1, 1, 0);
}

//=============================================================================

/*
===============
R_ModelInit
===============
*/
void R_ModelInit(void) {
	model_t		*mod;

	// leave a space for NULL model
	tr.numModels = 0;

	CModelCache->DeleteAll();

	mod = R_AllocModel();
	mod->type = MOD_BAD;
}


/*
================
R_Modellist_f
================
*/
void R_Modellist_f(void) {
	int		i, j;
	model_t	*mod;
	int		total;
	int		lods;

	total = 0;
	for (i = 1; i < tr.numModels; i++) {
		mod = tr.models[i];
		lods = 1;
		for (j = 1; j < MD3_MAX_LODS; j++) {
			if (mod->data.mdv[j] && mod->data.mdv[j] != mod->data.mdv[j - 1]) {
				lods++;
			}
		}
		ri.Printf(PRINT_ALL, "%8i : (%i) %s\n", mod->dataSize, lods, mod->name);
		total += mod->dataSize;
	}
	ri.Printf(PRINT_ALL, "%8i : Total models\n", total);

#if	0		// not working right with new hunk
	if (tr.world) {
		ri.Printf(PRINT_ALL, "\n%8i : %s\n", tr.world->dataSize, tr.world->name);
	}
#endif
}


//=============================================================================


/*
================
R_GetTag
================
*/
static mdvTag_t *R_GetTag(mdvModel_t *mod, int frame, const char *_tagName) {
	int             i;
	mdvTag_t       *tag;
	mdvTagName_t   *tagName;

	if (frame >= mod->numFrames) {
		// it is possible to have a bad frame while changing models, so don't error
		frame = mod->numFrames - 1;
	}

	tag = mod->tags + frame * mod->numTags;
	tagName = mod->tagNames;
	for (i = 0; i < mod->numTags; i++, tag++, tagName++)
	{
		if (!strcmp(tagName->name, _tagName))
		{
			return tag;
		}
	}

	return NULL;
}

void R_GetAnimTag(mdrHeader_t *mod, int framenum, const char *tagName, mdvTag_t * dest)
{
	int				i, j, k;
	int				frameSize;
	mdrFrame_t		*frame;
	mdrTag_t		*tag;

	if (framenum >= mod->numFrames)
	{
		// it is possible to have a bad frame while changing models, so don't error
		framenum = mod->numFrames - 1;
	}

	tag = (mdrTag_t *)((byte *)mod + mod->ofsTags);
	for (i = 0; i < mod->numTags; i++, tag++)
	{
		if (!strcmp(tag->name, tagName))
		{
			// uncompressed model...
			//
			frameSize = (intptr_t)(&((mdrFrame_t *)0)->bones[mod->numBones]);
			frame = (mdrFrame_t *)((byte *)mod + mod->ofsFrames + framenum * frameSize);

			for (j = 0; j < 3; j++)
			{
				for (k = 0; k < 3; k++)
					dest->axis[j][k] = frame->bones[tag->boneIndex].matrix[k][j];
			}

			dest->origin[0] = frame->bones[tag->boneIndex].matrix[0][3];
			dest->origin[1] = frame->bones[tag->boneIndex].matrix[1][3];
			dest->origin[2] = frame->bones[tag->boneIndex].matrix[2][3];

			return;
		}
	}

	AxisClear(dest->axis);
	VectorClear(dest->origin);
}

/*
================
R_LerpTag
================
*/
void R_LerpTag(orientation_t *tag, qhandle_t handle, int startFrame, int endFrame,
	float frac, const char *tagName) {

	mdvTag_t	*start, *end;
	mdvTag_t	start_space, end_space;
	int		i;
	float		frontLerp, backLerp;
	model_t		*model;

	model = R_GetModelByHandle(handle);
	if (!model->data.mdv[0])
	{
		if (model->type == MOD_MDR)
		{
			start = &start_space;
			end = &end_space;
			R_GetAnimTag((mdrHeader_t *)model->data.mdr, startFrame, tagName, start);
			R_GetAnimTag((mdrHeader_t *)model->data.mdr, endFrame, tagName, end);
		}
		else if (model->type == MOD_IQM) 
		{
			R_IQMLerpTag(tag, (iqmData_t *)model->data.iqm,	startFrame, endFrame, frac, tagName);
		}
		else 
		{

			AxisClear(tag->axis);
			VectorClear(tag->origin);
			return;

		}
	}
	else
	{
		start = R_GetTag(model->data.mdv[0], startFrame, tagName);
		end = R_GetTag(model->data.mdv[0], endFrame, tagName);
		if (!start || !end) {
			AxisClear(tag->axis);
			VectorClear(tag->origin);
			return;
		}
	}

	frontLerp = frac;
	backLerp = 1.0f - frac;

	for (i = 0; i < 3; i++) {
		tag->origin[i] = start->origin[i] * backLerp + end->origin[i] * frontLerp;
		tag->axis[0][i] = start->axis[0][i] * backLerp + end->axis[0][i] * frontLerp;
		tag->axis[1][i] = start->axis[1][i] * backLerp + end->axis[1][i] * frontLerp;
		tag->axis[2][i] = start->axis[2][i] * backLerp + end->axis[2][i] * frontLerp;
	}
	VectorNormalize(tag->axis[0]);
	VectorNormalize(tag->axis[1]);
	VectorNormalize(tag->axis[2]);
}


/*
====================
R_ModelBounds
====================
*/
void R_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs) {
	model_t		*model;

	model = R_GetModelByHandle(handle);

	if (model->type == MOD_BRUSH) {
		VectorCopy(model->data.bmodel->bounds[0], mins);
		VectorCopy(model->data.bmodel->bounds[1], maxs);

		return;
	}
	else if (model->type == MOD_MESH) {
		mdvModel_t	*header;
		mdvFrame_t	*frame;

		header = model->data.mdv[0];
		frame = header->frames;

		VectorCopy(frame->bounds[0], mins);
		VectorCopy(frame->bounds[1], maxs);

		return;
	}
	else if (model->type == MOD_MDR) {
		mdrHeader_t	*header;
		mdrFrame_t	*frame;

		header = model->data.mdr;
		frame = (mdrFrame_t *)((byte *)header + header->ofsFrames);

		VectorCopy(frame->bounds[0], mins);
		VectorCopy(frame->bounds[1], maxs);

		return;
	}
	else if (model->type == MOD_IQM) {
		iqmData_t *iqmData;

		iqmData = model->data.iqm;

		if (iqmData->bounds)
		{
			VectorCopy(iqmData->bounds, mins);
			VectorCopy(iqmData->bounds + 3, maxs);
			return;
		}
	}

	VectorClear(mins);
	VectorClear(maxs);
}

/*
=================
R_ComputeLOD

=================
*/
int R_ComputeLOD(trRefEntity_t *ent) {
	float radius;
	float flod, lodscale;
	float projectedRadius;
	mdvFrame_t *frame;
	mdrHeader_t *mdr;
	mdrFrame_t *mdrframe;
	int lod;

	if (tr.currentModel->numLods < 2)
	{
		// model has only 1 LOD level, skip computations and bias
		lod = 0;
	}
	else
	{
		// multiple LODs exist, so compute projected bounding sphere
		// and use that as a criteria for selecting LOD

		if (tr.currentModel->type == MOD_MDR)
		{
			int frameSize;
			mdr = tr.currentModel->data.mdr;
			frameSize = (size_t)(&((mdrFrame_t *)0)->bones[mdr->numBones]);

			mdrframe = (mdrFrame_t *)((byte *)mdr + mdr->ofsFrames + frameSize * ent->e.frame);

			radius = RadiusFromBounds(mdrframe->bounds[0], mdrframe->bounds[1]);
		}
		else
		{
			//frame = ( md3Frame_t * ) ( ( ( unsigned char * ) tr.currentModel->md3[0] ) + tr.currentModel->md3[0]->ofsFrames );
			frame = tr.currentModel->data.mdv[0]->frames;

			frame += ent->e.frame;

			radius = RadiusFromBounds(frame->bounds[0], frame->bounds[1]);
		}

		if ((projectedRadius = ProjectRadius(radius, ent->e.origin)) != 0)
		{
			lodscale = (r_lodscale->value + r_autolodscalevalue->integer);
			if (lodscale > 20) lodscale = 20;
			flod = 1.0f - projectedRadius * lodscale;
		}
		else
		{
			// object intersects near view plane, e.g. view weapon
			flod = 0;
		}

		flod *= tr.currentModel->numLods;
		lod = Q_ftol(flod);

		if (lod < 0)
		{
			lod = 0;
		}
		else if (lod >= tr.currentModel->numLods)
		{
			lod = tr.currentModel->numLods - 1;
		}
	}

	lod += r_lodbias->integer;

	if (lod >= tr.currentModel->numLods)
		lod = tr.currentModel->numLods - 1;
	if (lod < 0)
		lod = 0;

	return lod;
}
