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

bool G2_TestModelPointers(CGhoul2Info *ghlInfo);

#if G2API_DEBUG
#include <float.h> //for isnan


#define MAX_ERROR_PRINTS (3)
class ErrorReporter
{
	std::string mName;
	std::map<std::string,int> mErrors;
public:
	ErrorReporter(const std::string &name) :
	  mName(name)
	{
	}
	~ErrorReporter()
	{
		char mess[1000];
		int total=0;
		sprintf(mess,"****** %s Error Report Begin******\n",mName.c_str());
		Com_DPrintf(mess);

		std::map<std::string,int>::iterator i;
		for (i=mErrors.begin();i!=mErrors.end();++i)
		{
			total+=(*i).second;
			sprintf(mess,"%s (hits %d)\n",(*i).first.c_str(),(*i).second);
			Com_DPrintf(mess);
		}

		sprintf(mess,"****** %s Error Report End   %d errors of %ld kinds******\n",mName.c_str(),total,mErrors.size());
		Com_DPrintf(mess);
	}
	int AnimTest(CGhoul2Info_v &ghoul2,const char *m,const char *, int line)
	{
		if (G2_SetupModelPointers(ghoul2))
		{
			int i;
			for (i=0; i<ghoul2.size(); i++)
			{
				AnimTest(&ghoul2[i],m,0,line);
			}
			return i;
		}
		return 666; //these return values are to saisfy the optimizer
	}
	int AnimTest(CGhoul2Info *ghlInfo,const char *m,const char *, int line)
	{
		bool ok=G2_TestModelPointers(ghlInfo);
		if (!ok)
		{
			return 5; // I guess this happens from time to time
		}
		size_t i;
		int ret=0;

		char GLAName1[1000];
		char GLAName2[1000];
		char GLMName1[1000];
		char GLMName2[1000];

		strcpy(GLAName1,ghlInfo->animModel->name);
		strcpy(GLAName2,ghlInfo->aHeader->name);
		strcpy(GLMName1,ghlInfo->mFileName);
		strcpy(GLMName2,ghlInfo->currentModel->name);

		int numFramesInFile=ghlInfo->aHeader->numFrames;

		int numActiveBones=0;
		for (i=0;i<ghlInfo->mBlist.size();i++)
		{
			if (ghlInfo->mBlist[i].boneNumber!=-1) // slot used?
			{
				if (ghlInfo->mBlist[i].flags&BONE_ANIM_TOTAL) // anim on this?
				{
					numActiveBones++;
					bool loop=!!(ghlInfo->mBlist[i].flags&BONE_ANIM_OVERRIDE_LOOP);
					bool not_loop=!!(ghlInfo->mBlist[i].flags&BONE_ANIM_OVERRIDE);

					if (loop==not_loop)
					{
						Error("Unusual animation flags, should have some sort of override, but not both",1,0,line);
					}

					bool freeze=(ghlInfo->mBlist[i].flags&BONE_ANIM_OVERRIDE_FREEZE) == BONE_ANIM_OVERRIDE_FREEZE;

					if (loop&&freeze)
					{
						Error("Unusual animation flags, loop and freeze",1,0,line);
					}
					bool no_lerp=!!(ghlInfo->mBlist[i].flags&BONE_ANIM_NO_LERP);
					bool blend=!!(ghlInfo->mBlist[i].flags&BONE_ANIM_BLEND);


					//comments according to jake
					int			startFrame=ghlInfo->mBlist[i].startFrame;		// start frame for animation
					int			endFrame=ghlInfo->mBlist[i].endFrame;		// end frame for animation NOTE anim actually ends on endFrame+1
					int			startTime=ghlInfo->mBlist[i].startTime;		// time we started this animation
					int			pauseTime=ghlInfo->mBlist[i].pauseTime;		// time we paused this animation - 0 if not paused
					float		animSpeed=ghlInfo->mBlist[i].animSpeed;		// speed at which this anim runs. 1.0f means full speed of animation incoming - ie if anim is 20hrtz, we run at 20hrts. If 5hrts, we run at 5 hrts

					float		blendFrame=0.0f;		// frame PLUS LERP value to blend
					int			blendLerpFrame=0;	// frame to lerp the blend frame with.

					if (blend)
					{
						blendFrame=ghlInfo->mBlist[i].blendFrame;
						blendLerpFrame=ghlInfo->mBlist[i].blendLerpFrame;
						if (floor(blendFrame)<0.0f)
						{
							Error("negative blendFrame",1,0,line);
						}
						if (ceil(blendFrame)>=float(numFramesInFile))
						{
							Error("blendFrame >= numFramesInFile",1,0,line);
						}
						if (blendLerpFrame<0)
						{
							Error("negative blendLerpFrame",1,0,line);
						}
						if (blendLerpFrame>=numFramesInFile)
						{
							Error("blendLerpFrame >= numFramesInFile",1,0,line);
						}
					}
					if (startFrame<0)
					{
						Error("negative startFrame",1,0,line);
					}
					if (startFrame>=numFramesInFile)
					{
						Error("startFrame >= numFramesInFile",1,0,line);
					}
					if (endFrame<0)
					{
						Error("negative endFrame",1,0,line);
					}
					if (endFrame==0&&animSpeed>0.0f)
					{
						Error("Zero endFrame",1,0,line);
					}
					if (endFrame>numFramesInFile)
					{
						Error("endFrame > numFramesInFile",1,0,line);
					}
					// mikeg call out here for further checks.
					ret=(int)startTime+(int)pauseTime+(int)no_lerp; // quiet VC.
				}
			}
		}
		return ret;
	}
	int Error(const char *m,int kind,const char *, int line)
	{
		char mess[1000];
		assert(m);
		std::string full=mName;
		if (kind==2)
		{
			full+=":NOTE:     ";
		}
		else if (kind==1)
		{
//			assert(!"G2API Warning");
			full+=":WARNING:  ";
		}
		else
		{
//			assert(!"G2API Error");
			full+=":ERROR  :  ";
		}
		full+=m;
		sprintf(mess,"  [line %d]",line);
		full+=mess;

		// assert(0);
		int ret=0; //place a breakpoint here
		std::map<std::string,int>::iterator f=mErrors.find(full);
		if (f==mErrors.end())
		{
			ret++; // or a breakpoint here for the first occurance
			mErrors.insert(std::make_pair(full,0));
			f=mErrors.find(full);
		}
		assert(f!=mErrors.end());
		(*f).second++;
		if ((*f).second==1000)
		{
			ret*=-1; // breakpoint to find a specific occurance of an error
		}
		if ((*f).second<=MAX_ERROR_PRINTS&&kind<2)
		{
			Com_Printf("%s (hit # %d)\n",full.c_str(),(*f).second);
			if (1)
			{
				sprintf(mess,"%s (hit # %d)\n",full.c_str(),(*f).second);
				Com_DPrintf(mess);
			}
		}
		return ret;
	}
};

