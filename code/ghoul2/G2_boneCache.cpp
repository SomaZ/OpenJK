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

#include "../server/exe_headers.h"

#include "../client/client.h"	//FIXME!! EVIL - just include the definitions needed
#include "../client/vmachine.h"

#include "../rd-common/tr_common.h"

#include "qcommon/matcomp.h"
#if !defined(_QCOMMON_H_)
	#include "../qcommon/qcommon.h"
#endif
#if !defined(G2_H_INC)
    #include "G2.h"
#endif

#ifdef _G2_GORE
#include "ghoul2_gore.h"
#endif

#define	LL(x) x=LittleLong(x)
#define	LS(x) x=LittleShort(x)
#define	LF(x) x=LittleFloat(x)

#ifdef G2_PERFORMANCE_ANALYSIS
#include "../qcommon/timing.h"
timing_c G2PerformanceTimer_RB_SurfaceGhoul;

int G2PerformanceCounter_G2_TransformGhoulBones = 0;

int G2Time_RB_SurfaceGhoul = 0;

void G2Time_ResetTimers(void)
{
	G2Time_RB_SurfaceGhoul = 0;
	G2PerformanceCounter_G2_TransformGhoulBones = 0;
}

void G2Time_ReportTimers(void)
{
	Com_Printf("\n---------------------------------\nRB_SurfaceGhoul: %i\nTransformGhoulBones calls: %i\n---------------------------------\n\n",
		G2Time_RB_SurfaceGhoul,
		G2PerformanceCounter_G2_TransformGhoulBones
	);
}
#endif

//rww - RAGDOLL_BEGIN
#include <float.h>
//rww - RAGDOLL_END

extern	cvar_t	*r_Ghoul2UnSqash;
extern	cvar_t	*r_Ghoul2AnimSmooth;
extern	cvar_t	*r_Ghoul2NoLerp;
extern	cvar_t	*r_Ghoul2NoBlend;
extern	cvar_t	*r_Ghoul2UnSqashAfterSmooth;

bool HackadelicOnClient=false; // means this is a render traversal

// I hate doing this, but this is the simplest way to get this into the routines it needs to be
mdxaBone_t		worldMatrix;
mdxaBone_t		worldMatrixInv;
#ifdef _G2_GORE
qhandle_t		goreShader=-1;
#endif

const static mdxaBone_t		identityMatrix =
{
	{
		{ 0.0f, -1.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f }
	}
};

class CTransformBone
{
public:
	//rww - RAGDOLL_BEGIN
	int				touchRender;
	//rww - RAGDOLL_END

	mdxaBone_t		boneMatrix; //final matrix
	int				parent; // only set once
	int				touch; // for minimal recalculation
	CTransformBone()
	{
		touch=0;

	//rww - RAGDOLL_BEGIN
		touchRender = 0;
	//rww - RAGDOLL_END
	}

};

struct SBoneCalc
{
	int				newFrame;
	int				currentFrame;
	float			backlerp;
	float			blendFrame;
	int				blendOldFrame;
	bool			blendMode;
	float			blendLerp;
};

class CBoneCache;
void G2_TransformBone(int index,CBoneCache &CB);

class CBoneCache
{
	void EvalLow(int index)
	{
		assert(index>=0&&index<mNumBones);
		if (mFinalBones[index].touch!=mCurrentTouch)
		{
			// need to evaluate the bone
			assert((mFinalBones[index].parent>=0&&mFinalBones[index].parent<mNumBones)||(index==0&&mFinalBones[index].parent==-1));
			if (mFinalBones[index].parent>=0)
			{
				EvalLow(mFinalBones[index].parent); // make sure parent is evaluated
				SBoneCalc &par=mBones[mFinalBones[index].parent];
				mBones[index].newFrame=par.newFrame;
				mBones[index].currentFrame=par.currentFrame;
				mBones[index].backlerp=par.backlerp;
				mBones[index].blendFrame=par.blendFrame;
				mBones[index].blendOldFrame=par.blendOldFrame;
				mBones[index].blendMode=par.blendMode;
				mBones[index].blendLerp=par.blendLerp;
			}
			G2_TransformBone(index,*this);
			mFinalBones[index].touch=mCurrentTouch;
		}
	}

//rww - RAGDOLL_BEGIN
	void SmoothLow(int index)
	{
		if (mSmoothBones[index].touch==mLastTouch)
		{
			int i;
			float *oldM=&mSmoothBones[index].boneMatrix.matrix[0][0];
			float *newM=&mFinalBones[index].boneMatrix.matrix[0][0];
			for (i=0;i<12;i++,oldM++,newM++)
			{
				*oldM=mSmoothFactor*(*oldM-*newM)+*newM;
			}
		}
		else
		{
			memcpy(&mSmoothBones[index].boneMatrix,&mFinalBones[index].boneMatrix,sizeof(mdxaBone_t));
		}
		mdxaSkelOffsets_t *offsets = (mdxaSkelOffsets_t *)((byte *)header + sizeof(mdxaHeader_t));
		mdxaSkel_t *skel = (mdxaSkel_t *)((byte *)header + sizeof(mdxaHeader_t) + offsets->offsets[index]);
		mdxaBone_t tempMatrix;
		Multiply_3x4Matrix(&tempMatrix,&mSmoothBones[index].boneMatrix, &skel->BasePoseMat);
		float maxl;
		maxl=VectorLength(&skel->BasePoseMat.matrix[0][0]);
		VectorNormalize(&tempMatrix.matrix[0][0]);
		VectorNormalize(&tempMatrix.matrix[1][0]);
		VectorNormalize(&tempMatrix.matrix[2][0]);

		VectorScale(&tempMatrix.matrix[0][0],maxl,&tempMatrix.matrix[0][0]);
		VectorScale(&tempMatrix.matrix[1][0],maxl,&tempMatrix.matrix[1][0]);
		VectorScale(&tempMatrix.matrix[2][0],maxl,&tempMatrix.matrix[2][0]);
		Multiply_3x4Matrix(&mSmoothBones[index].boneMatrix,&tempMatrix,&skel->BasePoseMatInv);
		// Added by BTO (VV) - I hope this is right.
		mSmoothBones[index].touch=mCurrentTouch;
#ifdef _DEBUG
		for ( int i = 0; i < 3; i++ )
		{
			for ( int j = 0; j < 4; j++ )
			{
				assert( !Q_isnan(mSmoothBones[index].boneMatrix.matrix[i][j]));
			}
		}
#endif// _DEBUG
	}
//rww - RAGDOLL_END

public:
	int					frameSize;
	const mdxaHeader_t	*header;
	const model_s		*mod;

	// these are split for better cpu cache behavior
	SBoneCalc *mBones;
	CTransformBone *mFinalBones;

	CTransformBone *mSmoothBones; // for render smoothing
	mdxaSkel_t **mSkels;

	int				mNumBones;

	boneInfo_v		*rootBoneList;
	mdxaBone_t		rootMatrix;
	int				incomingTime;

	int				mCurrentTouch;

	//rww - RAGDOLL_BEGIN
	int				mCurrentTouchRender;
	int				mLastTouch;
	int				mLastLastTouch;
	//rww - RAGDOLL_END

	// for render smoothing
	bool			mSmoothingActive;
	bool			mUnsquash;
	float			mSmoothFactor;
//	int				mWraithID; // this is just used for debug prints, can use it for any int of interest in JK2

	CBoneCache(const model_s *amod,const mdxaHeader_t *aheader) :
		header(aheader),
		mod(amod)
	{
		assert(amod);
		assert(aheader);
		mSmoothingActive=false;
		mUnsquash=false;
		mSmoothFactor=0.0f;

		mNumBones = header->numBones;
		mBones = new SBoneCalc[mNumBones];
		mFinalBones = (CTransformBone*) Z_Malloc(sizeof(CTransformBone) * mNumBones, TAG_GHOUL2, qtrue);
		mSmoothBones = (CTransformBone*) Z_Malloc(sizeof(CTransformBone) * mNumBones, TAG_GHOUL2, qtrue);
		mSkels = new mdxaSkel_t*[mNumBones];
		mdxaSkelOffsets_t *offsets;
		mdxaSkel_t		*skel;
		offsets = (mdxaSkelOffsets_t *)((byte *)header + sizeof(mdxaHeader_t));

		int i;
		for (i=0;i<mNumBones;i++)
		{
			skel = (mdxaSkel_t *)((byte *)header + sizeof(mdxaHeader_t) + offsets->offsets[i]);
			mSkels[i]=skel;
			mFinalBones[i].parent=skel->parent;
		}
		mCurrentTouch=3;

//rww - RAGDOLL_BEGIN
		mLastTouch=2;
		mLastLastTouch=1;
//rww - RAGDOLL_END
	}
	~CBoneCache ()
	{
		delete [] mBones;
		// Alignment
		Z_Free(mFinalBones);
		Z_Free(mSmoothBones);
		delete [] mSkels;
	}

