#include "client/client.h"	//FIXME!! EVIL - just include the definitions needed
#include "tr_local.h"
#include "qcommon/matcomp.h"
#include "qcommon/qcommon.h"
#include "ghoul2/G2.h"
#ifndef REND2_SP
#include "ghoul2/g2_local.h"
#endif // !REND2_SP
#ifdef _G2_GORE
#include "G2_gore_r2.h"
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4512)	//default assignment operator could not be gened
#endif
#ifndef REND2_SP
#include "qcommon/disablewarnings.h"
#endif // !REND2_SP
#include "tr_cache.h"

#define	LL(x) x=LittleLong(x)

#ifdef G2_PERFORMANCE_ANALYSIS
#include "qcommon/timing.h"

timing_c G2PerformanceTimer_RenderSurfaces;
timing_c G2PerformanceTimer_R_AddGHOULSurfaces;
timing_c G2PerformanceTimer_G2_TransformGhoulBones;
timing_c G2PerformanceTimer_G2_ProcessGeneratedSurfaceBolts;
timing_c G2PerformanceTimer_ProcessModelBoltSurfaces;
timing_c G2PerformanceTimer_G2_ConstructGhoulSkeleton;
timing_c G2PerformanceTimer_RB_SurfaceGhoul;
timing_c G2PerformanceTimer_G2_SetupModelPointers;
timing_c G2PerformanceTimer_PreciseFrame;

int G2PerformanceCounter_G2_TransformGhoulBones = 0;

int G2Time_RenderSurfaces = 0;
int G2Time_R_AddGHOULSurfaces = 0;
int G2Time_G2_TransformGhoulBones = 0;
int G2Time_G2_ProcessGeneratedSurfaceBolts = 0;
int G2Time_ProcessModelBoltSurfaces = 0;
int G2Time_G2_ConstructGhoulSkeleton = 0;
int G2Time_RB_SurfaceGhoul = 0;
int G2Time_G2_SetupModelPointers = 0;
int G2Time_PreciseFrame = 0;

void G2Time_ResetTimers(void)
{
	G2Time_RenderSurfaces = 0;
	G2Time_R_AddGHOULSurfaces = 0;
	G2Time_G2_TransformGhoulBones = 0;
	G2Time_G2_ProcessGeneratedSurfaceBolts = 0;
	G2Time_ProcessModelBoltSurfaces = 0;
	G2Time_G2_ConstructGhoulSkeleton = 0;
	G2Time_RB_SurfaceGhoul = 0;
	G2Time_G2_SetupModelPointers = 0;
	G2Time_PreciseFrame = 0;
	G2PerformanceCounter_G2_TransformGhoulBones = 0;
}

void G2Time_ReportTimers(void)
{
	Com_Printf("\n---------------------------------\nRenderSurfaces: %i\nR_AddGhoulSurfaces: %i\nG2_TransformGhoulBones: %i\nG2_ProcessGeneratedSurfaceBolts: %i\nProcessModelBoltSurfaces: %i\nG2_ConstructGhoulSkeleton: %i\nRB_SurfaceGhoul: %i\nG2_SetupModelPointers: %i\n\nPrecise frame time: %i\nTransformGhoulBones calls: %i\n---------------------------------\n\n",
		G2Time_RenderSurfaces,
		G2Time_R_AddGHOULSurfaces,
		G2Time_G2_TransformGhoulBones,
		G2Time_G2_ProcessGeneratedSurfaceBolts,
		G2Time_ProcessModelBoltSurfaces,
		G2Time_G2_ConstructGhoulSkeleton,
		G2Time_RB_SurfaceGhoul,
		G2Time_G2_SetupModelPointers,
		G2Time_PreciseFrame,
		G2PerformanceCounter_G2_TransformGhoulBones
	);
}
#endif

//rww - RAGDOLL_BEGIN
#ifdef __linux__
#include <math.h>
#else
#include <float.h>
#endif

//rww - RAGDOLL_END

#ifdef REND2_SP
extern cvar_t* sv_mapname;
#endif

static const int MAX_RENDERABLE_SURFACES = 4096;
static CRenderableSurface renderSurfHeap[MAX_RENDERABLE_SURFACES];
static int currentRenderSurfIndex = 0;

static CRenderableSurface *AllocGhoul2RenderableSurface()
{
	if ( currentRenderSurfIndex >= MAX_RENDERABLE_SURFACES - 1)
	{
		ResetGhoul2RenderableSurfaceHeap();
		ri.Printf( PRINT_DEVELOPER, "AllocRenderableSurface: Reached maximum number of Ghoul2 renderable surfaces (%d)\n", MAX_RENDERABLE_SURFACES );
	}

	CRenderableSurface *rs = &renderSurfHeap[currentRenderSurfIndex++];

	rs->Init();

	return rs;
}

void ResetGhoul2RenderableSurfaceHeap()
{
	currentRenderSurfIndex = 0;
}

#ifdef _G2_GORE
qhandle_t		goreShader=-1;
#endif

class CRenderSurface
{
public:
	int				surfaceNum;
	surfaceInfo_v	&rootSList;
	shader_t		*cust_shader;
	int				fogNum;
	qboolean		personalModel;
	CBoneCache		*boneCache;
	int				renderfx;
	skin_t			*skin;
	model_t			*currentModel;
	int				lod;
	boltInfo_v		&boltList;
#ifdef _G2_GORE
	shader_t		*gore_shader;
	int				*goreSetTag;
#endif

	CRenderSurface(
		int				initsurfaceNum,
		surfaceInfo_v	&initrootSList,
		shader_t		*initcust_shader,
		int				initfogNum,
		qboolean		initpersonalModel,
		CBoneCache		*initboneCache,
		int				initrenderfx,
		skin_t			*initskin,
		model_t			*initcurrentModel,
		int				initlod,
#ifdef _G2_GORE
		boltInfo_v		&initboltList,
		shader_t		*initgore_shader,
		int				*initgoreSetTag
#else
		boltInfo_v		&initboltList
#endif
		)
		: surfaceNum(initsurfaceNum)
		, rootSList(initrootSList)
		, cust_shader(initcust_shader)
		, fogNum(initfogNum)
		, personalModel(initpersonalModel)
		, boneCache(initboneCache)
		, renderfx(initrenderfx)
		, skin(initskin)
		, currentModel(initcurrentModel)
		, lod(initlod)
		, boltList(initboltList)
#ifdef _G2_GORE
		, gore_shader(initgore_shader)
		, goreSetTag(initgoreSetTag)
#endif
	{
	}
};

/*

All bones should be an identity orientation to display the mesh exactly
as it is specified.

For all other frames, the bones represent the transformation from the
orientation of the bone in the base frame to the orientation in this
frame.

*/


/*
=============
R_ACullModel
=============
*/
static int R_GCullModel( trRefEntity_t *ent ) {

	// scale the radius if need be
	float largestScale = ent->e.modelScale[0];

	if (ent->e.modelScale[1] > largestScale)
	{
		largestScale = ent->e.modelScale[1];
	}
	if (ent->e.modelScale[2] > largestScale)
	{
		largestScale = ent->e.modelScale[2];
	}
	if (!largestScale)
	{
		largestScale = 1;
	}

	// cull bounding sphere
  	switch ( R_CullLocalPointAndRadius( vec3_origin,  ent->e.radius * largestScale) )
  	{
  	case CULL_OUT:
  		tr.pc.c_sphere_cull_md3_out++;
  		return CULL_OUT;

	case CULL_IN:
		tr.pc.c_sphere_cull_md3_in++;
		return CULL_IN;

	case CULL_CLIP:
		tr.pc.c_sphere_cull_md3_clip++;
		return CULL_IN;
 	}
	return CULL_IN;
}


/*
=================
R_AComputeFogNum

=================
*/
static int R_GComputeFogNum( trRefEntity_t *ent ) {

	int				i;
	float			frameRadius;
	fog_t			*fog;
	vec3_t			localOrigin;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}

	VectorCopy(ent->e.origin, localOrigin);
	frameRadius = ent->e.radius;
#ifndef REND2_SP
	int j;
	for ( i = 1 ; i < tr.world->numfogs ; i++ ) {
		fog = &tr.world->fogs[i];
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( localOrigin[j] - frameRadius >= fog->bounds[1][j] ) {
				break;
			}
			if ( localOrigin[j] + frameRadius <= fog->bounds[0][j] ) {
				break;
			}
		}
		if ( j == 3 ) {
			return i;
		}
	}
	return 0;
#else
	int partialFog = 0;
	for (i = 1; i < tr.world->numfogs; i++) {
		fog = &tr.world->fogs[i];
		if (localOrigin[0] - frameRadius >= fog->bounds[0][0]
			&& localOrigin[0] + frameRadius <= fog->bounds[1][0]
			&& localOrigin[1] - frameRadius >= fog->bounds[0][1]
			&& localOrigin[1] + frameRadius <= fog->bounds[1][1]
			&& localOrigin[2] - frameRadius >= fog->bounds[0][2]
			&& localOrigin[2] + frameRadius <= fog->bounds[1][2])
		{//totally inside it
			return i;
			break;
		}
		if ((localOrigin[0] - frameRadius >= fog->bounds[0][0] && localOrigin[1] - frameRadius >= fog->bounds[0][1] && localOrigin[2] - frameRadius >= fog->bounds[0][2] &&
			localOrigin[0] - frameRadius <= fog->bounds[1][0] && localOrigin[1] - frameRadius <= fog->bounds[1][1] && localOrigin[2] - frameRadius <= fog->bounds[1][2]) ||
			(localOrigin[0] + frameRadius >= fog->bounds[0][0] && localOrigin[1] + frameRadius >= fog->bounds[0][1] && localOrigin[2] + frameRadius >= fog->bounds[0][2] &&
				localOrigin[0] + frameRadius <= fog->bounds[1][0] && localOrigin[1] + frameRadius <= fog->bounds[1][1] && localOrigin[2] + frameRadius <= fog->bounds[1][2]))
		{//partially inside it
			//if (tr.refdef.fogIndex == i || R_FogParmsMatch(tr.refdef.fogIndex, i))
			//{//take new one only if it's the same one that the viewpoint is in
			//	return i;
			//	break;
			//}
			//else 
			if (!partialFog)
			{//first partialFog
				partialFog = i;
			}
		}
	}
	return partialFog;
#endif
}