#include "assert.h"
ErrorReporter &G2APIError()
{
	static ErrorReporter singleton("G2API");
	return singleton;
}

#define G2ERROR(exp,m) (void)( (exp) || (G2APIError().Error(m,0,__FILE__,__LINE__), 0) )
#define G2WARNING(exp,m) (void)( (exp) || (G2APIError().Error(m,1,__FILE__,__LINE__), 0) )
#define G2NOTE(exp,m) (void)( (exp) || (G2APIError().Error(m,2,__FILE__,__LINE__), 0) )
#define G2ANIM(ghlInfo,m) (void)((G2APIError().AnimTest(ghlInfo,m,__FILE__,__LINE__), 0) )
#else

#define G2ERROR(exp,m)		((void)0)
#define G2WARNING(exp,m)     ((void)0)
#define G2NOTE(exp,m)     ((void)0)
#define G2ANIM(ghlInfo,m) ((void)0)

#endif

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

// given a surface name, lets see if it's legal in the model
int G2_IsSurfaceLegal(const model_s *mod_m, const char *surfaceName, uint32_t *flags)
{
	assert(mod_m);
	assert(mod_m->mdxm);
	// damn include file dependancies
	mdxmSurfHierarchy_t	*surf;
	surf = (mdxmSurfHierarchy_t *) ( (byte *)mod_m->mdxm + mod_m->mdxm->ofsSurfHierarchy );

	for ( int i = 0 ; i < mod_m->mdxm->numSurfaces ; i++)
	{
	 	if (!Q_stricmp(surfaceName, surf->name))
	 	{
			*flags = surf->flags;
			return i;
		}
		// find the next surface
  		surf = (mdxmSurfHierarchy_t *)( (byte *)surf + (intptr_t)( &((mdxmSurfHierarchy_t *)0)->childIndexes[ surf->numChildren ] ));
	}
	return -1;
}