	SBoneCalc &Root()
	{
		assert(mNumBones);
		return mBones[0];
	}
	const mdxaBone_t &EvalUnsmooth(int index)
	{
		EvalLow(index);
		if (mSmoothingActive&&mSmoothBones[index].touch)
		{
			return mSmoothBones[index].boneMatrix;
		}
		return mFinalBones[index].boneMatrix;
	}
	const mdxaBone_t &Eval(int index)
	{
		/*
		bool wasEval=EvalLow(index);
		if (mSmoothingActive)
		{
			if (mSmoothBones[index].touch!=incomingTime||wasEval)
			{
				float dif=float(incomingTime)-float(mSmoothBones[index].touch);
				if (mSmoothBones[index].touch&&dif<300.0f)
				{

					if (dif<16.0f)  // 60 fps
					{
						dif=16.0f;
					}
					if (dif>100.0f) // 10 fps
					{
						dif=100.0f;
					}
					float f=1.0f-pow(1.0f-mSmoothFactor,16.0f/dif);

					int i;
					float *oldM=&mSmoothBones[index].boneMatrix.matrix[0][0];
					float *newM=&mFinalBones[index].boneMatrix.matrix[0][0];
					for (i=0;i<12;i++,oldM++,newM++)
					{
						*oldM=f*(*oldM-*newM)+*newM;
					}
					if (mUnsquash)
					{
						mdxaBone_t tempMatrix;
						Multiply_3x4Matrix(&tempMatrix,&mSmoothBones[index].boneMatrix, &mSkels[index]->BasePoseMat);
						float maxl;
						maxl=VectorLength(&mSkels[index]->BasePoseMat.matrix[0][0]);
						VectorNormalizeFast(&tempMatrix.matrix[0][0]);
						VectorNormalizeFast(&tempMatrix.matrix[1][0]);
						VectorNormalizeFast(&tempMatrix.matrix[2][0]);

						VectorScale(&tempMatrix.matrix[0][0],maxl,&tempMatrix.matrix[0][0]);
						VectorScale(&tempMatrix.matrix[1][0],maxl,&tempMatrix.matrix[1][0]);
						VectorScale(&tempMatrix.matrix[2][0],maxl,&tempMatrix.matrix[2][0]);
						Multiply_3x4Matrix(&mSmoothBones[index].boneMatrix,&tempMatrix,&mSkels[index]->BasePoseMatInv);
					}
				}
				else
				{
					memcpy(&mSmoothBones[index].boneMatrix,&mFinalBones[index].boneMatrix,sizeof(mdxaBone_t));
				}
				mSmoothBones[index].touch=incomingTime;
			}
			return mSmoothBones[index].boneMatrix;
		}
		return mFinalBones[index].boneMatrix;
		*/
		//all above is not necessary, smoothing is taken care of when we want to use smoothlow (only when evalrender)
		assert(index>=0&&index<mNumBones);
		if (mFinalBones[index].touch!=mCurrentTouch)
		{
			EvalLow(index);
		}
		return mFinalBones[index].boneMatrix;
	}

	//rww - RAGDOLL_BEGIN
	const inline mdxaBone_t &EvalRender(int index)
	{
		assert(index>=0&&index<mNumBones);
		if (mFinalBones[index].touch!=mCurrentTouch)
		{
			mFinalBones[index].touchRender=mCurrentTouchRender;
			EvalLow(index);
		}
		if (mSmoothingActive)
		{
			if (mSmoothBones[index].touch!=mCurrentTouch)
			{
				SmoothLow(index);
			}
			return mSmoothBones[index].boneMatrix;
		}
		return mFinalBones[index].boneMatrix;
	}
	//rww - RAGDOLL_END
	//rww - RAGDOLL_BEGIN
	bool WasRendered(int index)
	{
		assert(index>=0&&index<mNumBones);
		return mFinalBones[index].touchRender==mCurrentTouchRender;
	}
	int GetParent(int index)
	{
		if (index==0)
		{
			return -1;
		}
		assert(index>=0&&index<mNumBones);
		return mFinalBones[index].parent;
	}
	//rww - RAGDOLL_END

	// Added by BTO (VV) - This is probably broken
	// Need to add in smoothing step?
	CTransformBone *EvalFull(int index)
	{
#ifdef JK2_MODE
//		Eval(index);

// FIXME BBi Was commented
		Eval(index);
#else
		EvalRender(index);
#endif // JK2_MODE
		if (mSmoothingActive)
		{
			return mSmoothBones + index;
		}
		return mFinalBones + index;
	}
};

static inline float G2_GetVertBoneWeightNotSlow( const mdxmVertex_t *pVert, const int iWeightNum)
{
	float fBoneWeight;

	int iTemp = pVert->BoneWeightings[iWeightNum];

	iTemp|= (pVert->uiNmWeightsAndBoneIndexes >> (iG2_BONEWEIGHT_TOPBITS_SHIFT+(iWeightNum*2)) ) & iG2_BONEWEIGHT_TOPBITS_AND;

	fBoneWeight = fG2_BONEWEIGHT_RECIPROCAL_MULT * iTemp;

	return fBoneWeight;
}

//rww - RAGDOLL_BEGIN
const mdxaHeader_t *G2_GetModA(CGhoul2Info &ghoul2)
{
	if (!ghoul2.mBoneCache)
	{
		return 0;
	}

	CBoneCache &boneCache=*ghoul2.mBoneCache;
	return boneCache.header;
}

int G2_GetBoneDependents(CGhoul2Info &ghoul2,int boneNum,int *tempDependents,int maxDep)
{
	// fixme, these should be precomputed
	if (!ghoul2.mBoneCache||!maxDep)
	{
		return 0;
	}

	CBoneCache &boneCache=*ghoul2.mBoneCache;
	mdxaSkel_t		*skel;
	mdxaSkelOffsets_t *offsets;
	offsets = (mdxaSkelOffsets_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t));
	skel = (mdxaSkel_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[boneNum]);
	int i;
	int ret=0;
	for (i=0;i<skel->numChildren;i++)
	{
		if (!maxDep)
		{
			return i; // number added
		}
		*tempDependents=skel->children[i];
		assert(*tempDependents>0&&*tempDependents<boneCache.header->numBones);
		maxDep--;
		tempDependents++;
		ret++;
	}
	for (i=0;i<skel->numChildren;i++)
	{
		int num=G2_GetBoneDependents(ghoul2,skel->children[i],tempDependents,maxDep);
		tempDependents+=num;
		ret+=num;
		maxDep-=num;
		assert(maxDep>=0);
		if (!maxDep)
		{
			break;
		}
	}
	return ret;
}

bool G2_WasBoneRendered(CGhoul2Info &ghoul2,int boneNum)
{
	if (!ghoul2.mBoneCache)
	{
		return false;
	}
	CBoneCache &boneCache=*ghoul2.mBoneCache;

	return boneCache.WasRendered(boneNum);
}

void G2_GetBoneBasepose(CGhoul2Info &ghoul2,int boneNum,mdxaBone_t *&retBasepose,mdxaBone_t *&retBaseposeInv)
{
	if (!ghoul2.mBoneCache)
	{
		// yikes
		retBasepose=const_cast<mdxaBone_t *>(&identityMatrix);
		retBaseposeInv=const_cast<mdxaBone_t *>(&identityMatrix);
		return;
	}
	assert(ghoul2.mBoneCache);
	CBoneCache &boneCache=*ghoul2.mBoneCache;
	assert(boneCache.mod);
	assert(boneNum>=0&&boneNum<boneCache.header->numBones);

	mdxaSkel_t		*skel;
	mdxaSkelOffsets_t *offsets;
	offsets = (mdxaSkelOffsets_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t));
	skel = (mdxaSkel_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[boneNum]);
	retBasepose=&skel->BasePoseMat;
	retBaseposeInv=&skel->BasePoseMatInv;
}

char *G2_GetBoneNameFromSkel(CGhoul2Info &ghoul2, int boneNum)
{
	if (!ghoul2.mBoneCache)
	{
		return NULL;
	}
	CBoneCache &boneCache=*ghoul2.mBoneCache;
	assert(boneCache.mod);
	assert(boneNum>=0&&boneNum<boneCache.header->numBones);

	mdxaSkel_t		*skel;
	mdxaSkelOffsets_t *offsets;
	offsets = (mdxaSkelOffsets_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t));
	skel = (mdxaSkel_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[boneNum]);

	return skel->name;
}

void G2_RagGetBoneBasePoseMatrixLow(CGhoul2Info &ghoul2, int boneNum, mdxaBone_t &boneMatrix, mdxaBone_t &retMatrix, vec3_t scale)
{
	assert(ghoul2.mBoneCache);
	CBoneCache &boneCache=*ghoul2.mBoneCache;
	assert(boneCache.mod);
	assert(boneNum>=0&&boneNum<boneCache.header->numBones);

	mdxaSkel_t		*skel;
	mdxaSkelOffsets_t *offsets;
	offsets = (mdxaSkelOffsets_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t));
	skel = (mdxaSkel_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[boneNum]);
	Multiply_3x4Matrix(&retMatrix, &boneMatrix, &skel->BasePoseMat);

	if (scale[0])
	{
		retMatrix.matrix[0][3] *= scale[0];
	}
	if (scale[1])
	{
		retMatrix.matrix[1][3] *= scale[1];
	}
	if (scale[2])
	{
		retMatrix.matrix[2][3] *= scale[2];
	}

	VectorNormalize((float*)&retMatrix.matrix[0]);
	VectorNormalize((float*)&retMatrix.matrix[1]);
	VectorNormalize((float*)&retMatrix.matrix[2]);
}

void G2_GetBoneMatrixLow(CGhoul2Info &ghoul2,int boneNum,const vec3_t scale,mdxaBone_t &retMatrix,mdxaBone_t *&retBasepose,mdxaBone_t *&retBaseposeInv)
{
	if (!ghoul2.mBoneCache)
	{
		retMatrix=identityMatrix;
		// yikes
		retBasepose=const_cast<mdxaBone_t *>(&identityMatrix);
		retBaseposeInv=const_cast<mdxaBone_t *>(&identityMatrix);
		return;
	}
	mdxaBone_t bolt;
	assert(ghoul2.mBoneCache);
	CBoneCache &boneCache=*ghoul2.mBoneCache;
	assert(boneCache.mod);
	assert(boneNum>=0&&boneNum<boneCache.header->numBones);

	mdxaSkel_t		*skel;
	mdxaSkelOffsets_t *offsets;
	offsets = (mdxaSkelOffsets_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t));
	skel = (mdxaSkel_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[boneNum]);
	Multiply_3x4Matrix(&bolt, &boneCache.Eval(boneNum), &skel->BasePoseMat); // DEST FIRST ARG
	retBasepose=&skel->BasePoseMat;
	retBaseposeInv=&skel->BasePoseMatInv;

	if (scale[0])
	{
		bolt.matrix[0][3] *= scale[0];
	}
	if (scale[1])
	{
		bolt.matrix[1][3] *= scale[1];
	}
	if (scale[2])
	{
		bolt.matrix[2][3] *= scale[2];
	}
	VectorNormalize((float*)&bolt.matrix[0]);
	VectorNormalize((float*)&bolt.matrix[1]);
	VectorNormalize((float*)&bolt.matrix[2]);

	Multiply_3x4Matrix(&retMatrix,&worldMatrix, &bolt);

#ifdef _DEBUG
	for ( int i = 0; i < 3; i++ )
	{
		for ( int j = 0; j < 4; j++ )
		{
			assert( !Q_isnan(retMatrix.matrix[i][j]));
		}
	}
