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