class CQuickOverride
{
	int mOverride[512];
	int mAt[512];
	int mCurrentTouch;
public:
	CQuickOverride()
	{
		mCurrentTouch=1;
		int i;
		for (i=0;i<512;i++)
		{
			mOverride[i]=0;
		}
	}
	void Invalidate()
	{
		mCurrentTouch++;
	}
	void Set(int index,int pos)
	{
		if (index==10000)
		{
			return;
		}
		assert(index>=0&&index<512);
		mOverride[index]=mCurrentTouch;
		mAt[index]=pos;
	}
	int Test(int index)
	{
		assert(index>=0&&index<512);
		if (mOverride[index]!=mCurrentTouch)
		{
			return -1;
		}
		else
		{
			return mAt[index];
		}
	}
};

static CQuickOverride QuickOverride;

// find a particular surface in the surface override list
const surfaceInfo_t *G2_FindOverrideSurface(int surfaceNum,const surfaceInfo_v &surfaceList)
{

	if (surfaceNum<0)
	{
		// starting a new lookup
		QuickOverride.Invalidate();
		for(size_t i=0; i<surfaceList.size(); i++)
		{
			if (surfaceList[i].surface>=0)
			{
				QuickOverride.Set(surfaceList[i].surface,i);
			}
		}
		return NULL;
	}
	int idx=QuickOverride.Test(surfaceNum);
	if (idx<0)
	{
		if (surfaceNum==10000)
		{
			for(size_t i=0; i<surfaceList.size(); i++)
			{
				if (surfaceList[i].surface == surfaceNum)
				{
					return &surfaceList[i];
				}
			}
		}
#if _DEBUG
		// look through entire list
		size_t i;
		for(i=0; i<surfaceList.size(); i++)
		{
			if (surfaceList[i].surface == surfaceNum)
			{
				break;
			}
		}
		// didn't find it.
		assert(i==surfaceList.size()); // our quickoverride is not working right
#endif
		return NULL;
	}
	assert(idx>=0&&idx<(int)surfaceList.size());
	assert(surfaceList[idx].surface == surfaceNum);
	return &surfaceList[idx];
}

// given a bone number, see if there is an override bone in the bone list
int	G2_Find_Bone_In_List(boneInfo_v &blist, const int boneNum)
{
	// look through entire list
	for(size_t i=0; i<blist.size(); i++)
	{
		if (blist[i].boneNumber == boneNum)
		{
			return i;
		}
	}
	return -1;
}

const mdxmSurface_t *G2_FindSurface(CGhoul2Info *ghlInfo, surfaceInfo_v &slist, const char *surfaceName,
							 int *surfIndex/*NULL*/)
{
	int						i = 0;
	// find the model we want
	assert(G2_MODEL_OK(ghlInfo));

	const mdxmHierarchyOffsets_t *surfIndexes = (mdxmHierarchyOffsets_t *)((byte *)ghlInfo->currentModel->mdxm + sizeof(mdxmHeader_t));

 	// first find if we already have this surface in the list
	for (i = slist.size() - 1; i >= 0; i--)
	{
		if ((slist[i].surface != 10000) && (slist[i].surface != -1))
		{
			const mdxmSurface_t	*surf = (mdxmSurface_t *)G2_FindSurface(ghlInfo->currentModel, slist[i].surface, 0);
			// back track and get the surfinfo struct for this surface
			const mdxmSurfHierarchy_t	*surfInfo = (mdxmSurfHierarchy_t *)((byte *)surfIndexes + surfIndexes->offsets[surf->thisSurfaceIndex]);

  			// are these the droids we're looking for?
			if (!Q_stricmp (surfInfo->name, surfaceName))
			{
				// yup
				if (surfIndex)
				{
					*surfIndex = i;
				}
				return surf;
			}
		}
	}
	// didn't find it
	if (surfIndex)
	{
		*surfIndex = -1;
	}
	return 0;
}

// go away and determine what the pointer for a specific surface definition within the model definition is
void *G2_FindSurface(const model_s *mod, int index, int lod)
{
	assert(mod);
	mdxmHeader_t *mdxm = G2ABI_GetMdxmByModel(mod);
	assert(mdxm);

	// point at first lod list
	byte	*current = (byte*)((intptr_t)mdxm + (intptr_t)mdxm->ofsLODs);
	int i;

	//walk the lods
	assert(lod>=0&&lod<mod->mdxm->numLODs);
	for (i=0; i<lod; i++)
	{
		mdxmLOD_t *lodData = (mdxmLOD_t *)current;
		current += lodData->ofsEnd;
	}

	// avoid the lod pointer data structure
	current += sizeof(mdxmLOD_t);

	mdxmLODSurfOffset_t *indexes = (mdxmLODSurfOffset_t *)current;
	// we are now looking at the offset array
	assert(index>=0&&index<mod->mdxm->numSurfaces);
	current += indexes->offsets[index];

	return (void *)current;
}