#endif// _DEBUG
}

int G2_GetParentBoneMatrixLow(CGhoul2Info &ghoul2,int boneNum,const vec3_t scale,mdxaBone_t &retMatrix,mdxaBone_t *&retBasepose,mdxaBone_t *&retBaseposeInv)
{
	int parent=-1;
	if (ghoul2.mBoneCache)
	{
		CBoneCache &boneCache=*ghoul2.mBoneCache;
		assert(boneCache.mod);
		assert(boneNum>=0&&boneNum<boneCache.header->numBones);
		parent=boneCache.GetParent(boneNum);
		if (parent<0||parent>=boneCache.header->numBones)
		{
			parent=-1;
			retMatrix=identityMatrix;
			// yikes
			retBasepose=const_cast<mdxaBone_t *>(&identityMatrix);
			retBaseposeInv=const_cast<mdxaBone_t *>(&identityMatrix);
		}
		else
		{
			G2_GetBoneMatrixLow(ghoul2,parent,scale,retMatrix,retBasepose,retBaseposeInv);
		}
	}
	return parent;
}
//rww - RAGDOLL_END

void RemoveBoneCache(CBoneCache *boneCache)
{
	delete boneCache;
}

const mdxaBone_t &EvalBoneCache(int index,CBoneCache *boneCache)
{
	assert(boneCache);
	return boneCache->Eval(index);
}

void Multiply_3x4Matrix(mdxaBone_t *out,const  mdxaBone_t *in2,const mdxaBone_t *in)
{
	// first row of out
	out->matrix[0][0] = (in2->matrix[0][0] * in->matrix[0][0]) + (in2->matrix[0][1] * in->matrix[1][0]) + (in2->matrix[0][2] * in->matrix[2][0]);
	out->matrix[0][1] = (in2->matrix[0][0] * in->matrix[0][1]) + (in2->matrix[0][1] * in->matrix[1][1]) + (in2->matrix[0][2] * in->matrix[2][1]);
	out->matrix[0][2] = (in2->matrix[0][0] * in->matrix[0][2]) + (in2->matrix[0][1] * in->matrix[1][2]) + (in2->matrix[0][2] * in->matrix[2][2]);
	out->matrix[0][3] = (in2->matrix[0][0] * in->matrix[0][3]) + (in2->matrix[0][1] * in->matrix[1][3]) + (in2->matrix[0][2] * in->matrix[2][3]) + in2->matrix[0][3];
	// second row of outf out
	out->matrix[1][0] = (in2->matrix[1][0] * in->matrix[0][0]) + (in2->matrix[1][1] * in->matrix[1][0]) + (in2->matrix[1][2] * in->matrix[2][0]);
	out->matrix[1][1] = (in2->matrix[1][0] * in->matrix[0][1]) + (in2->matrix[1][1] * in->matrix[1][1]) + (in2->matrix[1][2] * in->matrix[2][1]);
	out->matrix[1][2] = (in2->matrix[1][0] * in->matrix[0][2]) + (in2->matrix[1][1] * in->matrix[1][2]) + (in2->matrix[1][2] * in->matrix[2][2]);
	out->matrix[1][3] = (in2->matrix[1][0] * in->matrix[0][3]) + (in2->matrix[1][1] * in->matrix[1][3]) + (in2->matrix[1][2] * in->matrix[2][3]) + in2->matrix[1][3];
	// third row of out  out
	out->matrix[2][0] = (in2->matrix[2][0] * in->matrix[0][0]) + (in2->matrix[2][1] * in->matrix[1][0]) + (in2->matrix[2][2] * in->matrix[2][0]);
	out->matrix[2][1] = (in2->matrix[2][0] * in->matrix[0][1]) + (in2->matrix[2][1] * in->matrix[1][1]) + (in2->matrix[2][2] * in->matrix[2][1]);
	out->matrix[2][2] = (in2->matrix[2][0] * in->matrix[0][2]) + (in2->matrix[2][1] * in->matrix[1][2]) + (in2->matrix[2][2] * in->matrix[2][2]);
	out->matrix[2][3] = (in2->matrix[2][0] * in->matrix[0][3]) + (in2->matrix[2][1] * in->matrix[1][3]) + (in2->matrix[2][2] * in->matrix[2][3]) + in2->matrix[2][3];
}

static int G2_GetBonePoolIndex(const mdxaHeader_t *pMDXAHeader, int iFrame, int iBone)
{
	assert(iFrame>=0&&iFrame<pMDXAHeader->numFrames);
	assert(iBone>=0&&iBone<pMDXAHeader->numBones);

	const int iOffsetToIndex	= (iFrame * pMDXAHeader->numBones * 3) + (iBone * 3);
	mdxaIndex_t *pIndex			= (mdxaIndex_t *)((byte*)pMDXAHeader + pMDXAHeader->ofsFrames + iOffsetToIndex);

	return (pIndex->iIndex[2] << 16) + (pIndex->iIndex[1] << 8) + (pIndex->iIndex[0]);
}

/*static inline*/ void UnCompressBone(float mat[3][4], int iBoneIndex, const mdxaHeader_t *pMDXAHeader, int iFrame)
{
	mdxaCompQuatBone_t *pCompBonePool = (mdxaCompQuatBone_t *) ((byte *)pMDXAHeader + pMDXAHeader->ofsCompBonePool);
	MC_UnCompressQuat(mat, pCompBonePool[ G2_GetBonePoolIndex( pMDXAHeader, iFrame, iBoneIndex ) ].Comp);
}



#define DEBUG_G2_TIMING (0)
#define DEBUG_G2_TIMING_RENDER_ONLY (1)

void G2_TimingModel(boneInfo_t &bone,int currentTime,int numFramesInFile,int &currentFrame,int &newFrame,float &lerp)
{
	assert(bone.startFrame>=0);
	assert(bone.startFrame<=numFramesInFile);
	assert(bone.endFrame>=0);
	assert(bone.endFrame<=numFramesInFile);

	// yes - add in animation speed to current frame
	float	animSpeed = bone.animSpeed;
	float	time;
	if (bone.pauseTime)
	{
		time = (bone.pauseTime - bone.startTime) / 50.0f;
	}
	else
	{
		time = (currentTime - bone.startTime) / 50.0f;
	}
	if (time<0.0f)
	{
		time=0.0f;
	}
	float	newFrame_g = bone.startFrame + (time * animSpeed);

	int		animSize = bone.endFrame - bone.startFrame;
	float	endFrame = (float)bone.endFrame;
	// we are supposed to be animating right?
	if (animSize)
	{
		// did we run off the end?
		if (((animSpeed > 0.0f) && (newFrame_g > endFrame - 1)) ||
			((animSpeed < 0.0f) && (newFrame_g < endFrame+1)))
		{
			// yep - decide what to do
			if (bone.flags & BONE_ANIM_OVERRIDE_LOOP)
			{
				// get our new animation frame back within the bounds of the animation set
				if (animSpeed < 0.0f)
				{
					// we don't use this case, or so I am told
					// if we do, let me know, I need to insure the mod works

					// should we be creating a virtual frame?
					if ((newFrame_g < endFrame+1) && (newFrame_g >= endFrame))
					{
						// now figure out what we are lerping between
						// delta is the fraction between this frame and the next, since the new anim is always at a .0f;
						lerp = float(endFrame+1)-newFrame_g;
						// frames are easy to calculate
						currentFrame = endFrame;
						assert(currentFrame>=0&&currentFrame<numFramesInFile);
						newFrame = bone.startFrame;
						assert(newFrame>=0&&newFrame<numFramesInFile);
					}
					else
					{
						if (newFrame_g <= endFrame+1)
						{
							newFrame_g=endFrame+fmod(newFrame_g-endFrame,animSize)-animSize;
						}
						// now figure out what we are lerping between
						// delta is the fraction between this frame and the next, since the new anim is always at a .0f;
						lerp = (ceil(newFrame_g)-newFrame_g);
						// frames are easy to calculate
						currentFrame = ceil(newFrame_g);
						assert(currentFrame>=0&&currentFrame<numFramesInFile);
						// should we be creating a virtual frame?
						if (currentFrame <= endFrame+1 )
						{
							newFrame = bone.startFrame;
							assert(newFrame>=0&&newFrame<numFramesInFile);
						}
						else
						{
							newFrame = currentFrame - 1;
							assert(newFrame>=0&&newFrame<numFramesInFile);
						}
					}
				}
				else
				{
					// should we be creating a virtual frame?
					if ((newFrame_g > endFrame - 1) && (newFrame_g < endFrame))
					{
						// now figure out what we are lerping between
						// delta is the fraction between this frame and the next, since the new anim is always at a .0f;
						lerp = (newFrame_g - (int)newFrame_g);
						// frames are easy to calculate
						currentFrame = (int)newFrame_g;
						assert(currentFrame>=0&&currentFrame<numFramesInFile);
						newFrame = bone.startFrame;
						assert(newFrame>=0&&newFrame<numFramesInFile);
					}
					else
					{
						if (newFrame_g >= endFrame)
						{
							newFrame_g=endFrame+fmod(newFrame_g-endFrame,animSize)-animSize;
						}
						// now figure out what we are lerping between
						// delta is the fraction between this frame and the next, since the new anim is always at a .0f;
						lerp = (newFrame_g - (int)newFrame_g);
						// frames are easy to calculate
						currentFrame = (int)newFrame_g;
						assert(currentFrame>=0&&currentFrame<numFramesInFile);
						// should we be creating a virtual frame?
						if (newFrame_g >= endFrame - 1)
						{
							newFrame = bone.startFrame;
							assert(newFrame>=0&&newFrame<numFramesInFile);
						}
						else
						{
							newFrame = currentFrame + 1;
							assert(newFrame>=0&&newFrame<numFramesInFile);
						}
					}
				}
				// sanity check
				assert (((newFrame < endFrame) && (newFrame >= bone.startFrame)) || (animSize < 10));
			}
			else
			{
				if (((bone.flags & (BONE_ANIM_OVERRIDE_FREEZE)) == (BONE_ANIM_OVERRIDE_FREEZE)))
				{
					// if we are supposed to reset the default anim, then do so
					if (animSpeed > 0.0f)
					{
						currentFrame = bone.endFrame - 1;
						assert(currentFrame>=0&&currentFrame<numFramesInFile);
					}
					else
					{
						currentFrame = bone.endFrame+1;
						assert(currentFrame>=0&&currentFrame<numFramesInFile);
					}

					newFrame = currentFrame;
					assert(newFrame>=0&&newFrame<numFramesInFile);
					lerp = 0;
				}
				else
				{
					bone.flags &= ~(BONE_ANIM_TOTAL);
				}

			}
		}
		else
		{
			if (animSpeed> 0.0)
			{
				// frames are easy to calculate
				currentFrame = (int)newFrame_g;

				// figure out the difference between the two frames	- we have to decide what frame and what percentage of that
				// frame we want to display
				lerp = (newFrame_g - currentFrame);

				assert(currentFrame>=0&&currentFrame<numFramesInFile);

				newFrame = currentFrame + 1;
				// are we now on the end frame?
				assert((int)endFrame<=numFramesInFile);
				if (newFrame >= (int)endFrame)
				{
					// we only want to lerp with the first frame of the anim if we are looping
					if (bone.flags & BONE_ANIM_OVERRIDE_LOOP)
					{
					  	newFrame = bone.startFrame;
						assert(newFrame>=0&&newFrame<numFramesInFile);
					}
					// if we intend to end this anim or freeze after this, then just keep on the last frame
					else
					{
						newFrame = bone.endFrame-1;
						assert(newFrame>=0&&newFrame<numFramesInFile);
					}
				}
				assert(newFrame>=0&&newFrame<numFramesInFile);
			}
			else
			{
				lerp = (ceil(newFrame_g)-newFrame_g);
				// frames are easy to calculate
				currentFrame = ceil(newFrame_g);
				if (currentFrame>bone.startFrame)
				{
					currentFrame=bone.startFrame;
					newFrame = currentFrame;
					lerp=0.0f;
				}
				else
				{
					newFrame=currentFrame-1;
					// are we now on the end frame?
					if (newFrame < endFrame+1)
					{
						// we only want to lerp with the first frame of the anim if we are looping
						if (bone.flags & BONE_ANIM_OVERRIDE_LOOP)
						{
					  		newFrame = bone.startFrame;
							assert(newFrame>=0&&newFrame<numFramesInFile);
						}
						// if we intend to end this anim or freeze after this, then just keep on the last frame
						else
						{
							newFrame = bone.endFrame+1;
							assert(newFrame>=0&&newFrame<numFramesInFile);
						}
					}
				}
				assert(currentFrame>=0&&currentFrame<numFramesInFile);
				assert(newFrame>=0&&newFrame<numFramesInFile);
			}
		}
	}
	else
	{
		if (animSpeed<0.0)
		{
			currentFrame = bone.endFrame+1;
		}
		else
		{
			currentFrame = bone.endFrame-1;
		}
		if (currentFrame<0)
		{
			currentFrame=0;
		}
		assert(currentFrame>=0&&currentFrame<numFramesInFile);
		newFrame = currentFrame;
		assert(newFrame>=0&&newFrame<numFramesInFile);
		lerp = 0;

	}
	/*
	assert(currentFrame>=0&&currentFrame<numFramesInFile);
	assert(newFrame>=0&&newFrame<numFramesInFile);
	assert(lerp>=0.0f&&lerp<=1.0f);
	*/
}