// work out lod for this entity.
static int G2_ComputeLOD( trRefEntity_t *ent, const model_t *currentModel, int lodBias )
{
	float flod, lodscale;
	float projectedRadius;
	int lod;

	if ( currentModel->numLods < 2 )
	{	// model has only 1 LOD level, skip computations and bias
		return(0);
	}

	if ( r_lodbias->integer > lodBias )
	{
		lodBias = r_lodbias->integer;
	}

	// scale the radius if need be
	float largestScale = ent->e.modelScale[0];

	if (ent->e.modelScale[1] > largestScale)
	{
		largestScale = ent->e.modelScale[1];
	}
	if (ent->e.modelScale[2] > largestScale)
	{
		largestScale = ent->e.modelScale[2];
	}
	if (!largestScale)
	{
		largestScale = 1;
	}

	projectedRadius = ProjectRadius( 0.75*largestScale*ent->e.radius, ent->e.origin );

	// we reduce the radius to make the LOD match other model types which use
	// the actual bound box size
	if ( projectedRadius != 0 )
 	{
 		lodscale = (r_lodscale->value+r_autolodscalevalue->value);
 		if ( lodscale > 20 )
		{
			lodscale = 20;
		}
		else if ( lodscale < 0 )
		{
			lodscale = 0;
		}
 		flod = 1.0f - projectedRadius * lodscale;
 	}
 	else
 	{
 		// object intersects near view plane, e.g. view weapon
 		flod = 0;
 	}
 	flod *= currentModel->numLods;
	lod = Q_ftol( flod );

 	if ( lod < 0 )
 	{
 		lod = 0;
 	}
 	else if ( lod >= currentModel->numLods )
 	{
 		lod = currentModel->numLods - 1;
 	}


	lod += lodBias;

	if ( lod >= currentModel->numLods )
		lod = currentModel->numLods - 1;
	if ( lod < 0 )
		lod = 0;

	return lod;
}


//======================================================================
//
// Surface Manipulation code

void G2API_SetSurfaceOnOffFromSkin(CGhoul2Info* ghlInfo, qhandle_t renderSkin)
{
	int j;
	const skin_t* skin = R_GetSkinByHandle(renderSkin);
	//FIXME:  using skin handles means we have to increase the numsurfs in a skin, but reading directly would cause file hits, we need another way to cache or just deal with the larger skin_t

	if (skin)
	{
		ghlInfo->mSlist.clear();	//remove any overrides we had before.
		ghlInfo->mMeshFrameNum = 0;
		for (j = 0; j < skin->numSurfaces; j++)
		{
			uint32_t flags;
			int surfaceNum = ri.G2_IsSurfaceLegal(ghlInfo->currentModel, skin->surfaces[j]->name, &flags);
			// the names have both been lowercased
			if (!(flags & G2SURFACEFLAG_OFF) && !strcmp(skin->surfaces[j]->shader->name, "*off"))
			{
				ri.G2_SetSurfaceOnOff(ghlInfo, skin->surfaces[j]->name, G2SURFACEFLAG_OFF);
			}
			else
			{
				//if ( strcmp( &skin->surfaces[j]->name[strlen(skin->surfaces[j]->name)-4],"_off") )
				if ((surfaceNum != -1) && (!(flags & G2SURFACEFLAG_OFF)))	//only turn on if it's not an "_off" surface
				{
					//ri.G2_SetSurfaceOnOff(ghlInfo, skin->surfaces[j]->name, 0);
				}
			}
		}
	}
}