// set a named surface offFlags - if it doesn't find a surface with this name in the list then it will add one.
qboolean G2_SetSurfaceOnOff (CGhoul2Info *ghlInfo, const char *surfaceName, const int offFlags)
{
	int					surfIndex = -1;
	surfaceInfo_t		temp_slist_entry;

	// find the model we want
 	// first find if we already have this surface in the list
	const mdxmSurface_t *surf = G2_FindSurface(ghlInfo, ghlInfo->mSlist, surfaceName, &surfIndex);
	if (surf)
	{
		// set descendants value

		// slist[surfIndex].offFlags = offFlags;
		// seems to me that we shouldn't overwrite the other flags.
		// the only bit we really care about in the incoming flags is the off bit
		ghlInfo->mSlist[surfIndex].offFlags &= ~(G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS);
		ghlInfo->mSlist[surfIndex].offFlags |= offFlags & (G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS);
		return qtrue;
	}
	else
	{
		// ok, not in the list already - in that case, lets verify this surface exists in the model mesh
		uint32_t flags;
		int surfaceNum = G2_IsSurfaceLegal(ghlInfo->currentModel, surfaceName, &flags);
		if (surfaceNum != -1)
		{
			uint32_t newflags = flags;
			// the only bit we really care about in the incoming flags is the off bit
			newflags &= ~(G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS);
			newflags |= offFlags & (G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS);

			if (newflags != flags)
			{	// insert here then because it changed, no need to add an override otherwise
				temp_slist_entry.offFlags = newflags;
				temp_slist_entry.surface = surfaceNum;

				ghlInfo->mSlist.push_back(temp_slist_entry);
			}
			return qtrue;
		}
	}
	return qfalse;
}

qboolean G2_SetupModelPointers(CGhoul2Info *ghlInfo) // returns true if the model is properly set up
{
	G2ERROR(ghlInfo,"NULL ghlInfo");
	if (!ghlInfo)
	{
		return qfalse;
	}
	ghlInfo->mValid=false;
//	G2WARNING(ghlInfo->mModelindex != -1,"Setup request on non-used info slot?");
	if (ghlInfo->mModelindex != -1)
	{
		G2ERROR(ghlInfo->mFileName[0],"empty ghlInfo->mFileName");
		ghlInfo->mModel = RE_RegisterModel(ghlInfo->mFileName);
		ghlInfo->currentModel = R_GetModelByHandle(ghlInfo->mModel);
		G2ERROR(ghlInfo->currentModel,va("NULL Model (glm) %s",ghlInfo->mFileName));
		if (ghlInfo->currentModel)
		{
			G2ERROR(ghlInfo->currentModel->mdxm,va("Model has no mdxm (glm) %s",ghlInfo->mFileName));
			if (ghlInfo->currentModel->mdxm)
			{
				if (ghlInfo->currentModelSize)
				{
					if (ghlInfo->currentModelSize!=ghlInfo->currentModel->mdxm->ofsEnd)
					{
						Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
					}
				}
				ghlInfo->currentModelSize=ghlInfo->currentModel->mdxm->ofsEnd;
				G2ERROR(ghlInfo->currentModelSize,va("Zero sized Model? (glm) %s",ghlInfo->mFileName));

				ghlInfo->animModel =  R_GetModelByHandle(ghlInfo->currentModel->mdxm->animIndex + ghlInfo->animModelIndexOffset);
				G2ERROR(ghlInfo->animModel,va("NULL Model (gla) %s",ghlInfo->mFileName));
				if (ghlInfo->animModel)
				{
					ghlInfo->aHeader =ghlInfo->animModel->mdxa;
					G2ERROR(ghlInfo->aHeader,va("Model has no mdxa (gla) %s",ghlInfo->mFileName));
					if (!ghlInfo->aHeader)
					{
						Com_Error(ERR_DROP, "Ghoul2 Model has no mdxa (gla) %s",ghlInfo->mFileName);
					}
					if (ghlInfo->currentAnimModelSize)
					{
						if (ghlInfo->currentAnimModelSize!=ghlInfo->aHeader->ofsEnd)
						{
							Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
						}
					}
					ghlInfo->currentAnimModelSize=ghlInfo->aHeader->ofsEnd;
					G2ERROR(ghlInfo->currentAnimModelSize,va("Zero sized Model? (gla) %s",ghlInfo->mFileName));
					ghlInfo->mValid=true;
				}
			}
		}
	}
	if (!ghlInfo->mValid)
	{
		ghlInfo->currentModel=0;
		ghlInfo->currentModelSize=0;
		ghlInfo->animModel=0;
		ghlInfo->currentAnimModelSize=0;
		ghlInfo->aHeader=0;
	}
	return (qboolean)ghlInfo->mValid;
}