//basically construct a seperate skeleton with full hierarchy to store a matrix
//off which will give us the desired settling position given the frame in the skeleton
//that should be used -rww
int G2_Add_Bone (const model_s *mod, boneInfo_v &blist, const char *boneName);
int G2_Find_Bone(CGhoul2Info *ghlInfo, boneInfo_v &blist, const char *boneName);
void G2_RagGetAnimMatrix(CGhoul2Info &ghoul2, const int boneNum, mdxaBone_t &matrix, const int frame)
{
	mdxaBone_t animMatrix;
	mdxaSkel_t *skel;
	mdxaSkel_t *pskel;
	mdxaSkelOffsets_t *offsets;
	int parent;
	int bListIndex;
	int parentBlistIndex;
#ifdef _RAG_PRINT_TEST
	bool actuallySet = false;
#endif

	assert(ghoul2.mBoneCache);
	assert(ghoul2.animModel);

	offsets = (mdxaSkelOffsets_t *)((byte *)ghoul2.mBoneCache->header + sizeof(mdxaHeader_t));
	skel = (mdxaSkel_t *)((byte *)ghoul2.mBoneCache->header + sizeof(mdxaHeader_t) + offsets->offsets[boneNum]);

	//find/add the bone in the list
	if (!skel->name[0])
	{
		bListIndex = -1;
	}
	else
	{
		bListIndex = G2_Find_Bone(&ghoul2, ghoul2.mBlist, skel->name);
		if (bListIndex == -1)
		{
#ifdef _RAG_PRINT_TEST
			Com_Printf("Attempting to add %s\n", skel->name);
#endif
			bListIndex = G2_Add_Bone(ghoul2.animModel, ghoul2.mBlist, skel->name);
		}
	}

	assert(bListIndex != -1);

	boneInfo_t &bone = ghoul2.mBlist[bListIndex];

	if (bone.hasAnimFrameMatrix == frame)
	{ //already calculated so just grab it
		matrix = bone.animFrameMatrix;
		return;
	}

	//get the base matrix for the specified frame
	UnCompressBone(animMatrix.matrix, boneNum, ghoul2.mBoneCache->header, frame);

	parent = skel->parent;
	if (boneNum > 0 && parent > -1)
	{
		//recursively call to assure all parent matrices are set up
		G2_RagGetAnimMatrix(ghoul2, parent, matrix, frame);

		//assign the new skel ptr for our parent
		pskel = (mdxaSkel_t *)((byte *)ghoul2.mBoneCache->header + sizeof(mdxaHeader_t) + offsets->offsets[parent]);

		//taking bone matrix for the skeleton frame and parent's animFrameMatrix into account, determine our final animFrameMatrix
		if (!pskel->name[0])
		{
			parentBlistIndex = -1;
		}
		else
		{
			parentBlistIndex = G2_Find_Bone(&ghoul2, ghoul2.mBlist, pskel->name);
			if (parentBlistIndex == -1)
			{
				parentBlistIndex = G2_Add_Bone(ghoul2.animModel, ghoul2.mBlist, pskel->name);
			}
		}

		assert(parentBlistIndex != -1);

		boneInfo_t &pbone = ghoul2.mBlist[parentBlistIndex];

		assert(pbone.hasAnimFrameMatrix == frame); //this should have been calc'd in the recursive call

		Multiply_3x4Matrix(&bone.animFrameMatrix, &pbone.animFrameMatrix, &animMatrix);

#ifdef _RAG_PRINT_TEST
		if (parentBlistIndex != -1 && bListIndex != -1)
		{
			actuallySet = true;
		}
		else
		{
			Com_Printf("BAD LIST INDEX: %s, %s [%i]\n", skel->name, pskel->name, parent);
		}
#endif
	}
	else
	{ //root
		Multiply_3x4Matrix(&bone.animFrameMatrix, &ghoul2.mBoneCache->rootMatrix, &animMatrix);
#ifdef _RAG_PRINT_TEST
		if (bListIndex != -1)
		{
			actuallySet = true;
		}
		else
		{
			Com_Printf("BAD LIST INDEX: %s\n", skel->name);
		}
#endif
		//bone.animFrameMatrix = ghoul2.mBoneCache->mFinalBones[boneNum].boneMatrix;
		//Maybe use this for the root, so that the orientation is in sync with the current
		//root matrix? However this would require constant recalculation of this base
		//skeleton which I currently do not want.
	}

	//never need to figure it out again
	bone.hasAnimFrameMatrix = frame;

#ifdef _RAG_PRINT_TEST
	if (!actuallySet)
	{
		Com_Printf("SET FAILURE\n");
	}
#endif

	matrix = bone.animFrameMatrix;
}