void RenderSurfaces( CRenderSurface &RS, const trRefEntity_t *ent, int entityNum )
{
#ifdef G2_PERFORMANCE_ANALYSIS
	G2PerformanceTimer_RenderSurfaces.Start();
#endif

	int i;
	const shader_t *shader = 0;
	int offFlags = 0;
#ifdef _G2_GORE
	bool drawGore = true;
#endif

	assert(RS.currentModel);
	assert(RS.currentModel->data.glm && RS.currentModel->data.glm->header);
	// back track and get the surfinfo struct for this surface
	mdxmSurface_t *surface =
		(mdxmSurface_t *)ri.G2_FindSurface(RS.currentModel, RS.surfaceNum, RS.lod);
	mdxmHierarchyOffsets_t *surfIndexes = (mdxmHierarchyOffsets_t *)
		((byte *)RS.currentModel->data.glm->header + sizeof(mdxmHeader_t));
	mdxmSurfHierarchy_t *surfInfo = (mdxmSurfHierarchy_t *)
		((byte *)surfIndexes + surfIndexes->offsets[surface->thisSurfaceIndex]);

	// see if we have an override surface in the surface list
	const surfaceInfo_t	*surfOverride = ri.G2_FindOverrideSurface(RS.surfaceNum, RS.rootSList);

	// really, we should use the default flags for this surface unless it's been overriden
	offFlags = surfInfo->flags;

	// set the off flags if we have some
	if (surfOverride)
	{
		offFlags = surfOverride->offFlags;
	}

	// if this surface is not off, add it to the shader render list
	if (!offFlags)
	{
 		if ( RS.cust_shader )
		{
			shader = RS.cust_shader;
		}
		else if ( RS.skin )
		{
			int		j;

			// match the surface name to something in the skin file
			shader = R_GetShaderByHandle(surfInfo->shaderIndex);
			for ( j = 0 ; j < RS.skin->numSurfaces ; j++ )
			{
				// the names have both been lowercased
				if ( !strcmp( RS.skin->surfaces[j]->name, surfInfo->name ) )
				{
					shader = (shader_t*)RS.skin->surfaces[j]->shader;
					break;
				}
			}
		}
		else
		{
			shader = R_GetShaderByHandle( surfInfo->shaderIndex );
		}

		// Get dlightBits and Cubemap
		float radius;
		// scale the radius if needed
		float largestScale = MAX(ent->e.modelScale[0], MAX(ent->e.modelScale[1], ent->e.modelScale[2]));
		radius = ent->e.radius * largestScale;
		int dlightBits = R_DLightsForPoint(ent->e.origin, radius);
		int cubemapIndex = R_CubemapForPoint(ent->e.origin);

		// don't add third_person objects if not viewing through a portal
		if ( !RS.personalModel )
		{
			// set the surface info to point at the where the transformed bone
			// list is going to be for when the surface gets rendered out
			CRenderableSurface *newSurf = AllocGhoul2RenderableSurface();
			newSurf->vboMesh = &RS.currentModel->data.glm->vboModels[RS.lod].vboMeshes[RS.surfaceNum];
			assert (newSurf->vboMesh != NULL && RS.surfaceNum == surface->thisSurfaceIndex);
			newSurf->surfaceData = surface;
			newSurf->boneCache = RS.boneCache;
			newSurf->dlightBits = dlightBits;

			// render shadows?
			if (r_shadows->integer == 2
				&& (RS.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
				&& shader->sort == SS_OPAQUE)
				newSurf->genShadows = qtrue;

			R_AddDrawSurf(
				(surfaceType_t *)newSurf,
				entityNum,
				(shader_t *)shader,
				RS.fogNum,
				qfalse,
				R_IsPostRenderEntity(ent),
				cubemapIndex);

#ifdef _G2_GORE
			if ( *RS.goreSetTag && drawGore )
			{
				int curTime = ri.G2API_GetTime(tr.refdef.time);

				CGoreSet* gore = ri.FindGoreSet(*RS.goreSetTag);
				if (!gore) // my gore is gone, so remove it
				{
					*RS.goreSetTag = 0;
				}

				auto range = gore->mGoreRecords.equal_range(RS.surfaceNum);
				CRenderableSurface *last = newSurf;
				for ( auto k = range.first; k != range.second; /* blank */ )
				{
					auto kcur = k;
					k++;

					R2GoreTextureCoordinates* tex = nullptr; //FindR2GoreRecord(kcur->second.mGoreTag);
					if (!tex ||	// it is gone, lets get rid of it
						(kcur->second.mDeleteTime &&
						 curTime >= kcur->second.mDeleteTime)) // out of time
					{
						gore->mGoreRecords.erase(kcur);
					}
					else if (tex->tex[RS.lod])
					{
						CRenderableSurface *newSurf2 = AllocGhoul2RenderableSurface();
						*newSurf2 = *newSurf;
						newSurf2->goreChain = 0;
						newSurf2->alternateTex = tex->tex[RS.lod];
						newSurf2->scale = 1.0f;
						newSurf2->fade = 1.0f;
						newSurf2->impactTime = 1.0f;  // done with
						int magicFactor42 = 500; // ms, impact time
						if (curTime > kcur->second.mGoreGrowStartTime &&
							curTime < (kcur->second.mGoreGrowStartTime + magicFactor42) )
						{
							newSurf2->impactTime =
								float(curTime - kcur->second.mGoreGrowStartTime) /
								float(magicFactor42);  // linear
						}
#ifdef REND2_SP_GORE
						if (curTime < kcur->second.mGoreGrowEndTime)
						{
							newSurf2->scale = Q_max(
								1.0f,
								1.0f /
									((curTime - kcur->second.mGoreGrowStartTime) *
									kcur->second.mGoreGrowFactor +
									kcur->second.mGoreGrowOffset));
						}
#endif
						shader_t *gshader;
						if (kcur->second.shader)
						{
 							gshader = R_GetShaderByHandle(kcur->second.shader);
						}
						else
						{
							gshader = R_GetShaderByHandle(goreShader);
						}
#ifdef REND2_SP_GORE
						// Set fade on surf.
						// Only if we have a fade time set, and let us fade on
						// rgb if we want -rww
						if (kcur->second.mDeleteTime && kcur->second.mFadeTime)
						{
							if ( (kcur->second.mDeleteTime - curTime) < kcur->second.mFadeTime )
							{
								newSurf2->fade =
									(float)(kcur->second.mDeleteTime - curTime) /
									kcur->second.mFadeTime;
								if (kcur->second.mFadeRGB)
								{
									// RGB fades are scaled from 2.0f to 3.0f
									// (simply to differentiate)
									newSurf2->fade = Q_max(2.01f, newSurf2->fade + 2.0f);
								}
							}
						}
#endif
						last->goreChain = newSurf2;
						last = newSurf2;
						R_AddDrawSurf(
							(surfaceType_t *)newSurf2,
							entityNum,
							gshader,
							RS.fogNum,
							qfalse,
							R_IsPostRenderEntity(ent),
							cubemapIndex);
					}
				}
			}
#endif
		}

		// projection shadows work fine with personal models
		if (r_shadows->integer == 3
			&& RS.fogNum == 0
			&& (RS.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
			&& shader->sort == SS_OPAQUE) {

			CRenderableSurface *newSurf = AllocGhoul2RenderableSurface();
			newSurf->vboMesh = &RS.currentModel->data.glm->vboModels[RS.lod].vboMeshes[RS.surfaceNum];
			assert(newSurf->vboMesh != NULL && RS.surfaceNum == surface->thisSurfaceIndex);
			newSurf->surfaceData = surface;
			newSurf->boneCache = RS.boneCache;
			R_AddDrawSurf((surfaceType_t *)newSurf, entityNum, tr.projectionShadowShader, 0, qfalse, qfalse, 0);
		}
	}

	// if we are turning off all descendants, then stop this recursion now
	if (offFlags & G2SURFACEFLAG_NODESCENDANTS)
	{
		return;
	}

	// now recursively call for the children
	for (i=0; i< surfInfo->numChildren; i++)
	{
		RS.surfaceNum = surfInfo->childIndexes[i];
		RenderSurfaces(RS, ent, entityNum);
	}

#ifdef G2_PERFORMANCE_ANALYSIS
	G2Time_RenderSurfaces += G2PerformanceTimer_RenderSurfaces.End();
#endif
}

typedef struct extraData_s
{
	mat3x4_t boneMatrices[MAX_G2_BONES];
	int      uboOffset;
	int		 uboPreviousOffset;
	int	     uboGPUFrame;
} extraData_t;

/*
==============
R_AddGHOULSurfaces
==============
*/

void R_AddGhoulSurfaces( trRefEntity_t *ent, int entityNum )
{
#ifdef G2_PERFORMANCE_ANALYSIS
	G2PerformanceTimer_R_AddGHOULSurfaces.Start();
#endif

	CGhoul2Info_v &ghoul2 = *((CGhoul2Info_v *)ent->e.ghoul2);

	if ( !ghoul2.IsValid() )
	{
		return;
	}

	// if we don't want server ghoul2 models and this is one, or we just don't
	// want ghoul2 models at all, then return
	if (r_noServerGhoul2->integer)
	{
		return;
	}

	if (!G2_SetupModelPointers(ghoul2))
	{
		return;
	}

	int currentTime = ri.G2API_GetTime(tr.refdef.time);

	// cull the entire model if merged bounding box of both frames is outside
	// the view frustum.
	int cull = R_GCullModel(ent);
	if ( cull == CULL_OUT )
	{
		return;
	}

	int modelList[32];
	int	modelCount;
	ri.G2_TransformGhoulSkeleton(
		ghoul2,
		currentTime,
		ent->e.modelScale,
		modelList,
		&modelCount
	);

   	// don't add third_person objects if not in a portal
	qboolean personalModel = (qboolean)(
		(ent->e.renderfx & RF_THIRD_PERSON) &&
		!(tr.viewParms.isPortal ||
			(tr.viewParms.flags & VPF_DEPTHSHADOW)));

	// see if we are in a fog volume
	int fogNum = R_GComputeFogNum(ent);


#ifdef _G2_GORE
	if ( goreShader == -1 )
	{
		goreShader = RE_RegisterShader("gfx/damage/burnmark1");
	}
#endif

	// walk each possible model for this entity and try rendering it out
	for (int j = 0; j < modelCount; ++j )
	{
		CGhoul2Info& g2Info = ghoul2[modelList[j]];

		if ( !g2Info.mValid )
		{
			continue;
		}

		if ( (g2Info.mFlags & (GHOUL2_NOMODEL | GHOUL2_NORENDER)) != 0 )
		{
			continue;
		}

		// figure out whether we should be using a custom shader for this model
		skin_t *skin = nullptr;
		shader_t *cust_shader = nullptr;

		if (ent->e.customShader)
		{
			cust_shader = R_GetShaderByHandle(ent->e.customShader );
		}
		else
		{
			cust_shader = nullptr;
#ifndef REND2_SP
			// figure out the custom skin thing
			if (g2Info.mCustomSkin)
			{
				skin = R_GetSkinByHandle(g2Info.mCustomSkin);
			}
			else
#endif
				if (ent->e.customSkin)
				{
					skin = R_GetSkinByHandle(ent->e.customSkin);
				}
				else if (g2Info.mSkin > 0 && g2Info.mSkin < tr.numSkins)
				{
					skin = R_GetSkinByHandle(g2Info.mSkin);
				}
		}

		int whichLod = G2_ComputeLOD( ent, g2Info.currentModel, g2Info.mLodBias );

#ifdef _G2_GORE
		CRenderSurface RS(g2Info.mSurfaceRoot,
			g2Info.mSlist,
			cust_shader,
			fogNum,
			personalModel,
			g2Info.mBoneCache,
			ent->e.renderfx,
			skin,
			(model_t *)g2Info.currentModel,
			whichLod,
			g2Info.mBltlist,
			nullptr,
			&g2Info.mGoreSetTag);
#else
		CRenderSurface RS(g2Info.mSurfaceRoot,
			g2Info.mSlist,
			cust_shader,
			fogNum,
			personalModel,
			g2Info.mBoneCache,
			ent->e.renderfx,
			skin,
			(model_t *)g2Info.currentModel,
			whichLod,
			g2Info.mBltlist);
#endif
		if ( !personalModel && (RS.renderfx & RF_SHADOW_PLANE) )
		{
			RS.renderfx |= RF_NOSHADOW;
		}
		RenderSurfaces(RS, ent, entityNum);
	}

#ifdef G2_PERFORMANCE_ANALYSIS
	G2Time_R_AddGHOULSurfaces += G2PerformanceTimer_R_AddGHOULSurfaces.End();
#endif
}

#ifdef _G2_LISTEN_SERVER_OPT
qboolean G2API_OverrideServerWithClientData(CGhoul2Info *serverInstance);
#endif

void RB_TransformBones(const trRefEntity_t* ent, const trRefdef_t* refdef, int currentFrameNum, gpuFrame_t* frame)
{
	CGhoul2Info_v& ghoul2 = *((CGhoul2Info_v*)ent->e.ghoul2);

	if (!ghoul2.IsValid())
	{
		return;
	}

	// if we don't want server ghoul2 models and this is one, or we just don't
	// want ghoul2 models at all, then return
	if (r_noServerGhoul2->integer)
	{
		return;
	}

	if (!G2_SetupModelPointers(ghoul2))
	{
		return;
	}

	int currentTime = ri.G2API_GetTime(tr.refdef.time);

	int modelList[32];
	int	modelCount;
	ri.G2_TransformGhoulSkeleton(
		ghoul2,
		currentTime,
		ent->e.modelScale,
		modelList,
		&modelCount
	);

	// walk each possible model for this entity and try transforming all bones
	for (int j = 0; j < modelCount; ++j)
	{
		CGhoul2Info& g2Info = ghoul2[modelList[j]];

		if (!g2Info.mValid)
		{
			continue;
		}

		if ((g2Info.mFlags & (GHOUL2_NOMODEL | GHOUL2_NORENDER)) != 0)
		{
			continue;
		}

		CBoneCache* bc = g2Info.mBoneCache;
		extraData_t* extra = (extraData_t*)ri.G2_GetBCExtraData(bc);
		int numBones = ri.G2_GetBCNumBones(bc);
		if (!extra)
		{
			extra = (extraData_t*)ri.Z_Malloc(sizeof(*extra), TAG_GHOUL2, qfalse, 4);
			extra->uboGPUFrame = -1;
			ri.G2_SetBCExtraData(bc, extra);
		}
		if (extra->uboGPUFrame == currentFrameNum)
			return;

		for (int bone = 0; bone < numBones; bone++)
		{
			const mdxaBone_t& b = ri.EvalBoneCache(bone, bc);
			Com_Memcpy(
				extra->boneMatrices + bone,
				&b.matrix[0][0],
				sizeof(mat3x4_t));
		}
		extra->uboOffset = -1;

		SkeletonBoneMatricesBlock bonesBlock = {};
		Com_Memcpy(
			bonesBlock.matrices,
			extra->boneMatrices,
			sizeof(mat3x4_t) * numBones);

		int uboOffset = RB_AppendConstantsData(
			frame, &bonesBlock, sizeof(mat3x4_t) * numBones);

		extra->uboOffset = uboOffset;
		extra->uboGPUFrame = currentFrameNum;

		if (!backEndData->cachePreviousFrameUbos)
		{
			extra->uboPreviousOffset = -1;
			continue;
		}

		if (frame->numCachedGhoulUboOffsets == MAX_GENTITIES)
		{
			ri.Printf(PRINT_DEVELOPER, "Too many ghoul2 models to cache, skipping now.\n");
		}
		else
		{
			frame->cachedGhoulUboOffsets[frame->numCachedGhoulUboOffsets].ghoulPointer = ent->e.ghoul2;
			frame->cachedGhoulUboOffsets[frame->numCachedGhoulUboOffsets].model = g2Info.mModel;
			frame->cachedGhoulUboOffsets[frame->numCachedGhoulUboOffsets].boltIndex = g2Info.mModelBoltLink;
			frame->cachedGhoulUboOffsets[frame->numCachedGhoulUboOffsets].boneUboOffset = uboOffset;
			frame->numCachedGhoulUboOffsets++;
		}

		int foundCache = -1;
		for (int c = 0; c < backEndData->previousFrame->numCachedGhoulUboOffsets; c++)
		{
			ghoul2UboCache_t* currentCache = &backEndData->previousFrame->cachedGhoulUboOffsets[c];
			if (currentCache->ghoulPointer != ent->e.ghoul2)
				continue;
			if (currentCache->model != g2Info.mModel)
				continue;
			if (currentCache->boltIndex != g2Info.mModelBoltLink && foundCache == -1)
			{
				foundCache = c;
				continue;
			}
			foundCache = c;
			break;
		}

		if (foundCache == -1)
			extra->uboPreviousOffset = 0;
		else
			extra->uboPreviousOffset = backEndData->previousFrame->cachedGhoulUboOffsets[foundCache].boneUboOffset;

	}
}

static inline float G2_GetVertBoneWeightNotSlow( const mdxmVertex_t *pVert, const int iWeightNum)
{
	float fBoneWeight;

	int iTemp = pVert->BoneWeightings[iWeightNum];

	iTemp|= (pVert->uiNmWeightsAndBoneIndexes >> (iG2_BONEWEIGHT_TOPBITS_SHIFT+(iWeightNum*2)) ) & iG2_BONEWEIGHT_TOPBITS_AND;

	fBoneWeight = fG2_BONEWEIGHT_RECIPROCAL_MULT * iTemp;

	return fBoneWeight;
}



int RB_GetBoneUboOffset(CRenderableSurface *surf)
{
	if (surf->boneCache)
	{
		extraData_t* extra = (extraData_t *)ri.G2_GetBCExtraData(surf->boneCache);
		return extra->uboOffset;
	}
	else
		return -1;
}

int RB_GetPreviousBoneUboOffset(CRenderableSurface *surf)
{
	if (surf->boneCache)
	{
		extraData_t* extra = (extraData_t*)ri.G2_GetBCExtraData(surf->boneCache);
		return extra->uboPreviousOffset;
	}
	else
		return -1;
}

void RB_SetBoneUboOffset(CRenderableSurface *surf, int offset, int currentFrameNum)
{
	extraData_t* extra = (extraData_t*)ri.G2_GetBCExtraData(surf->boneCache);
	extra->uboOffset = offset;
	extra->uboGPUFrame = currentFrameNum;
}

void RB_FillBoneBlock(CRenderableSurface *surf, mat3x4_t *outMatrices)
{
	extraData_t* extra = (extraData_t*)ri.G2_GetBCExtraData(surf->boneCache);
	Com_Memcpy(
		outMatrices,
		extra->boneMatrices,
		sizeof(extra->boneMatrices));
}

void RB_SurfaceGhoul( CRenderableSurface *surf )
{
	mdxmVBOMesh_t *surface = surf->vboMesh;

	if ( surface->vbo == NULL || surface->ibo == NULL )
	{
		return;
	}

	uint32_t numIndexes = surface->numIndexes;
	uint32_t numVertexes = surface->numVertexes;
	uint32_t minIndex = surface->minIndex;
	uint32_t maxIndex = surface->maxIndex;
	uint32_t indexOffset = surface->indexOffset;

#ifdef _G2_GORE
	if (surf->alternateTex)
	{
		if (!surf->alternateTex->cachedInFrame[backEndData->realFrameNumber % MAX_FRAMES])
		{
			RB_UpdateGoreVertexData(backEndData->currentFrame, surf->alternateTex, false);
		}
		else
		{
			R_BindVBO(backEndData->currentFrame->goreVBO);
			R_BindIBO(backEndData->currentFrame->goreIBO);
		}
		tess.externalIBO = backEndData->currentFrame->goreIBO;

		numIndexes = surf->alternateTex->numIndexes;
		numVertexes = surf->alternateTex->numVerts;
		minIndex = surf->alternateTex->firstVert;
		maxIndex = surf->alternateTex->firstVert + surf->alternateTex->numVerts;
		indexOffset = surf->alternateTex->firstIndex;

#ifdef REND2_SP_GORE
		// UNTESTED CODE
		if (surf->scale > 1.0f)
		{
			tess.scale = true;
			tess.texCoords[tess.firstIndex][0][0] = surf->scale;
		}

		//now check for fade overrides -rww
		if (surf->fade)
		{
			static int lFade;
			if (surf->fade < 1.0)
			{
				tess.fade = true;
				lFade = Q_ftol(254.4f*surf->fade);
				tess.svars.colors[tess.firstIndex][0] =
				tess.svars.colors[tess.firstIndex][1] =
				tess.svars.colors[tess.firstIndex][2] = Q_ftol(1.0f);
				tess.svars.colors[tess.firstIndex][3] = lFade;
			}
			else if (surf->fade > 2.0f && surf->fade < 3.0f)
			{ //hack to fade out on RGB if desired (don't want to add more to CRenderableSurface) -rww
				tess.fade = true;
				lFade = Q_ftol(254.4f*(surf->fade - 2.0f));
				if (lFade < tess.svars.colors[tess.firstIndex][0])
				{ //don't set it unless the fade is less than the current r value (to avoid brightening suddenly before we start fading)
					tess.svars.colors[tess.firstIndex][0] =
					tess.svars.colors[tess.firstIndex][1] =
					tess.svars.colors[tess.firstIndex][2] = lFade;
				}
				tess.svars.colors[tess.firstIndex][3] = lFade;
			}
		}
#endif
	} else {
#endif

		R_BindVBO(surface->vbo);
		R_BindIBO(surface->ibo);
		tess.externalIBO = surface->ibo;

		glState.genShadows = surf->genShadows;
#ifdef _G2_GORE
	}
#endif
	int i, mergeForward, mergeBack;
	GLvoid *firstIndexOffset, *lastIndexOffset;

	// merge this into any existing multidraw primitives
	mergeForward = -1;
	mergeBack = -1;
	firstIndexOffset = BUFFER_OFFSET(indexOffset * sizeof(glIndex_t));
	lastIndexOffset = BUFFER_OFFSET((indexOffset + numIndexes) * sizeof(glIndex_t));

	if (r_mergeMultidraws->integer)
	{
		i = 0;

		if (r_mergeMultidraws->integer == 1)
		{
			// lazy merge, only check the last primitive
			if (tess.multiDrawPrimitives)
			{
				i = tess.multiDrawPrimitives - 1;
			}
		}

		for (; i < tess.multiDrawPrimitives; i++)
		{
			if (tess.multiDrawLastIndex[i] == firstIndexOffset)
			{
				mergeBack = i;
			}

			if (lastIndexOffset == tess.multiDrawFirstIndex[i])
			{
				mergeForward = i;
			}
		}
	}

	if (mergeBack != -1 && mergeForward == -1)
	{
		tess.multiDrawNumIndexes[mergeBack] += numIndexes;
		tess.multiDrawLastIndex[mergeBack] = tess.multiDrawFirstIndex[mergeBack] + tess.multiDrawNumIndexes[mergeBack];
		tess.multiDrawMinIndex[mergeBack] = MIN(tess.multiDrawMinIndex[mergeBack], minIndex);
		tess.multiDrawMaxIndex[mergeBack] = MAX(tess.multiDrawMaxIndex[mergeBack], maxIndex);
		backEnd.pc.c_multidrawsMerged++;
	}
	else if (mergeBack == -1 && mergeForward != -1)
	{
		tess.multiDrawNumIndexes[mergeForward] += numIndexes;
		tess.multiDrawFirstIndex[mergeForward] = (glIndex_t *)firstIndexOffset;
		tess.multiDrawLastIndex[mergeForward] = tess.multiDrawFirstIndex[mergeForward] + tess.multiDrawNumIndexes[mergeForward];
		tess.multiDrawMinIndex[mergeForward] = MIN(tess.multiDrawMinIndex[mergeForward], minIndex);
		tess.multiDrawMaxIndex[mergeForward] = MAX(tess.multiDrawMaxIndex[mergeForward], maxIndex);
		backEnd.pc.c_multidrawsMerged++;
	}
	else if (mergeBack != -1 && mergeForward != -1)
	{
		tess.multiDrawNumIndexes[mergeBack] += numIndexes + tess.multiDrawNumIndexes[mergeForward];
		tess.multiDrawLastIndex[mergeBack] = tess.multiDrawFirstIndex[mergeBack] + tess.multiDrawNumIndexes[mergeBack];
		tess.multiDrawMinIndex[mergeBack] = MIN(tess.multiDrawMinIndex[mergeBack], MIN(tess.multiDrawMinIndex[mergeForward], minIndex));
		tess.multiDrawMaxIndex[mergeBack] = MAX(tess.multiDrawMaxIndex[mergeBack], MAX(tess.multiDrawMaxIndex[mergeForward], maxIndex));
		tess.multiDrawPrimitives--;

		if (mergeForward != tess.multiDrawPrimitives)
		{
			tess.multiDrawNumIndexes[mergeForward] = tess.multiDrawNumIndexes[tess.multiDrawPrimitives];
			tess.multiDrawFirstIndex[mergeForward] = tess.multiDrawFirstIndex[tess.multiDrawPrimitives];
		}
		backEnd.pc.c_multidrawsMerged += 2;
	}
	else if (mergeBack == -1 && mergeForward == -1)
	{
		tess.multiDrawNumIndexes[tess.multiDrawPrimitives] = numIndexes;
		tess.multiDrawFirstIndex[tess.multiDrawPrimitives] = (glIndex_t *)firstIndexOffset;
		tess.multiDrawLastIndex[tess.multiDrawPrimitives] = (glIndex_t *)lastIndexOffset;
		tess.multiDrawMinIndex[tess.multiDrawPrimitives] = minIndex;
		tess.multiDrawMaxIndex[tess.multiDrawPrimitives] = maxIndex;
		tess.multiDrawPrimitives++;
	}

	backEnd.pc.c_multidraws++;

	tess.numIndexes += numIndexes;
	tess.numVertexes += numVertexes;
	tess.useInternalVBO = qfalse;
	tess.dlightBits |= surf->dlightBits;

	glState.skeletalAnimation = qtrue;
}

/*
=================
R_LoadMDXM - load a Ghoul 2 Mesh file
=================
*/

/*

Some information used in the creation of the JK2 - JKA bone remap table

These are the old bones:
Complete list of all 72 bones:

*/

int OldToNewRemapTable[72] = {
0,// Bone 0:   "model_root":           Parent: ""  (index -1)
1,// Bone 1:   "pelvis":               Parent: "model_root"  (index 0)
2,// Bone 2:   "Motion":               Parent: "pelvis"  (index 1)
3,// Bone 3:   "lfemurYZ":             Parent: "pelvis"  (index 1)
4,// Bone 4:   "lfemurX":              Parent: "pelvis"  (index 1)
5,// Bone 5:   "ltibia":               Parent: "pelvis"  (index 1)
6,// Bone 6:   "ltalus":               Parent: "pelvis"  (index 1)
6,// Bone 7:   "ltarsal":              Parent: "pelvis"  (index 1)
7,// Bone 8:   "rfemurYZ":             Parent: "pelvis"  (index 1)
8,// Bone 9:   "rfemurX":	            Parent: "pelvis"  (index 1)
9,// Bone10:   "rtibia":	            Parent: "pelvis"  (index 1)
10,// Bone11:   "rtalus":	            Parent: "pelvis"  (index 1)
10,// Bone12:   "rtarsal":              Parent: "pelvis"  (index 1)
11,// Bone13:   "lower_lumbar":         Parent: "pelvis"  (index 1)
12,// Bone14:   "upper_lumbar":	        Parent: "lower_lumbar"  (index 13)
13,// Bone15:   "thoracic":	            Parent: "upper_lumbar"  (index 14)
14,// Bone16:   "cervical":	            Parent: "thoracic"  (index 15)
15,// Bone17:   "cranium":              Parent: "cervical"  (index 16)
16,// Bone18:   "ceyebrow":	            Parent: "face_always_"  (index 71)
17,// Bone19:   "jaw":                  Parent: "face_always_"  (index 71)
18,// Bone20:   "lblip2":	            Parent: "face_always_"  (index 71)
19,// Bone21:   "leye":		            Parent: "face_always_"  (index 71)
20,// Bone22:   "rblip2":	            Parent: "face_always_"  (index 71)
21,// Bone23:   "ltlip2":               Parent: "face_always_"  (index 71)
22,// Bone24:   "rtlip2":	            Parent: "face_always_"  (index 71)
23,// Bone25:   "reye":		            Parent: "face_always_"  (index 71)
24,// Bone26:   "rclavical":            Parent: "thoracic"  (index 15)
25,// Bone27:   "rhumerus":             Parent: "thoracic"  (index 15)
26,// Bone28:   "rhumerusX":            Parent: "thoracic"  (index 15)
27,// Bone29:   "rradius":              Parent: "thoracic"  (index 15)
28,// Bone30:   "rradiusX":             Parent: "thoracic"  (index 15)
29,// Bone31:   "rhand":                Parent: "thoracic"  (index 15)
29,// Bone32:   "mc7":                  Parent: "thoracic"  (index 15)
34,// Bone33:   "r_d5_j1":              Parent: "thoracic"  (index 15)
35,// Bone34:   "r_d5_j2":              Parent: "thoracic"  (index 15)
35,// Bone35:   "r_d5_j3":              Parent: "thoracic"  (index 15)
30,// Bone36:   "r_d1_j1":              Parent: "thoracic"  (index 15)
31,// Bone37:   "r_d1_j2":              Parent: "thoracic"  (index 15)
31,// Bone38:   "r_d1_j3":              Parent: "thoracic"  (index 15)
32,// Bone39:   "r_d2_j1":              Parent: "thoracic"  (index 15)
33,// Bone40:   "r_d2_j2":              Parent: "thoracic"  (index 15)
33,// Bone41:   "r_d2_j3":              Parent: "thoracic"  (index 15)
32,// Bone42:   "r_d3_j1":			    Parent: "thoracic"  (index 15)
33,// Bone43:   "r_d3_j2":		        Parent: "thoracic"  (index 15)
33,// Bone44:   "r_d3_j3":              Parent: "thoracic"  (index 15)
34,// Bone45:   "r_d4_j1":              Parent: "thoracic"  (index 15)
35,// Bone46:   "r_d4_j2":	            Parent: "thoracic"  (index 15)
35,// Bone47:   "r_d4_j3":		        Parent: "thoracic"  (index 15)
36,// Bone48:   "rhang_tag_bone":	    Parent: "thoracic"  (index 15)
37,// Bone49:   "lclavical":            Parent: "thoracic"  (index 15)
38,// Bone50:   "lhumerus":	            Parent: "thoracic"  (index 15)
39,// Bone51:   "lhumerusX":	        Parent: "thoracic"  (index 15)
40,// Bone52:   "lradius":	            Parent: "thoracic"  (index 15)
41,// Bone53:   "lradiusX":	            Parent: "thoracic"  (index 15)
42,// Bone54:   "lhand":	            Parent: "thoracic"  (index 15)
42,// Bone55:   "mc5":		            Parent: "thoracic"  (index 15)
43,// Bone56:   "l_d5_j1":	            Parent: "thoracic"  (index 15)
44,// Bone57:   "l_d5_j2":	            Parent: "thoracic"  (index 15)
44,// Bone58:   "l_d5_j3":	            Parent: "thoracic"  (index 15)
43,// Bone59:   "l_d4_j1":	            Parent: "thoracic"  (index 15)
44,// Bone60:   "l_d4_j2":	            Parent: "thoracic"  (index 15)
44,// Bone61:   "l_d4_j3":	            Parent: "thoracic"  (index 15)
45,// Bone62:   "l_d3_j1":	            Parent: "thoracic"  (index 15)
46,// Bone63:   "l_d3_j2":	            Parent: "thoracic"  (index 15)
46,// Bone64:   "l_d3_j3":	            Parent: "thoracic"  (index 15)
45,// Bone65:   "l_d2_j1":	            Parent: "thoracic"  (index 15)
46,// Bone66:   "l_d2_j2":	            Parent: "thoracic"  (index 15)
46,// Bone67:   "l_d2_j3":	            Parent: "thoracic"  (index 15)
47,// Bone68:   "l_d1_j1":				Parent: "thoracic"  (index 15)
48,// Bone69:   "l_d1_j2":	            Parent: "thoracic"  (index 15)
48,// Bone70:   "l_d1_j3":				Parent: "thoracic"  (index 15)
52// Bone71:   "face_always_":			Parent: "cranium"  (index 17)
};


/*

Bone   0:   "model_root":
            Parent: ""  (index -1)
            #Kids:  1
            Child 0: (index 1), name "pelvis"

Bone   1:   "pelvis":
            Parent: "model_root"  (index 0)
            #Kids:  4
            Child 0: (index 2), name "Motion"
            Child 1: (index 3), name "lfemurYZ"
            Child 2: (index 7), name "rfemurYZ"
            Child 3: (index 11), name "lower_lumbar"

Bone   2:   "Motion":
            Parent: "pelvis"  (index 1)
            #Kids:  0

Bone   3:   "lfemurYZ":
            Parent: "pelvis"  (index 1)
            #Kids:  3
            Child 0: (index 4), name "lfemurX"
            Child 1: (index 5), name "ltibia"
            Child 2: (index 49), name "ltail"

Bone   4:   "lfemurX":
            Parent: "lfemurYZ"  (index 3)
            #Kids:  0

Bone   5:   "ltibia":
            Parent: "lfemurYZ"  (index 3)
            #Kids:  1
            Child 0: (index 6), name "ltalus"

Bone   6:   "ltalus":
            Parent: "ltibia"  (index 5)
            #Kids:  0

Bone   7:   "rfemurYZ":
            Parent: "pelvis"  (index 1)
            #Kids:  3
            Child 0: (index 8), name "rfemurX"
            Child 1: (index 9), name "rtibia"
            Child 2: (index 50), name "rtail"

Bone   8:   "rfemurX":
            Parent: "rfemurYZ"  (index 7)
            #Kids:  0

Bone   9:   "rtibia":
            Parent: "rfemurYZ"  (index 7)
            #Kids:  1
            Child 0: (index 10), name "rtalus"

Bone  10:   "rtalus":
            Parent: "rtibia"  (index 9)
            #Kids:  0

Bone  11:   "lower_lumbar":
            Parent: "pelvis"  (index 1)
            #Kids:  1
            Child 0: (index 12), name "upper_lumbar"

Bone  12:   "upper_lumbar":
            Parent: "lower_lumbar"  (index 11)
            #Kids:  1
            Child 0: (index 13), name "thoracic"

Bone  13:   "thoracic":
            Parent: "upper_lumbar"  (index 12)
            #Kids:  5
            Child 0: (index 14), name "cervical"
            Child 1: (index 24), name "rclavical"
            Child 2: (index 25), name "rhumerus"
            Child 3: (index 37), name "lclavical"
            Child 4: (index 38), name "lhumerus"

Bone  14:   "cervical":
            Parent: "thoracic"  (index 13)
            #Kids:  1
            Child 0: (index 15), name "cranium"

Bone  15:   "cranium":
            Parent: "cervical"  (index 14)
            #Kids:  1
            Child 0: (index 52), name "face_always_"

Bone  16:   "ceyebrow":
            Parent: "face_always_"  (index 52)
            #Kids:  0

Bone  17:   "jaw":
            Parent: "face_always_"  (index 52)
            #Kids:  0

Bone  18:   "lblip2":
            Parent: "face_always_"  (index 52)
            #Kids:  0

Bone  19:   "leye":
            Parent: "face_always_"  (index 52)
            #Kids:  0

Bone  20:   "rblip2":
            Parent: "face_always_"  (index 52)
            #Kids:  0

Bone  21:   "ltlip2":
            Parent: "face_always_"  (index 52)
            #Kids:  0

Bone  22:   "rtlip2":
            Parent: "face_always_"  (index 52)
            #Kids:  0

Bone  23:   "reye":
            Parent: "face_always_"  (index 52)
            #Kids:  0

Bone  24:   "rclavical":
            Parent: "thoracic"  (index 13)
            #Kids:  0

Bone  25:   "rhumerus":
            Parent: "thoracic"  (index 13)
            #Kids:  2
            Child 0: (index 26), name "rhumerusX"
            Child 1: (index 27), name "rradius"

Bone  26:   "rhumerusX":
            Parent: "rhumerus"  (index 25)
            #Kids:  0

Bone  27:   "rradius":
            Parent: "rhumerus"  (index 25)
            #Kids:  9
            Child 0: (index 28), name "rradiusX"
            Child 1: (index 29), name "rhand"
            Child 2: (index 30), name "r_d1_j1"
            Child 3: (index 31), name "r_d1_j2"
            Child 4: (index 32), name "r_d2_j1"
            Child 5: (index 33), name "r_d2_j2"
            Child 6: (index 34), name "r_d4_j1"
            Child 7: (index 35), name "r_d4_j2"
            Child 8: (index 36), name "rhang_tag_bone"

Bone  28:   "rradiusX":
            Parent: "rradius"  (index 27)
            #Kids:  0

Bone  29:   "rhand":
            Parent: "rradius"  (index 27)
            #Kids:  0

Bone  30:   "r_d1_j1":
            Parent: "rradius"  (index 27)
            #Kids:  0

Bone  31:   "r_d1_j2":
            Parent: "rradius"  (index 27)
            #Kids:  0

Bone  32:   "r_d2_j1":
            Parent: "rradius"  (index 27)
            #Kids:  0

Bone  33:   "r_d2_j2":
            Parent: "rradius"  (index 27)
            #Kids:  0

Bone  34:   "r_d4_j1":
            Parent: "rradius"  (index 27)
            #Kids:  0

Bone  35:   "r_d4_j2":
            Parent: "rradius"  (index 27)
            #Kids:  0

Bone  36:   "rhang_tag_bone":
            Parent: "rradius"  (index 27)
            #Kids:  0

Bone  37:   "lclavical":
            Parent: "thoracic"  (index 13)
            #Kids:  0

Bone  38:   "lhumerus":
            Parent: "thoracic"  (index 13)
            #Kids:  2
            Child 0: (index 39), name "lhumerusX"
            Child 1: (index 40), name "lradius"

Bone  39:   "lhumerusX":
            Parent: "lhumerus"  (index 38)
            #Kids:  0

Bone  40:   "lradius":
            Parent: "lhumerus"  (index 38)
            #Kids:  9
            Child 0: (index 41), name "lradiusX"
            Child 1: (index 42), name "lhand"
            Child 2: (index 43), name "l_d4_j1"
            Child 3: (index 44), name "l_d4_j2"
            Child 4: (index 45), name "l_d2_j1"
            Child 5: (index 46), name "l_d2_j2"
            Child 6: (index 47), name "l_d1_j1"
            Child 7: (index 48), name "l_d1_j2"
            Child 8: (index 51), name "lhang_tag_bone"

Bone  41:   "lradiusX":
            Parent: "lradius"  (index 40)
            #Kids:  0

Bone  42:   "lhand":
            Parent: "lradius"  (index 40)
            #Kids:  0

Bone  43:   "l_d4_j1":
            Parent: "lradius"  (index 40)
            #Kids:  0

Bone  44:   "l_d4_j2":
            Parent: "lradius"  (index 40)
            #Kids:  0

Bone  45:   "l_d2_j1":
            Parent: "lradius"  (index 40)
            #Kids:  0

Bone  46:   "l_d2_j2":
            Parent: "lradius"  (index 40)
            #Kids:  0

Bone  47:   "l_d1_j1":
            Parent: "lradius"  (index 40)
            #Kids:  0

Bone  48:   "l_d1_j2":
            Parent: "lradius"  (index 40)
            #Kids:  0

Bone  49:   "ltail":
            Parent: "lfemurYZ"  (index 3)
            #Kids:  0

Bone  50:   "rtail":
            Parent: "rfemurYZ"  (index 7)
            #Kids:  0

Bone  51:   "lhang_tag_bone":
            Parent: "lradius"  (index 40)
            #Kids:  0

Bone  52:   "face_always_":
            Parent: "cranium"  (index 15)
            #Kids:  8
            Child 0: (index 16), name "ceyebrow"
            Child 1: (index 17), name "jaw"
            Child 2: (index 18), name "lblip2"
            Child 3: (index 19), name "leye"
            Child 4: (index 20), name "rblip2"
            Child 5: (index 21), name "ltlip2"
            Child 6: (index 22), name "rtlip2"
            Child 7: (index 23), name "reye"



*/

qboolean R_LoadMDXM(model_t *mod, void *buffer, const char *mod_name, qboolean &bAlreadyCached)
{
	int					i,l, j;
	mdxmHeader_t		*pinmodel, *mdxm;
	mdxmLOD_t			*lod;
	mdxmSurface_t		*surf;
	int					version;
	int					size;
	mdxmSurfHierarchy_t	*surfInfo;

	pinmodel= (mdxmHeader_t *)buffer;
	//
	// read some fields from the binary, but only LittleLong() them when we know this wasn't an already-cached model...
	//
	version = (pinmodel->version);
	size	= (pinmodel->ofsEnd);

	if (!bAlreadyCached)
	{
		LL(version);
		LL(size);
	}

	if (version != MDXM_VERSION) {
		Com_Printf (S_COLOR_YELLOW  "R_LoadMDXM: %s has wrong version (%i should be %i)\n",
				 mod_name, version, MDXM_VERSION);
		return qfalse;
	}

	mod->type	   = MOD_MDXM;
	mod->dataSize += size;

	qboolean bAlreadyFound = qfalse;
	mdxm = (mdxmHeader_t*)CModelCache->Allocate(size, buffer, mod_name, &bAlreadyFound, TAG_MODEL_GLM);
	mod->data.glm = (mdxmData_t *)Hunk_Alloc(sizeof (mdxmData_t), h_low);
	mod->data.glm->header = mdxm;

	//RE_RegisterModels_Malloc(size, buffer, mod_name, &bAlreadyFound, TAG_MODEL_GLM);

	assert(bAlreadyCached == bAlreadyFound);

	if (!bAlreadyFound)
	{
		// horrible new hackery, if !bAlreadyFound then we've just done a
		// tag-morph, so we need to set the bool reference passed into this
		// function to true, to tell the caller NOT to do an ri.FS_Freefile
		// since we've hijacked that memory block...
		//
		// Aaaargh. Kill me now...
		//
		bAlreadyCached = qtrue;
		assert( mdxm == buffer );

		LL(mdxm->ident);
		LL(mdxm->version);
		LL(mdxm->numLODs);
		LL(mdxm->ofsLODs);
		LL(mdxm->numSurfaces);
		LL(mdxm->ofsSurfHierarchy);
		LL(mdxm->ofsEnd);
	}

	if (mdxm->numBones > MAX_G2_BONES)
	{
		Com_Printf(S_COLOR_YELLOW  "R_LoadMDXM: model %s has too many bones for rend2\n", mdxm->name);
		return qfalse;
	}

	// first up, go load in the animation file we need that has the skeletal
	// animation info for this model
	mdxm->animIndex = RE_RegisterModel(va("%s.gla", mdxm->animName));
#ifdef REND2_SP
	char animGLAName[MAX_QPATH];
	char* strippedName;
	char* slash = NULL;
	const char* mapname = sv_mapname->string;

	if (strcmp(mapname, "nomap"))
	{
		if (strrchr(mapname, '/'))	//maps in subfolders use the root name, ( presuming only one level deep!)
		{
			mapname = strrchr(mapname, '/') + 1;
		}
		//stripped name of GLA for this model
		Q_strncpyz(animGLAName, mdxm->animName, sizeof(animGLAName));
		slash = strrchr(animGLAName, '/');
		if (slash)
		{
			*slash = 0;
		}
		strippedName = COM_SkipPath(animGLAName);
		if (VALIDSTRING(strippedName))
		{
			RE_RegisterModel(va("models/players/%s_%s/%s_%s.gla", strippedName, mapname, strippedName, mapname));
		}
	}

#endif

	if (!mdxm->animIndex)
	{
		Com_Printf (S_COLOR_YELLOW  "R_LoadMDXM: missing animation file %s for mesh %s\n", mdxm->animName, mdxm->name);
		return qfalse;
	}

	mod->numLods = mdxm->numLODs -1 ;	//copy this up to the model for ease of use - it wil get inced after this.

#ifndef JK2_MODE
	bool isAnOldModelFile = false;
	if (mdxm->numBones == 72 && strstr(mdxm->animName,"_humanoid") )
	{
		isAnOldModelFile = true;
	}
#endif
	surfInfo = (mdxmSurfHierarchy_t *)( (byte *)mdxm + mdxm->ofsSurfHierarchy);
 	for ( i = 0 ; i < mdxm->numSurfaces ; i++)
	{
		LL(surfInfo->numChildren);
		LL(surfInfo->parentIndex);
#ifndef JK2_MODE
		Q_strlwr(surfInfo->name);	//just in case
		if ( !strcmp( &surfInfo->name[strlen(surfInfo->name)-4],"_off") )
		{
			surfInfo->name[strlen(surfInfo->name)-4]=0;	//remove "_off" from name
		}
#endif

		if (surfInfo->shader[0] == '[')
		{
			surfInfo->shader[0] = 0;	//kill the stupid [nomaterial] since carcass doesn't
		}

		// do all the children indexs
		for (j=0; j<surfInfo->numChildren; j++)
		{
			LL(surfInfo->childIndexes[j]);
		}

		shader_t	*sh;
		// get the shader name
		sh = R_FindShader( surfInfo->shader, lightmapsNone, stylesDefault, qtrue );
		// insert it in the surface list
		if ( sh->defaultShader )
		{
			surfInfo->shaderIndex = 0;
		}
		else
		{
			surfInfo->shaderIndex = sh->index;
		}

		CModelCache->StoreShaderRequest(mod_name, &surfInfo->shader[0], &surfInfo->shaderIndex);

		// find the next surface
		surfInfo = (mdxmSurfHierarchy_t *)((byte *)surfInfo + offsetof(mdxmSurfHierarchy_t, childIndexes) + sizeof(int) * surfInfo->numChildren);
	}

	// swap all the LOD's	(we need to do the middle part of this even for intel, because of shader reg and err-check)
	lod = (mdxmLOD_t *) ( (byte *)mdxm + mdxm->ofsLODs );
	for ( l = 0 ; l < mdxm->numLODs ; l++)
	{
		//int	triCount = 0;

		LL(lod->ofsEnd);
		// swap all the surfaces
		surf = (mdxmSurface_t *) ( (byte *)lod + sizeof (mdxmLOD_t) + (mdxm->numSurfaces * sizeof(mdxmLODSurfOffset_t)) );
		for ( i = 0 ; i < mdxm->numSurfaces ; i++)
		{
			LL(surf->numTriangles);
			LL(surf->ofsTriangles);
			LL(surf->numVerts);
			LL(surf->ofsVerts);
			LL(surf->ofsEnd);
			LL(surf->ofsHeader);
			LL(surf->numBoneReferences);
			LL(surf->ofsBoneReferences);
//			LL(surf->maxVertBoneWeights);

			//triCount += surf->numTriangles;

			if ( surf->numVerts > SHADER_MAX_VERTEXES ) {
				Com_Error(
					ERR_DROP,
					"R_LoadMDXM: %s has more than %i verts on a surface (%i)",
					mod_name,
					SHADER_MAX_VERTEXES,
					surf->numVerts);
			}
			if ( surf->numTriangles*3 > SHADER_MAX_INDEXES ) {
				Com_Error(
					ERR_DROP,
					"R_LoadMDXM: %s has more than %i triangles on a surface (%i)",
					mod_name,
					SHADER_MAX_INDEXES / 3,
					surf->numTriangles);
			}

			// change to surface identifier
			surf->ident = SF_MDX;
			// register the shaders
#ifndef JK2_MODE
			if (isAnOldModelFile)
			{
				int *boneRef = (int *) ( (byte *)surf + surf->ofsBoneReferences );
				for ( j = 0 ; j < surf->numBoneReferences ; j++ )
				{
					if (boneRef[j] >= 0 && boneRef[j] < 72)
					{
						boneRef[j]=OldToNewRemapTable[boneRef[j]];
					}
					else
					{
						boneRef[j]=0;
					}
				}
			}
#endif
			// find the next surface
			surf = (mdxmSurface_t *)( (byte *)surf + surf->ofsEnd );
		}
		// find the next LOD
		lod = (mdxmLOD_t *)( (byte *)lod + lod->ofsEnd );
	}

	// Make a copy on the GPU
	lod = (mdxmLOD_t *)((byte *)mdxm + mdxm->ofsLODs);

	mod->data.glm->vboModels = (mdxmVBOModel_t *)Hunk_Alloc (sizeof (mdxmVBOModel_t) * mdxm->numLODs, h_low);
	for ( l = 0; l < mdxm->numLODs; l++ )
	{
		mdxmVBOModel_t *vboModel = &mod->data.glm->vboModels[l];
		mdxmVBOMesh_t *vboMeshes;

		vec3_t *verts;
		uint32_t *normals;
		vec2_t *texcoords;
		byte *bonerefs;
		byte *weights;
		uint32_t *tangents;

		byte *data;
		int dataSize = 0;
		int ofsPosition, ofsNormals, ofsTexcoords, ofsBoneRefs, ofsWeights, ofsTangents, ofsColor, ofsLMCoords, ofsLightDir;;
		int stride = 0;
		int numVerts = 0;
		int numTriangles = 0;

		// +1 to add total vertex count
		int *baseVertexes = (int *)Hunk_AllocateTempMemory (sizeof (int) * (mdxm->numSurfaces + 1));
		int *indexOffsets = (int *)Hunk_AllocateTempMemory (sizeof (int) * mdxm->numSurfaces);

		vboModel->numVBOMeshes = mdxm->numSurfaces;
		vboModel->vboMeshes = (mdxmVBOMesh_t *)Hunk_Alloc (sizeof (mdxmVBOMesh_t) * mdxm->numSurfaces, h_low);
		vboMeshes = vboModel->vboMeshes;

		surf = (mdxmSurface_t *)((byte *)lod + sizeof (mdxmLOD_t) + (mdxm->numSurfaces * sizeof (mdxmLODSurfOffset_t)));

		// Calculate the required size of the vertex buffer.
		for ( int n = 0; n < mdxm->numSurfaces; n++ )
		{
			baseVertexes[n] = numVerts;
			indexOffsets[n] = numTriangles * 3;

			numVerts += surf->numVerts;
			numTriangles += surf->numTriangles;

			surf = (mdxmSurface_t *)((byte *)surf + surf->ofsEnd);
		}

		baseVertexes[mdxm->numSurfaces] = numVerts;

		dataSize += numVerts * sizeof (*verts);
		dataSize += numVerts * sizeof (*normals);
		dataSize += numVerts * sizeof (*texcoords);
		dataSize += numVerts * sizeof (*weights) * 4;
		dataSize += numVerts * sizeof (*bonerefs) * 4;
		dataSize += numVerts * sizeof (*tangents);
		dataSize += sizeof(vec4_t) + sizeof(vec2_t) + sizeof(uint32_t);

		// Allocate and write to memory
		data = (byte *)Hunk_AllocateTempMemory (dataSize);

		ofsPosition = stride;
		verts = (vec3_t *)(data + ofsPosition);
		stride += sizeof (*verts);

		ofsNormals = stride;
		normals = (uint32_t *)(data + ofsNormals);
		stride += sizeof (*normals);

		ofsTexcoords = stride;
		texcoords = (vec2_t *)(data + ofsTexcoords);
		stride += sizeof (*texcoords);

		ofsBoneRefs = stride;
		bonerefs = data + ofsBoneRefs;
		stride += sizeof (*bonerefs) * 4;

		ofsWeights = stride;
		weights = data + ofsWeights;
		stride += sizeof (*weights) * 4;

		ofsTangents = stride;
		tangents = (uint32_t *)(data + ofsTangents);
		stride += sizeof (*tangents);

		ofsColor = dataSize - (sizeof(vec4_t) + sizeof(vec2_t) + sizeof(uint32_t));
		float *color = (float *)(data + ofsColor);
		VectorSet4(color, 1.0f, 1.0f, 1.0f, 1.0f);

		ofsLMCoords = dataSize - (sizeof(vec2_t) + sizeof(uint32_t));
		float *lmTcs = (float *)(data + ofsLMCoords);
		VectorSet2(lmTcs, 0.0f, 0.0f);

		ofsLightDir = dataSize - sizeof(uint32_t);
		uint32_t *lightdir = (uint32_t*)(data + ofsLightDir);
		*lightdir = 0;

		// Fill in the index buffer and compute tangents
		glIndex_t *indices = (glIndex_t *)Hunk_AllocateTempMemory(sizeof(glIndex_t) * numTriangles * 3);
		glIndex_t *index = indices;
		uint32_t *tangentsf = (uint32_t *)Hunk_AllocateTempMemory(sizeof(uint32_t) * numVerts);

		surf = (mdxmSurface_t *)((byte *)lod + sizeof(mdxmLOD_t) + (mdxm->numSurfaces * sizeof(mdxmLODSurfOffset_t)));

		for (int n = 0; n < mdxm->numSurfaces; n++)
		{
			mdxmTriangle_t *t = (mdxmTriangle_t *)((byte *)surf + surf->ofsTriangles);
			glIndex_t *surf_indices = (glIndex_t *)Hunk_AllocateTempMemory(sizeof(glIndex_t) * surf->numTriangles * 3);
			glIndex_t *surf_index = surf_indices;

			for (int k = 0; k < surf->numTriangles; k++, index += 3, surf_index += 3)
			{
				index[0] = t[k].indexes[0] + baseVertexes[n];
				assert(index[0] >= 0 && index[0] < (unsigned)numVerts);

				index[1] = t[k].indexes[1] + baseVertexes[n];
				assert(index[1] >= 0 && index[1] < (unsigned)numVerts);

				index[2] = t[k].indexes[2] + baseVertexes[n];
				assert(index[2] >= 0 && index[2] < (unsigned)numVerts);

				surf_index[0] = t[k].indexes[0];
				surf_index[1] = t[k].indexes[1];
				surf_index[2] = t[k].indexes[2];
			}

			// Build tangent space
			mdxmVertex_t *vertices = (mdxmVertex_t *)((byte *)surf + surf->ofsVerts);
			mdxmVertexTexCoord_t *textureCoordinates = (mdxmVertexTexCoord_t *)(vertices + surf->numVerts);

			R_CalcMikkTSpaceGlmSurface(
				surf->numTriangles,
				vertices,
				textureCoordinates,
				tangentsf + baseVertexes[n],
				surf_indices
			);

			Hunk_FreeTempMemory(surf_indices);

			surf = (mdxmSurface_t *)((byte *)surf + surf->ofsEnd);
		}

		assert(index == (indices + numTriangles * 3));

		surf = (mdxmSurface_t *)((byte *)lod + sizeof (mdxmLOD_t) + (mdxm->numSurfaces * sizeof (mdxmLODSurfOffset_t)));

		for (int n = 0; n < mdxm->numSurfaces; n++)
		{
			// Positions and normals
			mdxmVertex_t *v = (mdxmVertex_t *)((byte *)surf + surf->ofsVerts);
			int *boneRef = (int *)((byte *)surf + surf->ofsBoneReferences);

			for (int k = 0; k < surf->numVerts; k++)
			{
				VectorCopy(v[k].vertCoords, *verts);
				*normals = R_VboPackNormal(v[k].normal);

				verts = (vec3_t *)((byte *)verts + stride);
				normals = (uint32_t *)((byte *)normals + stride);
			}

			// Weights
			for (int k = 0; k < surf->numVerts; k++)
			{
				int numWeights = G2_GetVertWeights(&v[k]);
				int lastWeight = 255;
				int lastInfluence = numWeights - 1;
				for (int w = 0; w < lastInfluence; w++)
				{
					float weight = G2_GetVertBoneWeightNotSlow(&v[k], w);
					weights[w] = (byte)(weight * 255.0f);
					int packedIndex = G2_GetVertBoneIndex(&v[k], w);
					bonerefs[w] = boneRef[packedIndex];

					lastWeight -= weights[w];
				}

				assert(lastWeight > 0);

				// Ensure that all the weights add up to 1.0
				weights[lastInfluence] = lastWeight;
				int packedIndex = G2_GetVertBoneIndex(&v[k], lastInfluence);
				bonerefs[lastInfluence] = boneRef[packedIndex];

				// Fill in the rest of the info with zeroes.
				for (int w = numWeights; w < 4; w++)
				{
					weights[w] = 0;
					bonerefs[w] = 0;
				}

				weights += stride;
				bonerefs += stride;
			}

			// Texture coordinates
			mdxmVertexTexCoord_t *tc = (mdxmVertexTexCoord_t *)(v + surf->numVerts);
			for (int k = 0; k < surf->numVerts; k++)
			{
				(*texcoords)[0] = tc[k].texCoords[0];
				(*texcoords)[1] = tc[k].texCoords[1];

				texcoords = (vec2_t *)((byte *)texcoords + stride);
			}

			for (int k = 0; k < surf->numVerts; k++)
			{
				*tangents = *(tangentsf + baseVertexes[n] + k);
				tangents = (uint32_t *)((byte *)tangents + stride);
			}

			surf = (mdxmSurface_t *)((byte *)surf + surf->ofsEnd);
		}

		// TODO: Check why this was here and why it always fails
		// assert ((byte *)verts == (data + dataSize));

		const char *modelName = strrchr (mdxm->name, '/');
		if (modelName == NULL)
		{
			modelName = mdxm->name;
		}
		VBO_t *vbo = R_CreateVBO (data, dataSize, VBO_USAGE_STATIC, mod_name);
		IBO_t *ibo = R_CreateIBO((byte *)indices, sizeof(glIndex_t) * numTriangles * 3, VBO_USAGE_STATIC, mod_name);

		Hunk_FreeTempMemory (data);
		Hunk_FreeTempMemory (tangentsf);
		Hunk_FreeTempMemory (indices);

		vbo->offsets[ATTR_INDEX_POSITION] = ofsPosition;
		vbo->offsets[ATTR_INDEX_NORMAL] = ofsNormals;
		vbo->offsets[ATTR_INDEX_TEXCOORD0] = ofsTexcoords;
		vbo->offsets[ATTR_INDEX_BONE_INDEXES] = ofsBoneRefs;
		vbo->offsets[ATTR_INDEX_BONE_WEIGHTS] = ofsWeights;
		vbo->offsets[ATTR_INDEX_TANGENT] = ofsTangents;

		vbo->offsets[ATTR_INDEX_COLOR] = ofsColor;
		vbo->offsets[ATTR_INDEX_TEXCOORD1] = ofsLMCoords;
		vbo->offsets[ATTR_INDEX_TEXCOORD2] = ofsLMCoords;
		vbo->offsets[ATTR_INDEX_TEXCOORD3] = ofsLMCoords;
		vbo->offsets[ATTR_INDEX_TEXCOORD4] = ofsLMCoords;
		vbo->offsets[ATTR_INDEX_LIGHTDIRECTION] = ofsLightDir;

		vbo->strides[ATTR_INDEX_POSITION] = stride;
		vbo->strides[ATTR_INDEX_NORMAL] = stride;
		vbo->strides[ATTR_INDEX_TEXCOORD0] = stride;
		vbo->strides[ATTR_INDEX_BONE_INDEXES] = stride;
		vbo->strides[ATTR_INDEX_BONE_WEIGHTS] = stride;
		vbo->strides[ATTR_INDEX_TANGENT] = stride;

		vbo->strides[ATTR_INDEX_COLOR] = sizeof(vec4_t);
		vbo->strides[ATTR_INDEX_TEXCOORD1] = sizeof(vec2_t);
		vbo->strides[ATTR_INDEX_TEXCOORD2] = sizeof(vec2_t);
		vbo->strides[ATTR_INDEX_TEXCOORD3] = sizeof(vec2_t);
		vbo->strides[ATTR_INDEX_TEXCOORD4] = sizeof(vec2_t);
		vbo->strides[ATTR_INDEX_LIGHTDIRECTION] = sizeof(uint32_t);

		vbo->sizes[ATTR_INDEX_POSITION] = sizeof(*verts);
		vbo->sizes[ATTR_INDEX_NORMAL] = sizeof(*normals);
		vbo->sizes[ATTR_INDEX_TEXCOORD0] = sizeof(*texcoords);
		vbo->sizes[ATTR_INDEX_BONE_WEIGHTS] = sizeof(*weights);
		vbo->sizes[ATTR_INDEX_BONE_INDEXES] = sizeof(*bonerefs);
		vbo->sizes[ATTR_INDEX_TANGENT] = sizeof(*tangents);

		vbo->sizes[ATTR_INDEX_COLOR] = sizeof(vec4_t);
		vbo->sizes[ATTR_INDEX_TEXCOORD1] = sizeof(vec2_t);
		vbo->sizes[ATTR_INDEX_TEXCOORD2] = sizeof(vec2_t);
		vbo->sizes[ATTR_INDEX_TEXCOORD3] = sizeof(vec2_t);
		vbo->sizes[ATTR_INDEX_TEXCOORD4] = sizeof(vec2_t);
		vbo->sizes[ATTR_INDEX_LIGHTDIRECTION] = sizeof(uint32_t);

		vbo->stepRates[ATTR_INDEX_COLOR] = MAX_INSTANCES;
		vbo->stepRates[ATTR_INDEX_TEXCOORD1] = MAX_INSTANCES;
		vbo->stepRates[ATTR_INDEX_TEXCOORD2] = MAX_INSTANCES;
		vbo->stepRates[ATTR_INDEX_TEXCOORD3] = MAX_INSTANCES;
		vbo->stepRates[ATTR_INDEX_TEXCOORD4] = MAX_INSTANCES;
		vbo->stepRates[ATTR_INDEX_LIGHTDIRECTION] = MAX_INSTANCES;

		surf = (mdxmSurface_t *)((byte *)lod + sizeof (mdxmLOD_t) + (mdxm->numSurfaces * sizeof (mdxmLODSurfOffset_t)));

		for ( int n = 0; n < mdxm->numSurfaces; n++ )
		{
			vboMeshes[n].vbo = vbo;
			vboMeshes[n].ibo = ibo;

			vboMeshes[n].indexOffset = indexOffsets[n];
			vboMeshes[n].minIndex = baseVertexes[n];
			vboMeshes[n].maxIndex = baseVertexes[n + 1] - 1;
			vboMeshes[n].numVertexes = surf->numVerts;
			vboMeshes[n].numIndexes = surf->numTriangles * 3;

			surf = (mdxmSurface_t *)((byte *)surf + surf->ofsEnd);
		}

		vboModel->vbo = vbo;
		vboModel->ibo = ibo;

		Hunk_FreeTempMemory (indexOffsets);
		Hunk_FreeTempMemory (baseVertexes);

		lod = (mdxmLOD_t *)((byte *)lod + lod->ofsEnd);
	}

	return qtrue;
}

//#define CREATE_LIMB_HIERARCHY

#ifdef CREATE_LIMB_HIERARCHY

#define NUM_ROOTPARENTS				4
#define NUM_OTHERPARENTS			12
#define NUM_BOTTOMBONES				4

#define CHILD_PADDING				4 //I don't know, I guess this can be changed.

static const char *rootParents[NUM_ROOTPARENTS] =
{
	"rfemurYZ",
	"rhumerus",
	"lfemurYZ",
	"lhumerus"
};

static const char *otherParents[NUM_OTHERPARENTS] =
{
	"rhumerusX",
	"rradius",
	"rradiusX",
	"lhumerusX",
	"lradius",
	"lradiusX",
	"rfemurX",
	"rtibia",
	"rtalus",
	"lfemurX",
	"ltibia",
	"ltalus"
};

static const char *bottomBones[NUM_BOTTOMBONES] =
{
	"rtarsal",
	"rhand",
	"ltarsal",
	"lhand"
};

qboolean BoneIsRootParent(char *name)
{
	int i = 0;

	while (i < NUM_ROOTPARENTS)
	{
		if (!Q_stricmp(name, rootParents[i]))
		{
			return qtrue;
		}

		i++;
	}

	return qfalse;
}

qboolean BoneIsOtherParent(char *name)
{
	int i = 0;

	while (i < NUM_OTHERPARENTS)
	{
		if (!Q_stricmp(name, otherParents[i]))
		{
			return qtrue;
		}

		i++;
	}

	return qfalse;
}

qboolean BoneIsBottom(char *name)
{
	int i = 0;

	while (i < NUM_BOTTOMBONES)
	{
		if (!Q_stricmp(name, bottomBones[i]))
		{
			return qtrue;
		}

		i++;
	}

	return qfalse;
}

void ShiftMemoryDown(mdxaSkelOffsets_t *offsets, mdxaHeader_t *mdxa, int boneIndex, byte **endMarker)
{
	int i = 0;

	//where the next bone starts
	byte *nextBone = ((byte *)mdxa + sizeof(mdxaHeader_t) + offsets->offsets[boneIndex+1]);
	int size = (*endMarker - nextBone);

	memmove((nextBone+CHILD_PADDING), nextBone, size);
	memset(nextBone, 0, CHILD_PADDING);
	*endMarker += CHILD_PADDING;
	// Move the whole thing down CHILD_PADDING amount in memory, clear the new
	// preceding space, and increment the end pointer.

	i = boneIndex+1;

	// Now add CHILD_PADDING amount to every offset beginning at the offset of
	// the bone that was moved.
	while (i < mdxa->numBones)
	{
		offsets->offsets[i] += CHILD_PADDING;
		i++;
	}

	mdxa->ofsFrames += CHILD_PADDING;
	mdxa->ofsCompBonePool += CHILD_PADDING;
	mdxa->ofsEnd += CHILD_PADDING;
	// ofsSkel does not need to be updated because we are only moving memory
	// after that point.
}

//Proper/desired hierarchy list
static const char *BoneHierarchyList[] =
{
	"lfemurYZ",
	"lfemurX",
	"ltibia",
	"ltalus",
	"ltarsal",

	"rfemurYZ",
	"rfemurX",
	"rtibia",
	"rtalus",
	"rtarsal",

	"lhumerus",
	"lhumerusX",
	"lradius",
	"lradiusX",
	"lhand",

	"rhumerus",
	"rhumerusX",
	"rradius",
	"rradiusX",
	"rhand",

	0
};

//Gets the index of a child or parent. If child is passed as qfalse then parent is assumed.
int BoneParentChildIndex(
	mdxaHeader_t *mdxa,
	mdxaSkelOffsets_t *offsets,
	mdxaSkel_t *boneInfo,
	qboolean child)
{
	int i = 0;
	int matchindex = -1;
	mdxaSkel_t *bone;
	const char *match = NULL;

	while (BoneHierarchyList[i])
	{
		if (!Q_stricmp(boneInfo->name, BoneHierarchyList[i]))
		{
			// we have a match, the slot above this will be our desired parent.
			// (or below for child)
			if (child)
			{
				match = BoneHierarchyList[i+1];
			}
			else
			{
				match = BoneHierarchyList[i-1];
			}
			break;
		}
		i++;
	}

	if (!match)
	{ //no good
		return -1;
	}

	i = 0;

	while (i < mdxa->numBones)
	{
		bone = (mdxaSkel_t *)((byte *)mdxa + sizeof(mdxaHeader_t) + offsets->offsets[i]);

		if (bone && !Q_stricmp(bone->name, match))
		{ //this is the one
			matchindex = i;
			break;
		}

		i++;
	}

	return matchindex;
}
#endif //CREATE_LIMB_HIERARCHY

/*
=================
R_LoadMDXA - load a Ghoul 2 animation file
=================
*/
qboolean R_LoadMDXA(model_t *mod, void *buffer, const char *mod_name, qboolean &bAlreadyCached)
{

	mdxaHeader_t *pinmodel, *mdxa;
	int version;
	int size;
#ifdef CREATE_LIMB_HIERARCHY
	int oSize = 0;
	byte *sizeMarker;
#endif

#if 0 //#ifndef _M_IX86
	int					j, k, i;
	int					frameSize;
	mdxaFrame_t			*cframe;
	mdxaSkel_t			*boneInfo;
#endif

	pinmodel = (mdxaHeader_t *)buffer;
	//
	// read some fields from the binary, but only LittleLong() them when we know this wasn't an
	// already-cached model...
	//
	version = (pinmodel->version);
	size = (pinmodel->ofsEnd);

	if (!bAlreadyCached)
	{
		LL(version);
		LL(size);
	}

	if (version != MDXA_VERSION)
	{
		Com_Printf(
			S_COLOR_YELLOW "R_LoadMDXA: %s has wrong version (%i should be %i)\n",
			mod_name,
			version,
			MDXA_VERSION);
		return qfalse;
	}

	mod->type = MOD_MDXA;
	mod->dataSize += size;

	qboolean bAlreadyFound = qfalse;

#ifdef CREATE_LIMB_HIERARCHY
	oSize = size;

	int childNumber = (NUM_ROOTPARENTS + NUM_OTHERPARENTS);

	// Allocate us some extra space so we can shift memory down.
	size += (childNumber * (CHILD_PADDING * 8));
#endif // CREATE_LIMB_HIERARCHY

	mdxa = (mdxaHeader_t *)CModelCache->Allocate(
		size, buffer, mod_name, &bAlreadyFound, TAG_MODEL_GLA);
	mod->data.gla = mdxa;

	// I should probably eliminate 'bAlreadyFound', but wtf?
	assert(bAlreadyCached == bAlreadyFound);

	if (!bAlreadyFound)
	{
#ifdef CREATE_LIMB_HIERARCHY
		memcpy(mdxa, buffer, oSize);
#else
		// horrible new hackery, if !bAlreadyFound then we've just done a
		// tag-morph, so we need to set the bool reference passed into this
		// function to true, to tell the caller NOT to do an
		// ri.FS_Freefile since we've hijacked that memory block...
		//
		// Aaaargh. Kill me now...
		//
		bAlreadyCached = qtrue;
		assert(mdxa == buffer);
#endif
		LL(mdxa->ident);
		LL(mdxa->version);
		LL(mdxa->numFrames);
		LL(mdxa->numBones);
		LL(mdxa->ofsFrames);
		LL(mdxa->ofsEnd);
	}

#ifdef CREATE_LIMB_HIERARCHY
	if (!bAlreadyFound)
	{
		mdxaSkel_t *boneParent;

		sizeMarker = (byte *)mdxa + mdxa->ofsEnd;

		// rww - This is probably temporary until we put actual hierarchy in
		// for the models.  It is necessary for the correct operation of
		// ragdoll.
		mdxaSkelOffsets_t *offsets = (mdxaSkelOffsets_t *)((byte *)mdxa + sizeof(mdxaHeader_t));

		for (i = 0; i < mdxa->numBones; i++)
		{
			boneInfo = (mdxaSkel_t *)((byte *)offsets + offsets->offsets[i]);

			if (boneInfo)
			{
				char *bname = boneInfo->name;

				if (BoneIsRootParent(bname))
				{
					// These are the main parent bones. We don't want to change
					// their parents, but we want to give them children.
					ShiftMemoryDown(offsets, mdxa, i, &sizeMarker);

					boneInfo = (mdxaSkel_t *)((byte *)offsets + offsets->offsets[i]);

					int newChild = BoneParentChildIndex(mdxa, offsets, boneInfo, qtrue);

					if (newChild != -1)
					{
						boneInfo->numChildren++;
						boneInfo->children[boneInfo->numChildren - 1] = newChild;
					}
					else
					{
						assert(!"Failed to find matching child for bone in hierarchy creation");
					}
				}
				else if (BoneIsOtherParent(bname) || BoneIsBottom(bname))
				{
					if (!BoneIsBottom(bname))
					{ // unless it's last in the chain it has the next bone as a child.
						ShiftMemoryDown(offsets, mdxa, i, &sizeMarker);

						boneInfo = (mdxaSkel_t *)
							((byte *)mdxa + sizeof(mdxaHeader_t) + offsets->offsets[i]);
						int newChild = BoneParentChildIndex(mdxa, offsets, boneInfo, qtrue);

						if (newChild != -1)
						{
							boneInfo->numChildren++;
							boneInfo->children[boneInfo->numChildren - 1] = newChild;
						}
						else
						{
							assert(!"Failed to find matching child for bone in hierarchy creation");
						}
					}

					// Before we set the parent we want to remove this as a
					// child for whoever was parenting it.
					int oldParent = boneInfo->parent;

					if (oldParent > -1)
					{
						boneParent = (mdxaSkel_t *)
							((byte *)offsets + offsets->offsets[oldParent]);
					}
					else
					{
						boneParent = NULL;
					}

					if (boneParent)
					{
						k = 0;

						while (k < boneParent->numChildren)
						{
							if (boneParent->children[k] == i)
							{ // this bone is the child
								k++;
								while (k < boneParent->numChildren)
								{
									boneParent->children[k - 1] = boneParent->children[k];
									k++;
								}
								boneParent->children[k - 1] = 0;
								boneParent->numChildren--;
								break;
							}
							k++;
						}
					}

					// Now that we have cleared the original parent of
					// ownership, mark the bone's new parent.
					int newParent = BoneParentChildIndex(mdxa, offsets, boneInfo, qfalse);

					if (newParent != -1)
					{
						boneInfo->parent = newParent;
					}
					else
					{
						assert(!"Failed to find matching parent for bone in hierarchy creation");
					}
				}
			}
		}
	}
#endif // CREATE_LIMB_HIERARCHY

	if (mdxa->numFrames < 1)
	{
		Com_Printf(S_COLOR_YELLOW "R_LoadMDXA: %s has no frames\n", mod_name);
		return qfalse;
	}

	if (bAlreadyFound)
	{
		return qtrue; // All done, stop here, do not LittleLong() etc. Do not pass go...
	}

	return qtrue;
}