qboolean G2_SetupModelPointers(CGhoul2Info_v &ghoul2) // returns true if any model is properly set up
{
	qboolean ret=qfalse;
	int i;
	for (i=0; i<ghoul2.size(); i++)
	{
		qboolean r=G2_SetupModelPointers(&ghoul2[i]);
		ret=(qboolean)(ret||r);
	}
	return ret;
}

bool G2_TestModelPointers(CGhoul2Info *ghlInfo) // returns true if the model is properly set up
{
	G2ERROR(ghlInfo,"NULL ghlInfo");
	if (!ghlInfo)
	{
		return false;
	}
	ghlInfo->mValid=false;
	if (ghlInfo->mModelindex != -1)
	{
		ghlInfo->mModel = RE_RegisterModel(ghlInfo->mFileName);
		ghlInfo->currentModel = R_GetModelByHandle(ghlInfo->mModel);
		if (ghlInfo->currentModel)
		{
			if (ghlInfo->currentModel->mdxm)
			{
				if (ghlInfo->currentModelSize)
				{
					if (ghlInfo->currentModelSize!=ghlInfo->currentModel->mdxm->ofsEnd)
					{
						Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
					}
				}
				ghlInfo->currentModelSize=ghlInfo->currentModel->mdxm->ofsEnd;
				ghlInfo->animModel =  R_GetModelByHandle(ghlInfo->currentModel->mdxm->animIndex + ghlInfo->animModelIndexOffset);
				if (ghlInfo->animModel)
				{
					ghlInfo->aHeader =ghlInfo->animModel->mdxa;
					G2ERROR(ghlInfo->aHeader,va("Model has no mdxa (gla) %s",ghlInfo->mFileName));
					if (!ghlInfo->aHeader)
					{
						Com_Error(ERR_DROP, "Ghoul2 Model has no mdxa (gla) %s",ghlInfo->mFileName);
					}
					if (ghlInfo->currentAnimModelSize)
					{
						if (ghlInfo->currentAnimModelSize!=ghlInfo->aHeader->ofsEnd)
						{
							Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
						}
					}
					ghlInfo->currentAnimModelSize=ghlInfo->aHeader->ofsEnd;
					ghlInfo->mValid=true;
				}
			}
		}
	}
	if (!ghlInfo->mValid)
	{
		ghlInfo->currentModel=0;
		ghlInfo->currentModelSize=0;
		ghlInfo->animModel=0;
		ghlInfo->currentAnimModelSize=0;
		ghlInfo->aHeader=0;
	}
	return ghlInfo->mValid;
}

// create a matrix using a set of angles
void Create_Matrix(const float *angle, mdxaBone_t *matrix)
{
	vec3_t		axis[3];

	// convert angles to axis
	AnglesToAxis( angle, axis );
	matrix->matrix[0][0] = axis[0][0];
	matrix->matrix[1][0] = axis[0][1];
	matrix->matrix[2][0] = axis[0][2];

	matrix->matrix[0][1] = axis[1][0];
	matrix->matrix[1][1] = axis[1][1];
	matrix->matrix[2][1] = axis[1][2];

	matrix->matrix[0][2] = axis[2][0];
	matrix->matrix[1][2] = axis[2][1];
	matrix->matrix[2][2] = axis[2][2];

	matrix->matrix[0][3] = 0;
	matrix->matrix[1][3] = 0;
	matrix->matrix[2][3] = 0;


}

// given a matrix, generate the inverse of that matrix
void Inverse_Matrix(mdxaBone_t *src, mdxaBone_t *dest)
{
	int i, j;

    for (i = 0; i < 3; i++)
	{
        for (j = 0; j < 3; j++)
		{
            dest->matrix[i][j]=src->matrix[j][i];
		}
	}
    for (i = 0; i < 3; i++)
	{
        dest->matrix[i][3]=0;
        for (j = 0; j < 3; j++)
		{
            dest->matrix[i][3]-=dest->matrix[i][j]*src->matrix[j][3];
		}
	}
}

extern mdxaBone_t		worldMatrix;
extern mdxaBone_t		worldMatrixInv;

// generate the world matrix for a given set of angles and origin - called from lots of places
void G2_GenerateWorldMatrix(const vec3_t angles, const vec3_t origin)
{
	Create_Matrix(angles, &worldMatrix);
	worldMatrix.matrix[0][3] = origin[0];
	worldMatrix.matrix[1][3] = origin[1];
	worldMatrix.matrix[2][3] = origin[2];

	Inverse_Matrix(&worldMatrix, &worldMatrixInv);
}