// transform each individual bone's information - making sure to use any override information provided, both for angles and for animations, as
// well as multiplying each bone's matrix by it's parents matrix
void G2_TransformBone (int child,CBoneCache &BC)
{
	SBoneCalc &TB=BC.mBones[child];
	mdxaBone_t		tbone[6];
// 	mdxaFrame_t		*aFrame=0;
//	mdxaFrame_t		*bFrame=0;
//	mdxaFrame_t		*aoldFrame=0;
//	mdxaFrame_t		*boldFrame=0;
	mdxaSkel_t		*skel;
	mdxaSkelOffsets_t *offsets;
	boneInfo_v		&boneList = *BC.rootBoneList;
	int				j, boneListIndex;
	int				angleOverride = 0;

#if DEBUG_G2_TIMING
	bool printTiming=false;
#endif
	// should this bone be overridden by a bone in the bone list?
	boneListIndex = G2_Find_Bone_In_List(boneList, child);
	if (boneListIndex != -1)
	{
		// we found a bone in the list - we need to override something here.

		// do we override the rotational angles?
		if ((boneList[boneListIndex].flags) & (BONE_ANGLES_TOTAL))
		{
			angleOverride = (boneList[boneListIndex].flags) & (BONE_ANGLES_TOTAL);
		}

		// set blending stuff if we need to
		if (boneList[boneListIndex].flags & BONE_ANIM_BLEND)
		{
			float blendTime = BC.incomingTime - boneList[boneListIndex].blendStart;
			// only set up the blend anim if we actually have some blend time left on this bone anim - otherwise we might corrupt some blend higher up the hiearchy
			if (blendTime>=0.0f&&blendTime < boneList[boneListIndex].blendTime)
			{
				TB.blendFrame	 = boneList[boneListIndex].blendFrame;
				TB.blendOldFrame = boneList[boneListIndex].blendLerpFrame;
				TB.blendLerp = (blendTime / boneList[boneListIndex].blendTime);
				TB.blendMode = true;
			}
			else
			{
				TB.blendMode = false;
			}
		}
		else if (r_Ghoul2NoBlend->integer||((boneList[boneListIndex].flags) & (BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_OVERRIDE)))
		// turn off blending if we are just doing a straing animation override
		{
			TB.blendMode = false;
		}

		// should this animation be overridden by an animation in the bone list?
		if ((boneList[boneListIndex].flags) & (BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_OVERRIDE))
		{
			G2_TimingModel(boneList[boneListIndex],BC.incomingTime,BC.header->numFrames,TB.currentFrame,TB.newFrame,TB.backlerp);
		}
#if DEBUG_G2_TIMING
		printTiming=true;
#endif
		if ((r_Ghoul2NoLerp->integer)||((boneList[boneListIndex].flags) & (BONE_ANIM_NO_LERP)))
		{
			TB.backlerp = 0.0f;
		}
	}
	// figure out where the location of the bone animation data is
	assert(TB.newFrame>=0&&TB.newFrame<BC.header->numFrames);
	if (!(TB.newFrame>=0&&TB.newFrame<BC.header->numFrames))
	{
		TB.newFrame=0;
	}
//	aFrame = (mdxaFrame_t *)((byte *)BC.header + BC.header->ofsFrames + TB.newFrame * BC.frameSize );
	assert(TB.currentFrame>=0&&TB.currentFrame<BC.header->numFrames);
	if (!(TB.currentFrame>=0&&TB.currentFrame<BC.header->numFrames))
	{
		TB.currentFrame=0;
	}
//	aoldFrame = (mdxaFrame_t *)((byte *)BC.header + BC.header->ofsFrames + TB.currentFrame * BC.frameSize );

	// figure out where the location of the blended animation data is
	assert(!(TB.blendFrame < 0.0 || TB.blendFrame >= (BC.header->numFrames+1)));
	if (TB.blendFrame < 0.0 || TB.blendFrame >= (BC.header->numFrames+1) )
	{
		TB.blendFrame=0.0;
	}
//	bFrame = (mdxaFrame_t *)((byte *)BC.header + BC.header->ofsFrames + (int)TB.blendFrame * BC.frameSize );
	assert(TB.blendOldFrame>=0&&TB.blendOldFrame<BC.header->numFrames);
	if (!(TB.blendOldFrame>=0&&TB.blendOldFrame<BC.header->numFrames))
	{
		TB.blendOldFrame=0;
	}
#if DEBUG_G2_TIMING

#if DEBUG_G2_TIMING_RENDER_ONLY
	if (!HackadelicOnClient)
	{
		printTiming=false;
	}
#endif
	if (printTiming)
	{
		char mess[1000];
		if (TB.blendMode)
		{
			sprintf(mess,"b %2d %5d   %4d %4d %4d %4d  %f %f\n",boneListIndex,BC.incomingTime,(int)TB.newFrame,(int)TB.currentFrame,(int)TB.blendFrame,(int)TB.blendOldFrame,TB.backlerp,TB.blendLerp);
		}
		else
		{
			sprintf(mess,"a %2d %5d   %4d %4d            %f\n",boneListIndex,BC.incomingTime,TB.newFrame,TB.currentFrame,TB.backlerp);
		}
		OutputDebugString(mess);
		const boneInfo_t &bone=boneList[boneListIndex];
		if (bone.flags&BONE_ANIM_BLEND)
		{
			sprintf(mess,"                                                                    bfb[%2d] %5d  %5d  (%5d-%5d) %4.2f %4x   bt(%5d-%5d) %7.2f %5d\n",
				boneListIndex,
				BC.incomingTime,
				bone.startTime,
				bone.startFrame,
				bone.endFrame,
				bone.animSpeed,
				bone.flags,
				bone.blendStart,
				bone.blendStart+bone.blendTime,
				bone.blendFrame,
				bone.blendLerpFrame
				);
		}
		else
		{
			sprintf(mess,"                                                                    bfa[%2d] %5d  %5d  (%5d-%5d) %4.2f %4x\n",
				boneListIndex,
				BC.incomingTime,
				bone.startTime,
				bone.startFrame,
				bone.endFrame,
				bone.animSpeed,
				bone.flags
				);
		}
//		OutputDebugString(mess);
	}
#endif
//	boldFrame = (mdxaFrame_t *)((byte *)BC.header + BC.header->ofsFrames + TB.blendOldFrame * BC.frameSize );

//	mdxaCompBone_t	*compBonePointer = (mdxaCompBone_t *)((byte *)BC.header + BC.header->ofsCompBonePool);

	assert(child>=0&&child<BC.header->numBones);
//	assert(bFrame->boneIndexes[child]>=0);
//	assert(boldFrame->boneIndexes[child]>=0);
//	assert(aFrame->boneIndexes[child]>=0);
//	assert(aoldFrame->boneIndexes[child]>=0);

	// decide where the transformed bone is going

	// are we blending with another frame of anim?
	if (TB.blendMode)
	{
		float backlerp = TB.blendFrame - (int)TB.blendFrame;
		float frontlerp = 1.0 - backlerp;

// 		MC_UnCompress(tbone[3].matrix,compBonePointer[bFrame->boneIndexes[child]].Comp);
// 		MC_UnCompress(tbone[4].matrix,compBonePointer[boldFrame->boneIndexes[child]].Comp);
		UnCompressBone(tbone[3].matrix, child, BC.header, TB.blendFrame);
		UnCompressBone(tbone[4].matrix, child, BC.header, TB.blendOldFrame);

		for ( j = 0 ; j < 12 ; j++ )
		{
  			((float *)&tbone[5])[j] = (backlerp * ((float *)&tbone[3])[j])
				+ (frontlerp * ((float *)&tbone[4])[j]);
		}
	}

  	//
  	// lerp this bone - use the temp space on the ref entity to put the bone transforms into
  	//
  	if (!TB.backlerp)
  	{
// 		MC_UnCompress(tbone[2].matrix,compBonePointer[aoldFrame->boneIndexes[child]].Comp);
		UnCompressBone(tbone[2].matrix, child, BC.header, TB.currentFrame);

		// blend in the other frame if we need to
		if (TB.blendMode)
		{
			float blendFrontlerp = 1.0 - TB.blendLerp;
	  		for ( j = 0 ; j < 12 ; j++ )
			{
  				((float *)&tbone[2])[j] = (TB.blendLerp * ((float *)&tbone[2])[j])
					+ (blendFrontlerp * ((float *)&tbone[5])[j]);
			}
		}

  		if (!child)
		{
			// now multiply by the root matrix, so we can offset this model should we need to
			Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix, &BC.rootMatrix, &tbone[2]);
 		}
  	}
	else
  	{
		float frontlerp = 1.0 - TB.backlerp;
// 		MC_UnCompress(tbone[0].matrix,compBonePointer[aFrame->boneIndexes[child]].Comp);
//		MC_UnCompress(tbone[1].matrix,compBonePointer[aoldFrame->boneIndexes[child]].Comp);
		UnCompressBone(tbone[0].matrix, child, BC.header, TB.newFrame);
		UnCompressBone(tbone[1].matrix, child, BC.header, TB.currentFrame);

		for ( j = 0 ; j < 12 ; j++ )
		{
  			((float *)&tbone[2])[j] = (TB.backlerp * ((float *)&tbone[0])[j])
				+ (frontlerp * ((float *)&tbone[1])[j]);
		}

		// blend in the other frame if we need to
		if (TB.blendMode)
		{
			float blendFrontlerp = 1.0 - TB.blendLerp;
	  		for ( j = 0 ; j < 12 ; j++ )
			{
  				((float *)&tbone[2])[j] = (TB.blendLerp * ((float *)&tbone[2])[j])
					+ (blendFrontlerp * ((float *)&tbone[5])[j]);
			}
		}

  		if (!child)
  		{
			// now multiply by the root matrix, so we can offset this model should we need to
			Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix, &BC.rootMatrix, &tbone[2]);
  		}
	}
	// figure out where the bone hirearchy info is
	offsets = (mdxaSkelOffsets_t *)((byte *)BC.header + sizeof(mdxaHeader_t));
	skel = (mdxaSkel_t *)((byte *)BC.header + sizeof(mdxaHeader_t) + offsets->offsets[child]);

	int parent=BC.mFinalBones[child].parent;
	assert((parent==-1&&child==0)||(parent>=0&&parent<BC.mNumBones));
	if (angleOverride & BONE_ANGLES_REPLACE)
	{
		bool isRag=!!(angleOverride & BONE_ANGLES_RAGDOLL);
		if (!isRag)
		{ //do the same for ik.. I suppose.
			isRag = !!(angleOverride & BONE_ANGLES_IK);
		}

		mdxaBone_t &bone = BC.mFinalBones[child].boneMatrix;
		boneInfo_t &boneOverride = boneList[boneListIndex];

		if (isRag)
		{
			mdxaBone_t temp, firstPass;
			// give us the matrix the animation thinks we should have, so we can get the correct X&Y coors
			Multiply_3x4Matrix(&firstPass, &BC.mFinalBones[parent].boneMatrix, &tbone[2]);
			// this is crazy, we are gonna drive the animation to ID while we are doing post mults to compensate.
			Multiply_3x4Matrix(&temp,&firstPass, &skel->BasePoseMat);
			float	matrixScale = VectorLength((float*)&temp);
			static mdxaBone_t		toMatrix =
			{
				{
					{ 1.0f, 0.0f, 0.0f, 0.0f },
					{ 0.0f, 1.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 1.0f, 0.0f }
				}
			};
			toMatrix.matrix[0][0]=matrixScale;
			toMatrix.matrix[1][1]=matrixScale;
			toMatrix.matrix[2][2]=matrixScale;
			toMatrix.matrix[0][3]=temp.matrix[0][3];
			toMatrix.matrix[1][3]=temp.matrix[1][3];
			toMatrix.matrix[2][3]=temp.matrix[2][3];

 			Multiply_3x4Matrix(&temp, &toMatrix,&skel->BasePoseMatInv); //dest first arg

			float blendTime = BC.incomingTime - boneList[boneListIndex].boneBlendStart;
			float blendLerp = (blendTime / boneList[boneListIndex].boneBlendTime);
			if (blendLerp>0.0f)
			{
				// has started
				if (blendLerp>1.0f)
				{
					// done
//					Multiply_3x4Matrix(&bone, &BC.mFinalBones[parent].boneMatrix,&temp);
					memcpy (&bone,&temp, sizeof(mdxaBone_t));
				}
				else
				{
//					mdxaBone_t lerp;
					// now do the blend into the destination
					float blendFrontlerp = 1.0 - blendLerp;
	  				for ( j = 0 ; j < 12 ; j++ )
					{
  						((float *)&bone)[j] = (blendLerp * ((float *)&temp)[j])
							+ (blendFrontlerp * ((float *)&tbone[2])[j]);
					}
//					Multiply_3x4Matrix(&bone, &BC.mFinalBones[parent].boneMatrix,&lerp);
				}
			}
		}
		else
		{
			mdxaBone_t temp, firstPass;

			// give us the matrix the animation thinks we should have, so we can get the correct X&Y coors
			Multiply_3x4Matrix(&firstPass, &BC.mFinalBones[parent].boneMatrix, &tbone[2]);

			// are we attempting to blend with the base animation? and still within blend time?
			if (boneOverride.boneBlendTime && (((boneOverride.boneBlendTime + boneOverride.boneBlendStart) < BC.incomingTime)))
			{
				// ok, we are supposed to be blending. Work out lerp
				float blendTime = BC.incomingTime - boneList[boneListIndex].boneBlendStart;
				float blendLerp = (blendTime / boneList[boneListIndex].boneBlendTime);

				if (blendLerp <= 1)
				{
					if (blendLerp < 0)
					{
						assert(0);
					}

					// now work out the matrix we want to get *to* - firstPass is where we are coming *from*
					Multiply_3x4Matrix(&temp, &firstPass, &skel->BasePoseMat);

					float	matrixScale = VectorLength((float*)&temp);

					mdxaBone_t	newMatrixTemp;

					if (HackadelicOnClient)
					{
						for (int i=0; i<3;i++)
						{
							for(int x=0;x<3; x++)
							{
								newMatrixTemp.matrix[i][x] = boneOverride.newMatrix.matrix[i][x]*matrixScale;
							}
						}

						newMatrixTemp.matrix[0][3] = temp.matrix[0][3];
						newMatrixTemp.matrix[1][3] = temp.matrix[1][3];
						newMatrixTemp.matrix[2][3] = temp.matrix[2][3];
					}
					else
					{
						for (int i=0; i<3;i++)
						{
							for(int x=0;x<3; x++)
							{
								newMatrixTemp.matrix[i][x] = boneOverride.matrix.matrix[i][x]*matrixScale;
							}
						}

						newMatrixTemp.matrix[0][3] = temp.matrix[0][3];
						newMatrixTemp.matrix[1][3] = temp.matrix[1][3];
						newMatrixTemp.matrix[2][3] = temp.matrix[2][3];
					}

 					Multiply_3x4Matrix(&temp, &newMatrixTemp,&skel->BasePoseMatInv);

					// now do the blend into the destination
					float blendFrontlerp = 1.0 - blendLerp;
	  				for ( j = 0 ; j < 12 ; j++ )
					{
  						((float *)&bone)[j] = (blendLerp * ((float *)&temp)[j])
							+ (blendFrontlerp * ((float *)&firstPass)[j]);
					}
				}
				else
				{
					bone = firstPass;
				}
			}
			// no, so just override it directly
			else
			{

				Multiply_3x4Matrix(&temp,&firstPass, &skel->BasePoseMat);
				float	matrixScale = VectorLength((float*)&temp);

				mdxaBone_t	newMatrixTemp;

				if (HackadelicOnClient)
				{
					for (int i=0; i<3;i++)
					{
						for(int x=0;x<3; x++)
						{
							newMatrixTemp.matrix[i][x] = boneOverride.newMatrix.matrix[i][x]*matrixScale;
						}
					}

					newMatrixTemp.matrix[0][3] = temp.matrix[0][3];
					newMatrixTemp.matrix[1][3] = temp.matrix[1][3];
					newMatrixTemp.matrix[2][3] = temp.matrix[2][3];
				}
				else
				{
					for (int i=0; i<3;i++)
					{
						for(int x=0;x<3; x++)
						{
							newMatrixTemp.matrix[i][x] = boneOverride.matrix.matrix[i][x]*matrixScale;
						}
					}

					newMatrixTemp.matrix[0][3] = temp.matrix[0][3];
					newMatrixTemp.matrix[1][3] = temp.matrix[1][3];
					newMatrixTemp.matrix[2][3] = temp.matrix[2][3];
				}

 				Multiply_3x4Matrix(&bone, &newMatrixTemp,&skel->BasePoseMatInv);
			}
		}
	}
	else if (angleOverride & BONE_ANGLES_PREMULT)
	{
		if ((angleOverride&BONE_ANGLES_RAGDOLL) || (angleOverride&BONE_ANGLES_IK))
		{
			mdxaBone_t	tmp;
			if (!child)
			{
				if (HackadelicOnClient)
				{
					Multiply_3x4Matrix(&tmp, &BC.rootMatrix, &boneList[boneListIndex].newMatrix);
				}
				else
				{
					Multiply_3x4Matrix(&tmp, &BC.rootMatrix, &boneList[boneListIndex].matrix);
				}
			}
			else
			{
				if (HackadelicOnClient)
				{
					Multiply_3x4Matrix(&tmp, &BC.mFinalBones[parent].boneMatrix, &boneList[boneListIndex].newMatrix);
				}
				else
				{
					Multiply_3x4Matrix(&tmp, &BC.mFinalBones[parent].boneMatrix, &boneList[boneListIndex].matrix);
				}
			}
			Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix,&tmp, &tbone[2]);
		}
		else
		{
			if (!child)
			{
				// use the in coming root matrix as our basis
				if (HackadelicOnClient)
				{
					Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix, &BC.rootMatrix, &boneList[boneListIndex].newMatrix);
				}
				else
				{
					Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix, &BC.rootMatrix, &boneList[boneListIndex].matrix);
				}
 			}
			else
			{
				// convert from 3x4 matrix to a 4x4 matrix
				if (HackadelicOnClient)
				{
					Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix, &BC.mFinalBones[parent].boneMatrix, &boneList[boneListIndex].newMatrix);
				}
				else
				{
					Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix, &BC.mFinalBones[parent].boneMatrix, &boneList[boneListIndex].matrix);
				}
			}
		}
	}
	else
	// now transform the matrix by it's parent, asumming we have a parent, and we aren't overriding the angles absolutely
	if (child)
	{
		Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix, &BC.mFinalBones[parent].boneMatrix, &tbone[2]);
	}

	// now multiply our resulting bone by an override matrix should we need to
	if (angleOverride & BONE_ANGLES_POSTMULT)
	{
		mdxaBone_t	tempMatrix;
		memcpy (&tempMatrix,&BC.mFinalBones[child].boneMatrix, sizeof(mdxaBone_t));
		if (HackadelicOnClient)
		{
		  	Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix, &tempMatrix, &boneList[boneListIndex].newMatrix);
		}
		else
		{
		  	Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix, &tempMatrix, &boneList[boneListIndex].matrix);
		}
	}
	if (r_Ghoul2UnSqash->integer)
	{
		mdxaBone_t tempMatrix;
		Multiply_3x4Matrix(&tempMatrix,&BC.mFinalBones[child].boneMatrix, &skel->BasePoseMat);
		float maxl;
		maxl=VectorLength(&skel->BasePoseMat.matrix[0][0]);
		VectorNormalize(&tempMatrix.matrix[0][0]);
		VectorNormalize(&tempMatrix.matrix[1][0]);
		VectorNormalize(&tempMatrix.matrix[2][0]);

		VectorScale(&tempMatrix.matrix[0][0],maxl,&tempMatrix.matrix[0][0]);
		VectorScale(&tempMatrix.matrix[1][0],maxl,&tempMatrix.matrix[1][0]);
		VectorScale(&tempMatrix.matrix[2][0],maxl,&tempMatrix.matrix[2][0]);
		Multiply_3x4Matrix(&BC.mFinalBones[child].boneMatrix,&tempMatrix,&skel->BasePoseMatInv);
	}

}


#define		GHOUL2_RAG_STARTED						0x0010

// start the recursive hirearchial bone transform and lerp process for this model
void G2_TransformGhoulBones(boneInfo_v &rootBoneList,mdxaBone_t &rootMatrix, CGhoul2Info &ghoul2, int time,bool smooth=true)
{
#ifdef G2_PERFORMANCE_ANALYSIS
	G2PerformanceCounter_G2_TransformGhoulBones++;
#endif
	assert(ghoul2.aHeader);
	assert(ghoul2.currentModel);
	assert(ghoul2.currentModel->mdxm);
	if (!ghoul2.aHeader->numBones)
	{
		assert(0); // this would be strange
		return;
	}
	if (!ghoul2.mBoneCache)
	{
		ghoul2.mBoneCache=new CBoneCache(ghoul2.currentModel,ghoul2.aHeader);
	}
	ghoul2.mBoneCache->mod=ghoul2.currentModel;
	ghoul2.mBoneCache->header=ghoul2.aHeader;
	assert((int)ghoul2.mBoneCache->mNumBones==ghoul2.aHeader->numBones);

	ghoul2.mBoneCache->mSmoothingActive=false;
	ghoul2.mBoneCache->mUnsquash=false;

	// master smoothing control
	float val=r_Ghoul2AnimSmooth->value;
	if (smooth&&val>0.0f&&val<1.0f)
	{
		ghoul2.mBoneCache->mLastTouch=ghoul2.mBoneCache->mLastLastTouch;

		if(ghoul2.mFlags & GHOUL2_RAG_STARTED)
		{
			for (size_t k=0;k<rootBoneList.size();k++)
			{
				boneInfo_t &bone=rootBoneList[k];
				if (bone.flags&BONE_ANGLES_RAGDOLL)
				{
					if (bone.firstCollisionTime &&
						bone.firstCollisionTime>time-250 &&
						bone.firstCollisionTime<time)
					{
						val=0.9f;//(val+0.8f)/2.0f;
					}
					else if (bone.airTime > time)
					{
						val = 0.2f;
					}
					else
					{
						val = 0.8f;
					}
					break;
				}
			}
		}

		ghoul2.mBoneCache->mSmoothFactor=val;
		ghoul2.mBoneCache->mSmoothingActive=true;
		if (r_Ghoul2UnSqashAfterSmooth->integer)
		{
			ghoul2.mBoneCache->mUnsquash=true;
		}
	}
	else
	{
		ghoul2.mBoneCache->mSmoothFactor=1.0f;
	}
	ghoul2.mBoneCache->mCurrentTouch++;

//rww - RAGDOLL_BEGIN
	if (HackadelicOnClient)
	{
		ghoul2.mBoneCache->mLastLastTouch=ghoul2.mBoneCache->mCurrentTouch;
		ghoul2.mBoneCache->mCurrentTouchRender=ghoul2.mBoneCache->mCurrentTouch;
	}
	else
	{
		ghoul2.mBoneCache->mCurrentTouchRender=0;
	}
//rww - RAGDOLL_END

//	ghoul2.mBoneCache->mWraithID=0;
	ghoul2.mBoneCache->frameSize = 0;// can be deleted in new G2 format	//(int)( &((mdxaFrame_t *)0)->boneIndexes[ ghoul2.aHeader->numBones ] );

	ghoul2.mBoneCache->rootBoneList=&rootBoneList;
	ghoul2.mBoneCache->rootMatrix=rootMatrix;
	ghoul2.mBoneCache->incomingTime=time;

	SBoneCalc &TB=ghoul2.mBoneCache->Root();
	TB.newFrame=0;
	TB.currentFrame=0;
	TB.backlerp=0.0f;
	TB.blendFrame=0;
	TB.blendOldFrame=0;
	TB.blendMode=false;
	TB.blendLerp=0;

}


#define MDX_TAG_ORIGIN 2

//======================================================================
//
// Surface Manipulation code


// We've come across a surface that's designated as a bolt surface, process it and put it in the appropriate bolt place
void G2_ProcessSurfaceBolt2(CBoneCache &boneCache, const mdxmSurface_t *surface, int boltNum, boltInfo_v &boltList, const surfaceInfo_t *surfInfo, const model_s *mod,mdxaBone_t &retMatrix)
{
 	mdxmVertex_t 	*v, *vert0, *vert1, *vert2;
 	vec3_t			axes[3], sides[3];
 	float			pTri[3][3], d;
 	int				j, k;

	// now there are two types of tag surface - model ones and procedural generated types - lets decide which one we have here.
	if (surfInfo && surfInfo->offFlags == G2SURFACEFLAG_GENERATED)
	{
		int surfNumber = surfInfo->genPolySurfaceIndex & 0x0ffff;
		int	polyNumber = (surfInfo->genPolySurfaceIndex >> 16) & 0x0ffff;

		// find original surface our original poly was in.
		mdxmSurface_t	*originalSurf = (mdxmSurface_t *)G2_FindSurface(mod, surfNumber, surfInfo->genLod);
		mdxmTriangle_t	*originalTriangleIndexes = (mdxmTriangle_t *)((byte*)originalSurf + originalSurf->ofsTriangles);

		// get the original polys indexes
		int index0 = originalTriangleIndexes[polyNumber].indexes[0];
		int index1 = originalTriangleIndexes[polyNumber].indexes[1];
		int index2 = originalTriangleIndexes[polyNumber].indexes[2];

		// decide where the original verts are
 		vert0 = (mdxmVertex_t *) ((byte *)originalSurf + originalSurf->ofsVerts);
		vert0+=index0;

		vert1 = (mdxmVertex_t *) ((byte *)originalSurf + originalSurf->ofsVerts);
		vert1+=index1;

		vert2 = (mdxmVertex_t *) ((byte *)originalSurf + originalSurf->ofsVerts);
		vert2+=index2;

		// clear out the triangle verts to be
 	   	VectorClear( pTri[0] );
 	   	VectorClear( pTri[1] );
 	   	VectorClear( pTri[2] );
		int *piBoneReferences = (int*) ((byte*)originalSurf + originalSurf->ofsBoneReferences);

//		mdxmWeight_t	*w;

		// now go and transform just the points we need from the surface that was hit originally
//		w = vert0->weights;
		float fTotalWeight = 0.0f;
		int iNumWeights = G2_GetVertWeights( vert0 );
 		for ( k = 0 ; k < iNumWeights ; k++ )
 		{
			int		iBoneIndex	= G2_GetVertBoneIndex( vert0, k );
			float	fBoneWeight	= G2_GetVertBoneWeight( vert0, k, fTotalWeight, iNumWeights );

			const mdxaBone_t &bone=boneCache.Eval(piBoneReferences[iBoneIndex]);

			pTri[0][0] += fBoneWeight * ( DotProduct( bone.matrix[0], vert0->vertCoords ) + bone.matrix[0][3] );
 			pTri[0][1] += fBoneWeight * ( DotProduct( bone.matrix[1], vert0->vertCoords ) + bone.matrix[1][3] );
 			pTri[0][2] += fBoneWeight * ( DotProduct( bone.matrix[2], vert0->vertCoords ) + bone.matrix[2][3] );
		}

//		w = vert1->weights;
		fTotalWeight = 0.0f;
		iNumWeights = G2_GetVertWeights( vert1 );
 		for ( k = 0 ; k < iNumWeights ; k++)
 		{
			int		iBoneIndex	= G2_GetVertBoneIndex( vert1, k );
			float	fBoneWeight	= G2_GetVertBoneWeight( vert1, k, fTotalWeight, iNumWeights );

			const mdxaBone_t &bone=boneCache.Eval(piBoneReferences[iBoneIndex]);

      pTri[1][0] += fBoneWeight * ( DotProduct( bone.matrix[0], vert1->vertCoords ) + bone.matrix[0][3] );
 			pTri[1][1] += fBoneWeight * ( DotProduct( bone.matrix[1], vert1->vertCoords ) + bone.matrix[1][3] );
 			pTri[1][2] += fBoneWeight * ( DotProduct( bone.matrix[2], vert1->vertCoords ) + bone.matrix[2][3] );
		}

//		w = vert2->weights;
		fTotalWeight = 0.0f;
		iNumWeights = G2_GetVertWeights( vert2 );
 		for ( k = 0 ; k < iNumWeights ; k++)
 		{
			int		iBoneIndex	= G2_GetVertBoneIndex( vert2, k );
			float	fBoneWeight	= G2_GetVertBoneWeight( vert2, k, fTotalWeight, iNumWeights );

			const mdxaBone_t &bone=boneCache.Eval(piBoneReferences[iBoneIndex]);

			pTri[2][0] += fBoneWeight * ( DotProduct( bone.matrix[0], vert2->vertCoords ) + bone.matrix[0][3] );
 			pTri[2][1] += fBoneWeight * ( DotProduct( bone.matrix[1], vert2->vertCoords ) + bone.matrix[1][3] );
			pTri[2][2] += fBoneWeight * ( DotProduct( bone.matrix[2], vert2->vertCoords ) + bone.matrix[2][3] );
		}

   		vec3_t normal;
		vec3_t up;
		vec3_t right;
		vec3_t vec0, vec1;
		// work out baryCentricK
		float baryCentricK = 1.0 - (surfInfo->genBarycentricI + surfInfo->genBarycentricJ);

		// now we have the model transformed into model space, now generate an origin.
		retMatrix.matrix[0][3] = (pTri[0][0] * surfInfo->genBarycentricI) + (pTri[1][0] * surfInfo->genBarycentricJ) + (pTri[2][0] * baryCentricK);
		retMatrix.matrix[1][3] = (pTri[0][1] * surfInfo->genBarycentricI) + (pTri[1][1] * surfInfo->genBarycentricJ) + (pTri[2][1] * baryCentricK);
		retMatrix.matrix[2][3] = (pTri[0][2] * surfInfo->genBarycentricI) + (pTri[1][2] * surfInfo->genBarycentricJ) + (pTri[2][2] * baryCentricK);

		// generate a normal to this new triangle
		VectorSubtract(pTri[0], pTri[1], vec0);
		VectorSubtract(pTri[2], pTri[1], vec1);

		CrossProduct(vec0, vec1, normal);
		VectorNormalize(normal);

		// forward vector
		retMatrix.matrix[0][0] = normal[0];
		retMatrix.matrix[1][0] = normal[1];
		retMatrix.matrix[2][0] = normal[2];

		// up will be towards point 0 of the original triangle.
		// so lets work it out. Vector is hit point - point 0
		up[0] = retMatrix.matrix[0][3] - pTri[0][0];
		up[1] = retMatrix.matrix[1][3] - pTri[0][1];
		up[2] = retMatrix.matrix[2][3] - pTri[0][2];

		// normalise it
		VectorNormalize(up);

		// that's the up vector
		retMatrix.matrix[0][1] = up[0];
		retMatrix.matrix[1][1] = up[1];
		retMatrix.matrix[2][1] = up[2];

		// right is always straight

		CrossProduct( normal, up, right );
		// that's the up vector
		retMatrix.matrix[0][2] = right[0];
		retMatrix.matrix[1][2] = right[1];
		retMatrix.matrix[2][2] = right[2];


	}
	// no, we are looking at a normal model tag
	else
	{
	 	// whip through and actually transform each vertex
 		v = (mdxmVertex_t *) ((byte *)surface + surface->ofsVerts);
		int *piBoneReferences = (int*) ((byte*)surface + surface->ofsBoneReferences);
 		for ( j = 0; j < 3; j++ )
 		{
// 			mdxmWeight_t	*w;

 			VectorClear( pTri[j] );
 //			w = v->weights;

			const int iNumWeights = G2_GetVertWeights( v );

			float fTotalWeight = 0.0f;
 			for ( k = 0 ; k < iNumWeights ; k++)
 			{
				int		iBoneIndex	= G2_GetVertBoneIndex( v, k );
				float	fBoneWeight	= G2_GetVertBoneWeight( v, k, fTotalWeight, iNumWeights );

				const mdxaBone_t &bone=boneCache.Eval(piBoneReferences[iBoneIndex]);

				pTri[j][0] += fBoneWeight * ( DotProduct( bone.matrix[0], v->vertCoords ) + bone.matrix[0][3] );
				pTri[j][1] += fBoneWeight * ( DotProduct( bone.matrix[1], v->vertCoords ) + bone.matrix[1][3] );
				pTri[j][2] += fBoneWeight * ( DotProduct( bone.matrix[2], v->vertCoords ) + bone.matrix[2][3] );
 			}

 			v++;// = (mdxmVertex_t *)&v->weights[/*v->numWeights*/surface->maxVertBoneWeights];
 		}

 		// clear out used arrays
 		memset( axes, 0, sizeof( axes ) );
 		memset( sides, 0, sizeof( sides ) );

 		// work out actual sides of the tag triangle
 		for ( j = 0; j < 3; j++ )
 		{
 			sides[j][0] = pTri[(j+1)%3][0] - pTri[j][0];
 			sides[j][1] = pTri[(j+1)%3][1] - pTri[j][1];
 			sides[j][2] = pTri[(j+1)%3][2] - pTri[j][2];
 		}

 		// do math trig to work out what the matrix will be from this triangle's translated position
 		VectorNormalize2( sides[iG2_TRISIDE_LONGEST], axes[0] );
 		VectorNormalize2( sides[iG2_TRISIDE_SHORTEST], axes[1] );

 		// project shortest side so that it is exactly 90 degrees to the longer side
 		d = DotProduct( axes[0], axes[1] );
 		VectorMA( axes[0], -d, axes[1], axes[0] );
 		VectorNormalize2( axes[0], axes[0] );

 		CrossProduct( sides[iG2_TRISIDE_LONGEST], sides[iG2_TRISIDE_SHORTEST], axes[2] );
 		VectorNormalize2( axes[2], axes[2] );

 		// set up location in world space of the origin point in out going matrix
 		retMatrix.matrix[0][3] = pTri[MDX_TAG_ORIGIN][0];
 		retMatrix.matrix[1][3] = pTri[MDX_TAG_ORIGIN][1];
 		retMatrix.matrix[2][3] = pTri[MDX_TAG_ORIGIN][2];

 		// copy axis to matrix - do some magic to orient minus Y to positive X and so on so bolt on stuff is oriented correctly
		retMatrix.matrix[0][0] = axes[1][0];
		retMatrix.matrix[0][1] = axes[0][0];
		retMatrix.matrix[0][2] = -axes[2][0];

		retMatrix.matrix[1][0] = axes[1][1];
		retMatrix.matrix[1][1] = axes[0][1];
		retMatrix.matrix[1][2] = -axes[2][1];

		retMatrix.matrix[2][0] = axes[1][2];
		retMatrix.matrix[2][1] = axes[0][2];
		retMatrix.matrix[2][2] = -axes[2][2];
	}

}


void G2_GetBoltMatrixLow(CGhoul2Info &ghoul2,int boltNum,const vec3_t scale,mdxaBone_t &retMatrix)
{
	if (!ghoul2.mBoneCache)
	{
		retMatrix=identityMatrix;
		return;
	}
	assert(ghoul2.mBoneCache);
	CBoneCache &boneCache=*ghoul2.mBoneCache;
	assert(boneCache.mod);
	boltInfo_v &boltList=ghoul2.mBltlist;
	assert(boltNum>=0&&boltNum<(int)boltList.size());
	if (boltList[boltNum].boneNumber>=0)
	{
		mdxaSkel_t		*skel;
		mdxaSkelOffsets_t *offsets;
		offsets = (mdxaSkelOffsets_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t));
		skel = (mdxaSkel_t *)((byte *)boneCache.header + sizeof(mdxaHeader_t) + offsets->offsets[boltList[boltNum].boneNumber]);
		Multiply_3x4Matrix(&retMatrix, &boneCache.EvalUnsmooth(boltList[boltNum].boneNumber), &skel->BasePoseMat);
	}
	else if (boltList[boltNum].surfaceNumber>=0)
	{
		const surfaceInfo_t *surfInfo=0;
		{
			for (size_t i=0;i<ghoul2.mSlist.size();i++)
			{
				surfaceInfo_t &t=ghoul2.mSlist[i];
				if (t.surface==boltList[boltNum].surfaceNumber)
				{
					surfInfo=&t;
				}
			}
		}
		mdxmSurface_t *surface = 0;
		if (!surfInfo)
		{
			surface = (mdxmSurface_t *)G2_FindSurface(boneCache.mod,boltList[boltNum].surfaceNumber, 0);
		}
		if (!surface&&surfInfo&&surfInfo->surface<10000)
		{
			surface = (mdxmSurface_t *)G2_FindSurface(boneCache.mod,surfInfo->surface, 0);
		}
		G2_ProcessSurfaceBolt2(boneCache,surface,boltNum,boltList,surfInfo,(model_s *)boneCache.mod,retMatrix);
	}
	else
	{
		 // we have a bolt without a bone or surface, not a huge problem but we ought to at least clear the bolt matrix
		retMatrix=identityMatrix;
	}
}

// sort all the ghoul models in this list so if they go in reference order. This will ensure the bolt on's are attached to the right place
// on the previous model, since it ensures the model being attached to is built and rendered first.

// NOTE!! This assumes at least one model will NOT have a parent. If it does - we are screwed
static void G2_Sort_Models(CGhoul2Info_v &ghoul2, int * const modelList, int * const modelCount)
{
	int		startPoint, endPoint;
	int		i, boltTo, j;

	*modelCount = 0;

	// first walk all the possible ghoul2 models, and stuff the out array with those with no parents
	for (i=0; i<ghoul2.size();i++)
	{
		// have a ghoul model here?
		if (ghoul2[i].mModelindex == -1||!ghoul2[i].mValid)
		{
			continue;
		}
		// are we attached to anything?
		if (ghoul2[i].mModelBoltLink == -1)
		{
			// no, insert us first
			modelList[(*modelCount)++] = i;
	 	}
	}

	startPoint = 0;
	endPoint = *modelCount;

	// now, using that list of parentless models, walk the descendant tree for each of them, inserting the descendents in the list
	while (startPoint != endPoint)
	{
		for (i=0; i<ghoul2.size(); i++)
		{
			// have a ghoul model here?
			if (ghoul2[i].mModelindex == -1||!ghoul2[i].mValid)
			{
				continue;
			}

			// what does this model think it's attached to?
			if (ghoul2[i].mModelBoltLink != -1)
			{
				boltTo = (ghoul2[i].mModelBoltLink >> MODEL_SHIFT) & MODEL_AND;
				// is it any of the models we just added to the list?
				for (j=startPoint; j<endPoint; j++)
				{
					// is this my parent model?
					if (boltTo == modelList[j])
					{
						// yes, insert into list and exit now
						modelList[(*modelCount)++] = i;
						break;
					}
				}
			}
		}
		// update start and end points
		startPoint = endPoint;
		endPoint = *modelCount;
	}
}



static void RootMatrix(CGhoul2Info_v &ghoul2,int time,const vec3_t scale,mdxaBone_t &retMatrix)
{
	int i;
	for (i=0; i<ghoul2.size(); i++)
	{
		if (ghoul2[i].mModelindex != -1&&ghoul2[i].mValid)
		{
			if (ghoul2[i].mFlags & GHOUL2_NEWORIGIN)
			{
				mdxaBone_t bolt;
				mdxaBone_t		tempMatrix;

				G2_ConstructGhoulSkeleton(ghoul2,time,false,scale);
				G2_GetBoltMatrixLow(ghoul2[i],ghoul2[i].mNewOrigin,scale,bolt);
				tempMatrix.matrix[0][0]=1.0f;
				tempMatrix.matrix[0][1]=0.0f;
				tempMatrix.matrix[0][2]=0.0f;
				tempMatrix.matrix[0][3]=-bolt.matrix[0][3];
				tempMatrix.matrix[1][0]=0.0f;
				tempMatrix.matrix[1][1]=1.0f;
				tempMatrix.matrix[1][2]=0.0f;
				tempMatrix.matrix[1][3]=-bolt.matrix[1][3];
				tempMatrix.matrix[2][0]=0.0f;
				tempMatrix.matrix[2][1]=0.0f;
				tempMatrix.matrix[2][2]=1.0f;
				tempMatrix.matrix[2][3]=-bolt.matrix[2][3];
//				Inverse_Matrix(&bolt, &tempMatrix);
				Multiply_3x4Matrix(&retMatrix, &tempMatrix, (mdxaBone_t*)&identityMatrix);
				return;
			}
		}
	}
	retMatrix=identityMatrix;
}

bool G2_NeedsRecalc(CGhoul2Info *ghlInfo,int frameNum)
{
	re.G2ABI_SetupModelPointers(ghlInfo);
	// not sure if I still need this test, probably
	if (ghlInfo->mSkelFrameNum!=frameNum||
		!ghlInfo->mBoneCache||
		ghlInfo->mBoneCache->mod!=ghlInfo->currentModel)
	{
		ghlInfo->mSkelFrameNum=frameNum;
		return true;
	}
	return false;
}

/*
==============
G2_ConstructGhoulSkeleton - builds a complete skeleton for all ghoul models in a CGhoul2Info_v class	- using LOD 0
==============
*/
void G2_ConstructGhoulSkeleton( CGhoul2Info_v &ghoul2,const int frameNum,bool checkForNewOrigin,const vec3_t scale)
{
	int				i, j;
	int				modelCount;
	mdxaBone_t		rootMatrix;

	int modelList[32];
	assert(ghoul2.size()<=31);
	modelList[31]=548;

	if (checkForNewOrigin)
	{
		RootMatrix(ghoul2,frameNum,scale,rootMatrix);
	}
	else
	{
		rootMatrix = identityMatrix;
	}

	G2_Sort_Models(ghoul2, modelList, &modelCount);
	assert(modelList[31]==548);

	for (j=0; j<modelCount; j++)
	{
		// get the sorted model to play with
		i = modelList[j];

		if (ghoul2[i].mValid)
		{
			if (j&&ghoul2[i].mModelBoltLink != -1)
			{
				int	boltMod = (ghoul2[i].mModelBoltLink >> MODEL_SHIFT) & MODEL_AND;
				int	boltNum = (ghoul2[i].mModelBoltLink >> BOLT_SHIFT) & BOLT_AND;

				mdxaBone_t bolt;
				G2_GetBoltMatrixLow(ghoul2[boltMod],boltNum,scale,bolt);
				G2_TransformGhoulBones(ghoul2[i].mBlist,bolt,ghoul2[i],frameNum,checkForNewOrigin);
			}
			else
			{
				G2_TransformGhoulBones(ghoul2[i].mBlist,rootMatrix,ghoul2[i],frameNum,checkForNewOrigin);
			}
		}
	}
}