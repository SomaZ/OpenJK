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
// tr_shade.c

#include "tr_local.h" 
#include "tr_allocator.h"

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

color4ub_t	styleColors[MAX_LIGHT_STYLES];

void RB_BinTriangleCounts( void );


/*
==================
R_DrawElements

==================
*/

void R_DrawElementsVBO( int numIndexes, glIndex_t firstIndex, glIndex_t minIndex, glIndex_t maxIndex )
{
	int offset = firstIndex * sizeof(glIndex_t) +
		(tess.useInternalVBO ? backEndData->currentFrame->dynamicIboCommitOffset : 0);

	GL_DrawIndexed(GL_TRIANGLES, numIndexes, GL_INDEX_TYPE, offset, 1, 0);
}


static void R_DrawMultiElementsVBO( int multiDrawPrimitives, glIndex_t *multiDrawMinIndex, glIndex_t *multiDrawMaxIndex, 
	GLsizei *multiDrawNumIndexes, glIndex_t **multiDrawFirstIndex)
{
	GL_MultiDrawIndexed(
			GL_TRIANGLES,
			multiDrawNumIndexes,
			multiDrawFirstIndex,
			multiDrawPrimitives);
}


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;


/*
=================
R_BindAnimatedImageToTMU

=================
*/
void R_BindAnimatedImageToTMU( textureBundle_t *bundle, int tmu ) {
	int		index;

	if ( bundle->isVideoMap ) {
		int oldtmu = glState.currenttmu;
		GL_SelectTexture(tmu);
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		GL_SelectTexture(oldtmu);
		return;
	}

	if ((r_fullbright->integer || tr.refdef.doLAGoggles || (tr.refdef.rdflags & RDF_doFullbright)) && bundle->isLightmap)
	{
		//GL_Bind(tr.whiteImage);
		GL_BindToTMU(tr.whiteImage, tmu);
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		GL_BindToTMU( bundle->image[0], tmu);
		return;
	}

	if (backEnd.currentEntity->e.renderfx & RF_SETANIMINDEX )
	{
		index = backEnd.currentEntity->e.skinNum;
	}
	else
	{
		// it is necessary to do this messy calc to make sure animations line up
		// exactly with waveforms of the same frequency
		index = Q_ftol( tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
		index >>= FUNCTABLE_SIZE2;

		if ( index < 0 ) {
			index = 0;	// may happen with shader time offsets
		}
	}

	if ( bundle->oneShotAnimMap )
	{
		if ( index >= bundle->numImageAnimations )
		{
			// stick on last frame
			index = bundle->numImageAnimations - 1;
		}
	}
	else
	{
		// loop
		index %= bundle->numImageAnimations;
	}

	GL_BindToTMU( bundle->image[ index ], tmu );
}


/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris (shaderCommands_t *input) {
#if 1
	GL_Bind( tr.whiteImage );

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	GL_DepthRange(0.0f, 0.0f);

	{
		shaderProgram_t *sp = &tr.textureColorShader;
		vec4_t color;

		GLSL_VertexAttribsState(ATTR_POSITION, NULL);
		GLSL_BindProgram(sp);
		
		GLSL_SetUniformMatrix4x4(sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);
		VectorSet4(color, 1, 1, 1, 1);
		GLSL_SetUniformVec4(sp, UNIFORM_COLOR, color);

		if (input->multiDrawPrimitives)
		{
			R_DrawMultiElementsVBO(input->multiDrawPrimitives, input->multiDrawMinIndex, input->multiDrawMaxIndex, input->multiDrawNumIndexes, input->multiDrawFirstIndex);
		}
		else
		{
			R_DrawElementsVBO(input->numIndexes, input->firstIndex, input->minIndex, input->maxIndex);
		}
	}

	GL_DepthRange(0.0f, 1.0f);
#endif
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals (shaderCommands_t *input) {
	//FIXME: implement this
}

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum, int cubemapIndex ) {

	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;

	tess.numIndexes = 0;
	tess.firstIndex = 0;
	tess.numVertexes = 0;
	tess.multiDrawPrimitives = 0;
	tess.shader = state;
	tess.fogNum = fogNum;
	tess.cubemapIndex = cubemapIndex;
	tess.dlightBits = 0;		// will be OR'd in by surface functions
	tess.pshadowBits = 0;       // will be OR'd in by surface functions
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;
	tess.currentStageIteratorFunc = state->optimalStageIteratorFunc;
	tess.externalIBO = nullptr;
	tess.useInternalVBO = qtrue;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime) {
		tess.shaderTime = tess.shader->clampTime;
	}

	if (backEnd.viewParms.flags & VPF_SHADOWMAP)
	{
		tess.currentStageIteratorFunc = RB_StageIteratorGeneric;
	}
}



extern float EvalWaveForm( const waveForm_t *wf );
extern float EvalWaveFormClamped( const waveForm_t *wf );


static void ComputeTexMods( shaderStage_t *pStage, int bundleNum, float *outMatrix, float *outOffTurb)
{
	int tm;
	float matrix[6], currentmatrix[6];
	textureBundle_t *bundle = &pStage->bundle[bundleNum];

	matrix[0] = 1.0f; matrix[2] = 0.0f; matrix[4] = 0.0f;
	matrix[1] = 0.0f; matrix[3] = 1.0f; matrix[5] = 0.0f;

	currentmatrix[0] = 1.0f; currentmatrix[2] = 0.0f; currentmatrix[4] = 0.0f;
	currentmatrix[1] = 0.0f; currentmatrix[3] = 1.0f; currentmatrix[5] = 0.0f;

	outMatrix[0] = 1.0f; outMatrix[2] = 0.0f;
	outMatrix[1] = 0.0f; outMatrix[3] = 1.0f;

	outOffTurb[0] = 0.0f; outOffTurb[1] = 0.0f; outOffTurb[2] = 0.0f; outOffTurb[3] = 0.0f;

	for ( tm = 0; tm < bundle->numTexMods ; tm++ ) {
		switch ( bundle->texMods[tm].type )
		{
			
		case TMOD_NONE:
			tm = TR_MAX_TEXMODS;		// break out of for loop
			break;

		case TMOD_TURBULENT:
			RB_CalcTurbulentFactors(&bundle->texMods[tm].wave, &outOffTurb[2], &outOffTurb[3]);
			break;

		case TMOD_ENTITY_TRANSLATE:
			RB_CalcScrollTexMatrix( backEnd.currentEntity->e.shaderTexCoord, matrix );
			break;

		case TMOD_SCROLL:
			RB_CalcScrollTexMatrix( bundle->texMods[tm].scroll,
									 matrix );
			break;

		case TMOD_SCALE:
			RB_CalcScaleTexMatrix( bundle->texMods[tm].scale,
								  matrix );
			break;
		
		case TMOD_STRETCH:
			RB_CalcStretchTexMatrix( &bundle->texMods[tm].wave, 
								   matrix );
			break;

		case TMOD_TRANSFORM:
			RB_CalcTransformTexMatrix( &bundle->texMods[tm],
									 matrix );
			break;

		case TMOD_ROTATE:
			RB_CalcRotateTexMatrix( bundle->texMods[tm].rotateSpeed,
									matrix );
			break;

		default:
			ri.Error( ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'", bundle->texMods[tm].type, tess.shader->name );
			break;
		}

		switch ( bundle->texMods[tm].type )
		{	
		case TMOD_NONE:
		case TMOD_TURBULENT:
		default:
			break;

		case TMOD_ENTITY_TRANSLATE:
		case TMOD_SCROLL:
		case TMOD_SCALE:
		case TMOD_STRETCH:
		case TMOD_TRANSFORM:
		case TMOD_ROTATE:
			outMatrix[0] = matrix[0] * currentmatrix[0] + matrix[2] * currentmatrix[1];
			outMatrix[1] = matrix[1] * currentmatrix[0] + matrix[3] * currentmatrix[1];

			outMatrix[2] = matrix[0] * currentmatrix[2] + matrix[2] * currentmatrix[3];
			outMatrix[3] = matrix[1] * currentmatrix[2] + matrix[3] * currentmatrix[3];

			outOffTurb[0] = matrix[0] * currentmatrix[4] + matrix[2] * currentmatrix[5] + matrix[4];
			outOffTurb[1] = matrix[1] * currentmatrix[4] + matrix[3] * currentmatrix[5] + matrix[5];

			currentmatrix[0] = outMatrix[0];
			currentmatrix[1] = outMatrix[1];
			currentmatrix[2] = outMatrix[2];
			currentmatrix[3] = outMatrix[3];
			currentmatrix[4] = outOffTurb[0];
			currentmatrix[5] = outOffTurb[1];
			break;
		}
	}
}


static void ComputeDeformValues(deform_t *type, genFunc_t *waveFunc, float deformParams[7])
{
	// u_DeformGen
	*type = DEFORM_NONE;
	*waveFunc = GF_NONE;

	if(!ShaderRequiresCPUDeforms(tess.shader))
	{
		deformStage_t  *ds;

		// only support the first one
		ds = &tess.shader->deforms[0];

		switch (ds->deformation)
		{
			case DEFORM_WAVE:
				*type = DEFORM_WAVE;
				*waveFunc = ds->deformationWave.func;

				deformParams[0] = ds->deformationWave.base;
				deformParams[1] = ds->deformationWave.amplitude;
				deformParams[2] = ds->deformationWave.phase;
				deformParams[3] = ds->deformationWave.frequency;
				deformParams[4] = ds->deformationSpread;
				deformParams[5] = 0.0f;
				deformParams[6] = 0.0f;
				break;

			case DEFORM_BULGE:
				*type = DEFORM_BULGE;

				deformParams[0] = 0.0f;
				deformParams[1] = ds->bulgeHeight; // amplitude
				deformParams[2] = ds->bulgeWidth;  // phase
				deformParams[3] = ds->bulgeSpeed;  // frequency
				deformParams[4] = 0.0f;
				deformParams[5] = 0.0f;
				deformParams[6] = 0.0f;
				break;

			case DEFORM_MOVE:
				*type = DEFORM_MOVE;
				*waveFunc = ds->deformationWave.func;

				deformParams[0] = ds->deformationWave.base;
				deformParams[1] = ds->deformationWave.amplitude;
				deformParams[2] = ds->deformationWave.phase;
				deformParams[3] = ds->deformationWave.frequency;
				deformParams[4] = ds->moveVector[0];
				deformParams[5] = ds->moveVector[1];
				deformParams[6] = ds->moveVector[2];

				break;

			case DEFORM_NORMALS:
				*type = DEFORM_NORMALS;

				deformParams[0] = 0.0f;
				deformParams[1] = ds->deformationWave.amplitude; // amplitude
				deformParams[2] = 0.0f;  // phase
				deformParams[3] = ds->deformationWave.frequency;  // frequency
				deformParams[4] = 0.0f;
				deformParams[5] = 0.0f;
				deformParams[6] = 0.0f;
				break;

			case DEFORM_PROJECTION_SHADOW:
				*type = DEFORM_PROJECTION_SHADOW;

				deformParams[0] = backEnd.ori.axis[0][2];
				deformParams[1] = backEnd.ori.axis[1][2];
				deformParams[2] = backEnd.ori.axis[2][2];
				deformParams[3] = backEnd.ori.origin[2] - backEnd.currentEntity->e.shadowPlane;
				deformParams[4] = backEnd.currentEntity->modelLightDir[0];
				deformParams[5] = backEnd.currentEntity->modelLightDir[1];
				deformParams[6] = backEnd.currentEntity->modelLightDir[2];
				break;

			default:
				break;
		}
	}
}

static void ComputeShaderColors( shaderStage_t *pStage, vec4_t baseColor, vec4_t vertColor, int blend, colorGen_t *forceRGBGen, alphaGen_t *forceAlphaGen )
{
	colorGen_t rgbGen = pStage->rgbGen;
	alphaGen_t alphaGen = pStage->alphaGen;

	baseColor[0] =  
   	baseColor[1] = 
   	baseColor[2] = 
   	baseColor[3] = 1.0f; 
   	
   	vertColor[0] = 
   	vertColor[1] = 
   	vertColor[2] = 
   	vertColor[3] = 0.0f;

	if ( forceRGBGen != NULL && *forceRGBGen != CGEN_BAD )
	{
		rgbGen = *forceRGBGen;
	}

	if ( forceAlphaGen != NULL && *forceAlphaGen != AGEN_IDENTITY )
	{
		alphaGen = *forceAlphaGen;
	}

	switch ( rgbGen )
	{
		case CGEN_IDENTITY_LIGHTING:
			baseColor[0] = 
			baseColor[1] =
			baseColor[2] = tr.identityLight;
			break;
		case CGEN_EXACT_VERTEX:
		case CGEN_EXACT_VERTEX_LIT:
			baseColor[0] = 
			baseColor[1] =
			baseColor[2] = 
			baseColor[3] = 0.0f;

			vertColor[0] =
			vertColor[1] =
			vertColor[2] = 
			vertColor[3] = 1.0f;
			break;
		case CGEN_CONST:
			baseColor[0] = pStage->constantColor[0] / 255.0f;
			baseColor[1] = pStage->constantColor[1] / 255.0f;
			baseColor[2] = pStage->constantColor[2] / 255.0f;
			baseColor[3] = pStage->constantColor[3] / 255.0f;
			break;
		case CGEN_VERTEX:
			baseColor[0] = 
			baseColor[1] =
			baseColor[2] =
			baseColor[3] = 0.0f;

			vertColor[0] =
			vertColor[1] =
			vertColor[2] = tr.identityLight;
			vertColor[3] = 1.0f;
			break;
		case CGEN_VERTEX_LIT:
			baseColor[0] = 
			baseColor[1] =
			baseColor[2] = 
			baseColor[3] = 0.0f;

			vertColor[0] =
			vertColor[1] =
			vertColor[2] = 
			vertColor[3] = tr.identityLight;
			break;
		case CGEN_ONE_MINUS_VERTEX:
			baseColor[0] = 
			baseColor[1] =
			baseColor[2] = tr.identityLight;

			vertColor[0] =
			vertColor[1] =
			vertColor[2] = -tr.identityLight;
			break;
		case CGEN_FOG:
			{
				fog_t *fog = tr.world->fogs + tess.fogNum;
				VectorCopy4(fog->color, baseColor);
			}
			break;
		case CGEN_WAVEFORM:
			baseColor[0] = 
			baseColor[1] = 
			baseColor[2] = RB_CalcWaveColorSingle( &pStage->rgbWave );
			break;
		case CGEN_ENTITY:
		case CGEN_LIGHTING_DIFFUSE_ENTITY:
			if (backEnd.currentEntity)
			{
				baseColor[0] = ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[0] / 255.0f;
				baseColor[1] = ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[1] / 255.0f;
				baseColor[2] = ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[2] / 255.0f;
				baseColor[3] = ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[3] / 255.0f;

				if ( alphaGen == AGEN_IDENTITY &&
					backEnd.currentEntity->e.shaderRGBA[3] == 255 )
				{
					alphaGen = AGEN_SKIP;
				}
			}
			break;
		case CGEN_ONE_MINUS_ENTITY:
			if (backEnd.currentEntity)
			{
				baseColor[0] = 1.0f - ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[0] / 255.0f;
				baseColor[1] = 1.0f - ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[1] / 255.0f;
				baseColor[2] = 1.0f - ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[2] / 255.0f;
				baseColor[3] = 1.0f - ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[3] / 255.0f;
			}
			break;
		case CGEN_LIGHTMAPSTYLE:
			VectorScale4 (styleColors[pStage->lightmapStyle], 1.0f / 255.0f, baseColor);
			break;
		case CGEN_IDENTITY:
		case CGEN_LIGHTING_DIFFUSE:
		case CGEN_BAD:
			break;
	}

	//
	// alphaGen
	//
	switch ( alphaGen )
	{
		case AGEN_SKIP:
			break;
		case AGEN_CONST:
			if ( rgbGen != CGEN_CONST ) {
				baseColor[3] = pStage->constantColor[3] / 255.0f;
				vertColor[3] = 0.0f;
			}
			break;
		case AGEN_WAVEFORM:
			baseColor[3] = RB_CalcWaveAlphaSingle( &pStage->alphaWave );
			vertColor[3] = 0.0f;
			break;
		case AGEN_ENTITY:
			if (backEnd.currentEntity)
			{
				baseColor[3] = ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[3] / 255.0f;
			}
			vertColor[3] = 0.0f;
			break;
		case AGEN_ONE_MINUS_ENTITY:
			if (backEnd.currentEntity)
			{
				baseColor[3] = 1.0f - ((unsigned char *)backEnd.currentEntity->e.shaderRGBA)[3] / 255.0f;
			}
			vertColor[3] = 0.0f;
			break;
		case AGEN_VERTEX:
			if ( rgbGen != CGEN_VERTEX ) {
				baseColor[3] = 0.0f;
				vertColor[3] = 1.0f;
			}
			break;
		case AGEN_ONE_MINUS_VERTEX:
			baseColor[3] = 1.0f;
			vertColor[3] = -1.0f;
			break;
		case AGEN_IDENTITY:
		case AGEN_LIGHTING_SPECULAR:
		case AGEN_PORTAL:
			// Done entirely in vertex program
			baseColor[3] = 1.0f;
			vertColor[3] = 0.0f;
			break;
	}

	if ( forceAlphaGen != NULL )
	{
		*forceAlphaGen = alphaGen;
	}

	if ( forceRGBGen != NULL )
	{
		*forceRGBGen = rgbGen;
	}
	
	// multiply color by overbrightbits if this isn't a blend
	if (tr.overbrightBits 
	 && !((blend & GLS_SRCBLEND_BITS) == GLS_SRCBLEND_DST_COLOR)
	 && !((blend & GLS_SRCBLEND_BITS) == GLS_SRCBLEND_ONE_MINUS_DST_COLOR)
	 && !((blend & GLS_DSTBLEND_BITS) == GLS_DSTBLEND_SRC_COLOR)
	 && !((blend & GLS_DSTBLEND_BITS) == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR))
	{
		float scale = 1 << tr.overbrightBits;

		baseColor[0] *= scale;
		baseColor[1] *= scale;
		baseColor[2] *= scale;
		vertColor[0] *= scale;
		vertColor[1] *= scale;
		vertColor[2] *= scale;
	}

	// FIXME: find some way to implement this.
#if 0
	// if in greyscale rendering mode turn all color values into greyscale.
	if(r_greyscale->integer)
	{
		int scale;
		
		for(i = 0; i < tess.numVertexes; i++)
		{
			scale = (tess.svars.colors[i][0] + tess.svars.colors[i][1] + tess.svars.colors[i][2]) / 3;
			tess.svars.colors[i][0] = tess.svars.colors[i][1] = tess.svars.colors[i][2] = scale;
		}
	}
#endif
}


static void ComputeFogValues(vec4_t fogDistanceVector, vec4_t fogDepthVector, float *eyeT)
{
	// from RB_CalcFogTexCoords()
	fog_t  *fog;
	vec3_t  local;

	if (!tess.fogNum)
		return;

	fog = tr.world->fogs + tess.fogNum;

	VectorSubtract( backEnd.ori.origin, backEnd.viewParms.ori.origin, local );
	fogDistanceVector[0] = -backEnd.ori.modelViewMatrix[2];
	fogDistanceVector[1] = -backEnd.ori.modelViewMatrix[6];
	fogDistanceVector[2] = -backEnd.ori.modelViewMatrix[10];
	fogDistanceVector[3] = DotProduct( local, backEnd.viewParms.ori.axis[0] );

	// scale the fog vectors based on the fog's thickness
	VectorScale4(fogDistanceVector, fog->tcScale, fogDistanceVector);

	// rotate the gradient vector for this orientation
	if ( fog->hasSurface ) {
		fogDepthVector[0] = fog->surface[0] * backEnd.ori.axis[0][0] + 
			fog->surface[1] * backEnd.ori.axis[0][1] + fog->surface[2] * backEnd.ori.axis[0][2];
		fogDepthVector[1] = fog->surface[0] * backEnd.ori.axis[1][0] + 
			fog->surface[1] * backEnd.ori.axis[1][1] + fog->surface[2] * backEnd.ori.axis[1][2];
		fogDepthVector[2] = fog->surface[0] * backEnd.ori.axis[2][0] + 
			fog->surface[1] * backEnd.ori.axis[2][1] + fog->surface[2] * backEnd.ori.axis[2][2];
		fogDepthVector[3] = -fog->surface[3] + DotProduct( backEnd.ori.origin, fog->surface );

		*eyeT = DotProduct( backEnd.ori.viewOrigin, fogDepthVector ) + fogDepthVector[3];
	} else {
		VectorClear4(fogDepthVector);
		*eyeT = 1;	// non-surface fog always has eye inside
	}
}


static void ComputeFogColorMask( shaderStage_t *pStage, vec4_t fogColorMask )
{
	switch(pStage->adjustColorsForFog)
	{
		case ACFF_MODULATE_RGB:
			fogColorMask[0] =
			fogColorMask[1] =
			fogColorMask[2] = 1.0f;
			fogColorMask[3] = 0.0f;
			break;
		case ACFF_MODULATE_ALPHA:
			fogColorMask[0] =
			fogColorMask[1] =
			fogColorMask[2] = 0.0f;
			fogColorMask[3] = 1.0f;
			break;
		case ACFF_MODULATE_RGBA:
			fogColorMask[0] =
			fogColorMask[1] =
			fogColorMask[2] =
			fogColorMask[3] = 1.0f;
			break;
		default:
			fogColorMask[0] =
			fogColorMask[1] =
			fogColorMask[2] =
			fogColorMask[3] = 0.0f;
			break;
	}
}

static void CaptureDrawData(const shaderCommands_t *input, shaderStage_t *stage, int glslShaderIndex, int stageIndex )
{
	if ( !tr.numFramesToCapture )
		return;

	if ( input->multiDrawPrimitives )
	{
		int numIndexes = 0;
		for ( int i = 0; i < input->multiDrawPrimitives; i++ )
			numIndexes += input->multiDrawNumIndexes[i];

		const char *data = va("%d,%d,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,Y\n",
				tr.frameCount,
				backEnd.currentEntity == &tr.worldEntity ? -1 : (backEnd.currentEntity - tr.refdef.entities),
				stage->glslShaderGroup ? "lightall" : "generic", glslShaderIndex,
				input->shader->name, stageIndex, input->shader->sortedIndex, (int)input->shader->sort,
				input->fogNum,
				input->cubemapIndex,
				glState.vertexAttribsState,
				glState.glStateBits,
				glState.currentVBO->vertexesVBO,
				glState.currentIBO->indexesVBO,
				numIndexes / 3);
		ri.FS_Write(data, strlen(data), tr.debugFile);
	}
	else
	{
		const char *data = va("%d,%d,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,N\n",
				tr.frameCount,
				backEnd.currentEntity == &tr.worldEntity ? -1 : (backEnd.currentEntity - tr.refdef.entities),
				stage->glslShaderGroup ? "lightall" : "generic", glslShaderIndex,
				input->shader->name, stageIndex, input->shader->sortedIndex, (int)input->shader->sort,
				input->fogNum,
				input->cubemapIndex,
				glState.vertexAttribsState,
				glState.glStateBits,
				glState.currentVBO->vertexesVBO,
				glState.currentIBO->indexesVBO,
				input->numIndexes / 3);
		ri.FS_Write(data, strlen(data), tr.debugFile);
	}
}

uint32_t RB_CreateSortKey( const DrawItem& item, int stage, int layer )
{
	uint32_t key = 0;
	uintptr_t shaderProgram = (uintptr_t)item.program;

	assert(stage < 16);
	layer = Q_min(layer, 15);

	key |= (layer & 0xf) << 28;
	key |= (stage & 0xf) << 24;
	key |= shaderProgram & 0x00ffffff;
	return key;
}

static cullType_t RB_GetCullType( const viewParms_t *viewParms, const trRefEntity_t *refEntity, cullType_t shaderCullType )
{
	assert(refEntity);

	cullType_t cullType = CT_TWO_SIDED;
	if ( !backEnd.projection2D )
	{
		if ( shaderCullType != CT_TWO_SIDED )
		{
			bool cullFront = (shaderCullType == CT_FRONT_SIDED);
			if ( viewParms->isMirror )
				cullFront = !cullFront;

			if ( refEntity->mirrored )
				cullFront = !cullFront;

			if ( viewParms->flags & VPF_DEPTHSHADOW )
				cullFront = !cullFront;

			cullType = (cullFront ? CT_FRONT_SIDED : CT_BACK_SIDED);
		}
	}

	return cullType;
}

DepthRange RB_GetDepthRange( const trRefEntity_t *re, const shader_t *shader )
{
	DepthRange range = {0.0f, 1.0f};
	if ( shader->isSky )
	{
		// r_showsky will let all the sky blocks be drawn in
		// front of everything to allow developers to see how
		// much sky is getting sucked in
		if ( !r_showsky->integer )
		{
			range.minDepth = 1.0f;
			range.maxDepth = 1.0f;
		}
		else
		{
			range.maxDepth = 0.0f;
		}
	}
	else if ( re->e.renderfx & RF_NODEPTH )
	{
		range.maxDepth = 0.0f;
	}
	else if ( re->e.renderfx & RF_DEPTHHACK )
	{
		range.maxDepth = 0.3f;
	}

	return range;
}

void RB_FillDrawCommand(
	DrawCommand& drawCmd,
	GLenum primitiveType,
	int numInstances,
	const shaderCommands_t *input
)
{
	drawCmd.primitiveType = primitiveType;
	drawCmd.numInstances = numInstances;

	if ( input->multiDrawPrimitives )
	{
		if ( input->multiDrawPrimitives == 1 )
		{
			drawCmd.type = DRAW_COMMAND_INDEXED;
			drawCmd.params.indexed.indexType = GL_INDEX_TYPE;
			drawCmd.params.indexed.firstIndex = (glIndex_t)(size_t)(input->multiDrawFirstIndex[0]);
			drawCmd.params.indexed.numIndices = input->multiDrawNumIndexes[0];
		}
		else
		{
			drawCmd.type = DRAW_COMMAND_MULTI_INDEXED;
			drawCmd.params.multiIndexed.numDraws = input->multiDrawPrimitives;

			drawCmd.params.multiIndexed.firstIndices =
				ojkAllocArray<glIndex_t *>(*backEndData->perFrameMemory, input->multiDrawPrimitives);
			memcpy(drawCmd.params.multiIndexed.firstIndices,
				input->multiDrawFirstIndex,
				sizeof(glIndex_t *) * input->multiDrawPrimitives);

			drawCmd.params.multiIndexed.numIndices =
				ojkAllocArray<GLsizei>(*backEndData->perFrameMemory, input->multiDrawPrimitives);
			memcpy(drawCmd.params.multiIndexed.numIndices,
				input->multiDrawNumIndexes,
				sizeof(GLsizei *) * input->multiDrawPrimitives);
		}
	}
	else
	{
		int offset = input->firstIndex * sizeof(glIndex_t) +
			(input->useInternalVBO ? backEndData->currentFrame->dynamicIboCommitOffset : 0);

		drawCmd.type = DRAW_COMMAND_INDEXED;
		drawCmd.params.indexed.indexType = GL_INDEX_TYPE;
		drawCmd.params.indexed.firstIndex = offset;
		drawCmd.params.indexed.numIndices = input->numIndexes;
	}
}

static void ForwardDlight( const shaderCommands_t *input,  VertexArraysProperties *vertexArrays )
{
	deform_t deformType;
	genFunc_t deformGen;
	float deformParams[7];

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}
	
	ComputeDeformValues(&deformType, &deformGen, deformParams);

	cullType_t cullType = RB_GetCullType(&backEnd.viewParms, backEnd.currentEntity, input->shader->cullType);

	vertexAttribute_t attribs[ATTR_INDEX_MAX] = {};
	GL_VertexArraysToAttribs(attribs, ARRAY_LEN(attribs), vertexArrays);

	UniformDataWriter uniformDataWriter;
	SamplerBindingsWriter samplerBindingsWriter;

	shaderStage_t *pStage = tess.xstages[0];

	// FIXME: Should happen at stage generation instead
	for (int i = 0; i < MAX_SHADER_STAGES; i++)
	{
		if (tess.xstages[i] && (tess.xstages[i]->bundle[TB_NORMALMAP].image[0] || tess.xstages[i]->bundle[TB_SPECULARMAP].image[0])) {
			pStage = tess.xstages[i];
		}
	}

	if (!pStage)
		return;

	int index;
	shaderProgram_t *shaderGroup;
	uint32_t stateBits = 0;

	if(1)
	{
		index = pStage->glslShaderIndex;

		stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		shaderGroup = tr.lightallShader;
		index &= ~LIGHTDEF_LIGHTTYPE_MASK;
		index |= LIGHTDEF_USE_LIGHT_VECTOR;

		if (pStage->stateBits & GLS_POLYGON_OFFSET_FILL)
			stateBits |= GLS_POLYGON_OFFSET_FILL;

		if (glState.vertexAnimation)
			index |= LIGHTDEF_USE_VERTEX_ANIMATION;

		if (glState.skeletalAnimation)
			index |= LIGHTDEF_USE_SKELETAL_ANIMATION;
	}
	else
	{
		index = 0;

		stateBits = GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		shaderGroup = tr.dlightShader;
		if ( deformGen != DGEN_NONE )
			index |= DLIGHTDEF_USE_DEFORM_VERTEXES;
	}

	shaderProgram_t *sp = shaderGroup + index;
	for (int l = 0; l < backEnd.refdef.num_dlights; l++) {
		vec4_t texMatrix;
		vec4_t texOffTurb;

		if (!(tess.dlightBits & (1 << l))) {
			continue;	// this surface definately doesn't have any of this light
		}

		dlight_t *dl = &backEnd.refdef.dlights[l];
		float radius = dl->radius;

		backEnd.pc.c_lightallDraws++;

		uniformDataWriter.Start(sp);

		uniformDataWriter.SetUniformMatrix4x4(UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);
		uniformDataWriter.SetUniformVec3(UNIFORM_VIEWORIGIN, backEnd.viewParms.ori.origin);
		uniformDataWriter.SetUniformVec3(UNIFORM_LOCALVIEWORIGIN, backEnd.ori.viewOrigin);

		uniformDataWriter.SetUniformFloat(UNIFORM_VERTEXLERP, glState.vertexAttribsInterpolation);
		uniformDataWriter.SetUniformMatrix4x3(UNIFORM_BONE_MATRICES, &glState.boneMatrices[0][0], glState.numBones);

		uniformDataWriter.SetUniformInt(UNIFORM_DEFORMTYPE, deformType);
		if (deformType != DEFORM_NONE)
		{
			uniformDataWriter.SetUniformInt(UNIFORM_DEFORMFUNC, deformGen);
			uniformDataWriter.SetUniformFloat(UNIFORM_DEFORMPARAMS, deformParams, 7);
			uniformDataWriter.SetUniformFloat(UNIFORM_TIME, tess.shaderTime);
		}

		{
			vec4_t baseColor;
			vec4_t vertColor;

			ComputeShaderColors(pStage, baseColor, vertColor, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, NULL, NULL);

			uniformDataWriter.SetUniformVec4(UNIFORM_BASECOLOR, baseColor);
			uniformDataWriter.SetUniformVec4(UNIFORM_VERTCOLOR, vertColor);
		}

		if (pStage->alphaGen == AGEN_PORTAL)
		{
			uniformDataWriter.SetUniformFloat(UNIFORM_PORTALRANGE, tess.shader->portalRange);
		}

		uniformDataWriter.SetUniformInt(UNIFORM_COLORGEN, pStage->rgbGen);
		uniformDataWriter.SetUniformInt(UNIFORM_ALPHAGEN, pStage->alphaGen);

		uniformDataWriter.SetUniformVec3(UNIFORM_DIRECTEDLIGHT, dl->color);

		vec4_t vector = {};
		uniformDataWriter.SetUniformVec3(UNIFORM_AMBIENTLIGHT, vector);

		VectorCopy(dl->origin, vector);
		vector[3] = 1.0f;
		uniformDataWriter.SetUniformVec4(UNIFORM_LIGHTORIGIN, vector);
		uniformDataWriter.SetUniformFloat(UNIFORM_LIGHTRADIUS, radius);

		uniformDataWriter.SetUniformVec4(UNIFORM_NORMALSCALE, pStage->normalScale);

		vec4_t realTimeSpecGloss;
		realTimeSpecGloss[0] = pStage->specularScale[0];
		realTimeSpecGloss[1] = pStage->specularScale[1];
		realTimeSpecGloss[2] = pStage->specularScale[2];
		realTimeSpecGloss[3] = pStage->specularScale[3] * r_glossScale->value;
		uniformDataWriter.SetUniformVec4(UNIFORM_SPECULARSCALE, realTimeSpecGloss);

		matrix_t invModelMatrix;
		matrix_t transInvModelMatrix;
		Matrix16Inverse(backEnd.ori.modelMatrix, invModelMatrix);
		Matrix16Transpose(invModelMatrix, transInvModelMatrix);

		uniformDataWriter.SetUniformMatrix4x4(UNIFORM_MODELMATRIX, backEnd.ori.modelMatrix);
		uniformDataWriter.SetUniformMatrix4x4(UNIFORM_NORMALMATRIX, transInvModelMatrix);

		if (pStage->bundle[TB_DIFFUSEMAP].image[0])
			samplerBindingsWriter.AddAnimatedImage( &pStage->bundle[TB_DIFFUSEMAP], TB_DIFFUSEMAP);

		// bind textures that are sampled and used in the glsl shader, and
		// bind whiteImage to textures that are sampled but zeroed in the glsl shader
		//
		// alternatives:
		//  - use the last bound texture
		//     -> costs more to sample a higher res texture then throw out the result
		//  - disable texture sampling in glsl shader with #ifdefs, as before
		//     -> increases the number of shaders that must be compiled
		//

		if (pStage->bundle[TB_NORMALMAP].image[0])
			samplerBindingsWriter.AddAnimatedImage( &pStage->bundle[TB_NORMALMAP], TB_NORMALMAP);
		else if (r_normalMapping->integer)
			samplerBindingsWriter.AddStaticImage( tr.whiteImage, TB_NORMALMAP );

		if (pStage->bundle[TB_SPECULARMAP].image[0])
			samplerBindingsWriter.AddAnimatedImage( &pStage->bundle[TB_SPECULARMAP], TB_SPECULARMAP);
		else if (r_specularMapping->integer)
			samplerBindingsWriter.AddStaticImage( tr.whiteImage, TB_SPECULARMAP );

		vec4_t enableTextures = {};
		uniformDataWriter.SetUniformVec4(UNIFORM_ENABLETEXTURES, enableTextures);

		if (r_dlightMode->integer >= 2)
			samplerBindingsWriter.AddStaticImage(tr.shadowCubemaps[l].image, TB_SHADOWMAP2);

		ComputeTexMods( pStage, TB_DIFFUSEMAP, texMatrix, texOffTurb );
		uniformDataWriter.SetUniformVec4(UNIFORM_DIFFUSETEXMATRIX, texMatrix);
		uniformDataWriter.SetUniformVec4(UNIFORM_DIFFUSETEXOFFTURB, texOffTurb);

		uniformDataWriter.SetUniformInt(UNIFORM_TCGEN0, pStage->bundle[0].tcGen);
		uniformDataWriter.SetUniformInt(UNIFORM_TCGEN1, pStage->bundle[1].tcGen);

		int alphaTestFunction = pStage->alphaTestCmp;
		uniformDataWriter.SetUniformInt(UNIFORM_ALPHA_TEST_FUNCTION, alphaTestFunction);
		float alphaTestValue = pStage->alphaTestValue;
		uniformDataWriter.SetUniformFloat(UNIFORM_ALPHA_TEST_VALUE, alphaTestValue);

		CaptureDrawData(input, pStage, 0, 0);

		DrawItem item = {};

		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		item.renderState.stateBits = stateBits;
		item.renderState.cullType = cullType;
		item.renderState.depthRange = RB_GetDepthRange(backEnd.currentEntity, input->shader);
		item.program = sp;
		item.ibo = input->externalIBO ? input->externalIBO : backEndData->currentFrame->dynamicIbo;

		item.numAttributes = vertexArrays->numVertexArrays;
		item.attributes = ojkAllocArray<vertexAttribute_t>(
			*backEndData->perFrameMemory, vertexArrays->numVertexArrays);
		memcpy(item.attributes, attribs, sizeof(*item.attributes)*vertexArrays->numVertexArrays);

		item.uniformData = uniformDataWriter.Finish(*backEndData->perFrameMemory);
		// FIXME: This is a bit ugly with the casting
		item.samplerBindings = samplerBindingsWriter.Finish(
			*backEndData->perFrameMemory, (int *)&item.numSamplerBindings);

		RB_FillDrawCommand(item.draw, GL_TRIANGLES, 1, input);

		uint32_t key = RB_CreateSortKey(item, 15, input->shader->sort);
		RB_AddDrawItem(backEndData->currentPass, key, item);

		backEnd.pc.c_totalIndexes += tess.numIndexes;
		backEnd.pc.c_dlightIndexes += tess.numIndexes;
		backEnd.pc.c_dlightVertexes += tess.numVertexes;

		RB_BinTriangleCounts();
	}
}


static void ProjectPshadowVBOGLSL( const shaderCommands_t *input, const VertexArraysProperties *vertexArrays) {
	int		l;
	vec3_t	origin;
	float	radius;

	if ( !backEnd.refdef.num_pshadows ) {
		return;
	}

	cullType_t cullType = RB_GetCullType(&backEnd.viewParms, backEnd.currentEntity, input->shader->cullType);

	UniformDataWriter uniformDataWriter;
	SamplerBindingsWriter samplerBindingsWriter;
	shaderStage_t *pStage = tess.xstages[0];

	vertexAttribute_t attribs[ATTR_INDEX_MAX] = {};
	GL_VertexArraysToAttribs(attribs, ARRAY_LEN(attribs), vertexArrays);

	for ( l = 0 ; l < backEnd.refdef.num_pshadows ; l++ ) {
		pshadow_t	*ps;
		shaderProgram_t *sp;
		vec4_t vector;
		vec3_t vector2;

		if ( !( tess.pshadowBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this shadow
		}

		ps = &backEnd.refdef.pshadows[l];
		VectorCopy( ps->lightOrigin, origin );
		radius = ps->lightRadius;

		sp = &tr.pshadowShader;

		uniformDataWriter.Start(sp);

		uniformDataWriter.SetUniformMatrix4x4(UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

		VectorCopy(origin, vector);
		vector[3] = 1.0f;
		uniformDataWriter.SetUniformVec4(UNIFORM_LIGHTORIGIN, vector);

		VectorCopy(origin, vector2);
		VectorScale(ps->lightViewAxis[0], 1.0f / ps->viewRadius, vector2);
		uniformDataWriter.SetUniformVec3(UNIFORM_LIGHTFORWARD, vector2);

		VectorScale(ps->lightViewAxis[1], 1.0f / ps->viewRadius, vector2);
		uniformDataWriter.SetUniformVec3(UNIFORM_LIGHTRIGHT, vector2);

		VectorScale(ps->lightViewAxis[2], 1.0f / ps->viewRadius, vector2);
		uniformDataWriter.SetUniformVec3(UNIFORM_LIGHTUP, vector2);

		uniformDataWriter.SetUniformFloat(UNIFORM_LIGHTRADIUS, radius);
	  
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		uint32_t stateBits = 0;
		stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL;

		samplerBindingsWriter.AddStaticImage(tr.pshadowMaps[l], TB_DIFFUSEMAP);

		CaptureDrawData(input, pStage, 0, 0);

		DrawItem item = {};

		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		item.renderState.stateBits = stateBits;
		item.renderState.cullType = cullType;
		item.renderState.depthRange = RB_GetDepthRange(backEnd.currentEntity, input->shader);
		item.program = sp;
		item.ibo = input->externalIBO ? input->externalIBO : backEndData->currentFrame->dynamicIbo;

		item.numAttributes = vertexArrays->numVertexArrays;
		item.attributes = ojkAllocArray<vertexAttribute_t>(
			*backEndData->perFrameMemory, vertexArrays->numVertexArrays);
		memcpy(item.attributes, attribs, sizeof(*item.attributes)*vertexArrays->numVertexArrays);

		item.uniformData = uniformDataWriter.Finish(*backEndData->perFrameMemory);
		// FIXME: This is a bit ugly with the casting
		item.samplerBindings = samplerBindingsWriter.Finish(
			*backEndData->perFrameMemory, (int *)&item.numSamplerBindings);

		RB_FillDrawCommand(item.draw, GL_TRIANGLES, 1, input);

		uint32_t key = RB_CreateSortKey(item, 14, input->shader->sort);
		RB_AddDrawItem(backEndData->currentPass, key, item);

		backEnd.pc.c_totalIndexes += tess.numIndexes;
		//backEnd.pc.c_dlightIndexes += tess.numIndexes;
		RB_BinTriangleCounts();
	}
}



/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( shaderCommands_t *input, const fog_t *fog, const VertexArraysProperties *vertexArrays )
{
	shaderProgram_t *sp;

	deform_t deformType;
	genFunc_t deformGen;
	vec5_t deformParams;

	ComputeDeformValues(&deformType, &deformGen, deformParams);

	cullType_t cullType = CT_FRONT_SIDED;

	vertexAttribute_t attribs[ATTR_INDEX_MAX] = {};
	GL_VertexArraysToAttribs(attribs, ARRAY_LEN(attribs), vertexArrays);

	UniformDataWriter uniformDataWriter;

	int shaderBits = 0;

	if (deformGen != DGEN_NONE)
		shaderBits |= FOGDEF_USE_DEFORM_VERTEXES;

	if (glState.vertexAnimation)
		shaderBits |= FOGDEF_USE_VERTEX_ANIMATION;

	if (glState.skeletalAnimation)
		shaderBits |= FOGDEF_USE_SKELETAL_ANIMATION;
	
	sp = tr.fogShader + shaderBits;
	uniformDataWriter.Start(sp);

	backEnd.pc.c_fogDraws++;

	uniformDataWriter.SetUniformMatrix4x4(UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

	matrix_t invModelMatrix;
	matrix_t transInvModelMatrix;
	Matrix16Inverse(backEnd.ori.modelMatrix, invModelMatrix);
	Matrix16Transpose(invModelMatrix, transInvModelMatrix);

	uniformDataWriter.SetUniformMatrix4x4(UNIFORM_MODELMATRIX, backEnd.ori.modelMatrix);
	uniformDataWriter.SetUniformMatrix4x4(UNIFORM_NORMALMATRIX, transInvModelMatrix);

	uniformDataWriter.SetUniformFloat(UNIFORM_VERTEXLERP, glState.vertexAttribsInterpolation);
	uniformDataWriter.SetUniformMatrix4x3(UNIFORM_BONE_MATRICES, &glState.boneMatrices[0][0], glState.numBones);
	
	uniformDataWriter.SetUniformInt(UNIFORM_DEFORMTYPE, deformType);
	if (deformType != DEFORM_NONE)
	{
		uniformDataWriter.SetUniformInt(UNIFORM_DEFORMFUNC, deformGen);
		uniformDataWriter.SetUniformFloat(UNIFORM_DEFORMPARAMS, deformParams, 7);
		uniformDataWriter.SetUniformFloat(UNIFORM_TIME, tess.shaderTime);
	}

	uniformDataWriter.SetUniformVec4(UNIFORM_COLOR, fog->color);
	uniformDataWriter.SetUniformVec4(UNIFORM_FOGPLANE, fog->surface);
	uniformDataWriter.SetUniformInt(UNIFORM_FOGHASPLANE, fog->hasSurface);
	uniformDataWriter.SetUniformFloat(UNIFORM_FOGDEPTHTOOPAQUE, sqrtf(-logf(1.0f / 255.0f)) / fog->parms.depthForOpaque);
	uniformDataWriter.SetUniformVec3(UNIFORM_VIEWORIGIN, backEnd.refdef.vieworg);

	uint32_t stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	if ( tess.shader->fogPass == FP_EQUAL )
		stateBits |= GLS_DEPTHFUNC_EQUAL;

	if (tess.shader->polygonOffset)
		stateBits |= GLS_POLYGON_OFFSET_FILL;

	DrawItem item = {};
	item.renderState.stateBits = stateBits;
	item.renderState.cullType = cullType;
	item.renderState.depthRange = RB_GetDepthRange(backEnd.currentEntity, input->shader);
	item.program = sp;
	item.ibo = input->externalIBO ? input->externalIBO : backEndData->currentFrame->dynamicIbo;

	item.numAttributes = vertexArrays->numVertexArrays;
	item.attributes = ojkAllocArray<vertexAttribute_t>(
		*backEndData->perFrameMemory, vertexArrays->numVertexArrays);
	memcpy(item.attributes, attribs, sizeof(*item.attributes)*vertexArrays->numVertexArrays);

	item.uniformData = uniformDataWriter.Finish(*backEndData->perFrameMemory);

	RB_FillDrawCommand(item.draw, GL_TRIANGLES, 1, input);

	uint32_t key = RB_CreateSortKey(item, 15, input->shader->sort);
	RB_AddDrawItem(backEndData->currentPass, key, item);
}

static unsigned int RB_CalcShaderVertexAttribs( const shader_t *shader )
{
	unsigned int vertexAttribs = shader->vertexAttribs;

	if(glState.vertexAnimation)
	{
		vertexAttribs &= ~ATTR_COLOR;
		vertexAttribs |= ATTR_POSITION2;
		if (vertexAttribs & ATTR_NORMAL)
		{
			vertexAttribs |= ATTR_NORMAL2;
			vertexAttribs |= ATTR_TANGENT2;
		}
	}

	if (glState.skeletalAnimation)
	{
		vertexAttribs |= ATTR_BONE_WEIGHTS;
		vertexAttribs |= ATTR_BONE_INDEXES;
	}

	return vertexAttribs;
}

static shaderProgram_t *SelectShaderProgram( int stageIndex, shaderStage_t *stage, shaderProgram_t *glslShaderGroup, bool forceRefraction)
{
	uint32_t index;
	shaderProgram_t *result = nullptr;

	if (forceRefraction)
	{
		index = 0;
		if (tess.shader->numDeforms && !ShaderRequiresCPUDeforms(tess.shader))
		{
			index |= REFRACTION_USE_DEFORM_VERTEXES;
		}
		if (glState.vertexAnimation)
		{
			index |= REFRACTION_USE_VERTEX_ANIMATION;
		}
		else if (glState.skeletalAnimation)
		{
			index |= REFRACTION_USE_SKELETAL_ANIMATION;
		}
		result = &tr.refractionShader[index];
		backEnd.pc.c_lightallDraws++;
	}
	else if (backEnd.renderPass != MAIN_PASS)
	{
		index = 0;
		if (backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity)
		{
			if (tess.shader->numDeforms && !ShaderRequiresCPUDeforms(tess.shader))
			{
				index |= PREPASS_USE_DEFORM_VERTEXES;
			}
			if (glState.vertexAnimation)
			{
				index |= PREPASS_USE_VERTEX_ANIMATION;
			}
			else if (glState.skeletalAnimation)
			{
				index |= PREPASS_USE_SKELETAL_ANIMATION;
			}
		}

		if (tr.shadowCubeFbo && glState.currentFBO == tr.shadowCubeFbo)
			index |= PREPASS_USE_CUBEMAP_TRANSFORMS;

		if (backEnd.renderPass == PRE_PASS)
			index |= PREPASS_USE_G_BUFFERS;

		//FIX ME: UGLY, find better way of handling this
		if (stage->bundle[TB_NORMALMAP].image[0] != 0 && r_parallaxMapping->integer)
			if (stage->bundle[TB_NORMALMAP].image[0]->type == IMGTYPE_NORMALHEIGHT)
				index |= PREPASS_USE_PARALLAX;

		result = &tr.prepassShader[index];
		backEnd.pc.c_genericDraws++;
	}
	else if (stage->glslShaderGroup == tr.lightallShader)
	{
		index = stage->glslShaderIndex;

		if (r_lightmap->integer && (index & LIGHTDEF_USE_LIGHTMAP))
		{
			index = LIGHTDEF_USE_LIGHTMAP;
		}
		else
		{
			if (backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity)
			{
				if (glState.vertexAnimation)
				{
					index |= LIGHTDEF_USE_VERTEX_ANIMATION;
				}

				if (glState.skeletalAnimation)
				{
					index |= LIGHTDEF_USE_SKELETAL_ANIMATION;
				} 
			}

			if (r_sunlightMode->integer &&
					(backEnd.viewParms.flags & VPF_USESUNLIGHT) &&
					(index & LIGHTDEF_LIGHTTYPE_MASK))
			{
				index |= LIGHTDEF_USE_SHADOWMAP;
			}
		}

		result = &stage->glslShaderGroup[index];
		backEnd.pc.c_lightallDraws++;
	}
	else
	{
		result = GLSL_GetGenericShaderProgram(stageIndex);
		backEnd.pc.c_genericDraws++;
	}

	return result;
}

void RB_StageIteratorLiquid( void ) 
{
	deform_t deformType;
	genFunc_t deformGen;
	float deformParams[7];

	ComputeDeformValues(&deformType, &deformGen, deformParams);

	cullType_t cullType = CT_FRONT_SIDED;

	shaderCommands_t *input = &tess;
	if (!input->numVertexes || !input->numIndexes || (tr.renderCubeFbo && glState.currentFBO == tr.renderCubeFbo))
	{
		return;
	}
	//
	// log this call
	//
	if (r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va("--- RB_StageIteratorLiquid( %s ) ---\n", tess.shader->name));
	}

	//
	// update vertex buffer data
	// 
	uint32_t vertexAttribs = RB_CalcShaderVertexAttribs(input->shader);
	if (tess.useInternalVBO)
	{
		RB_DeformTessGeometry();
		RB_UpdateVBOs(vertexAttribs);
	}
	else
	{
		backEnd.pc.c_staticVboDraws++;
	}

	//
	// vertex arrays
	//
	VertexArraysProperties vertexArrays;
	if (tess.useInternalVBO)
	{
		CalculateVertexArraysProperties(vertexAttribs, &vertexArrays);
		for (int i = 0; i < vertexArrays.numVertexArrays; i++)
		{
			int attributeIndex = vertexArrays.enabledAttributes[i];
			vertexArrays.offsets[attributeIndex] += backEndData->currentFrame->dynamicVboCommitOffset;
		}
	}
	else
	{
		CalculateVertexArraysFromVBO(vertexAttribs, glState.currentVBO, &vertexArrays);
	}

	vertexAttribute_t attribs[ATTR_INDEX_MAX] = {};
	GL_VertexArraysToAttribs(attribs, ARRAY_LEN(attribs), &vertexArrays);

	UniformDataWriter uniformDataWriter;
	SamplerBindingsWriter samplerBindingsWriter;
	shaderProgram_t *liquidShader = SelectShaderProgram(0, NULL, NULL, true);
	uniformDataWriter.Start(liquidShader);
	uniformDataWriter.SetUniformMatrix4x4(UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);
	uniformDataWriter.SetUniformVec3(UNIFORM_VIEWORIGIN, backEnd.viewParms.ori.origin);
	uniformDataWriter.SetUniformVec3(UNIFORM_LOCALVIEWORIGIN, backEnd.ori.viewOrigin);

	matrix_t invModelMatrix;
	matrix_t transInvModelMatrix;
	Matrix16Inverse(backEnd.ori.modelMatrix, invModelMatrix);
	Matrix16Transpose(invModelMatrix, transInvModelMatrix);

	uniformDataWriter.SetUniformMatrix4x4(UNIFORM_MODELMATRIX, backEnd.ori.modelMatrix);
	uniformDataWriter.SetUniformMatrix4x4(UNIFORM_NORMALMATRIX, transInvModelMatrix);

	if (r_cubeMapping->integer)
	{
		vec4_t vec;
		cubemap_t *cubemap = &tr.cubemaps[input->cubemapIndex - 1];

		samplerBindingsWriter.AddStaticImage(cubemap->image, TB_CUBEMAP);

		VectorSubtract(cubemap->origin, backEnd.viewParms.ori.origin, vec);
		vec[3] = 1.0f;

		VectorScale4(vec, 1.0f / cubemap->parallaxRadius, vec);

		uniformDataWriter.SetUniformVec4(UNIFORM_CUBEMAPINFO, vec);
	}

	if (r_sunlightMode->integer)
	{
		samplerBindingsWriter.AddStaticImage(tr.screenShadowImage, TB_SHADOWMAP);
		uniformDataWriter.SetUniformVec3(UNIFORM_PRIMARYLIGHTAMBIENT, backEnd.refdef.sunAmbCol);
		uniformDataWriter.SetUniformVec3(UNIFORM_PRIMARYLIGHTCOLOR, backEnd.refdef.sunCol);
		uniformDataWriter.SetUniformVec4(UNIFORM_PRIMARYLIGHTORIGIN, backEnd.refdef.sunDir);
	}

	LiquidBlock *data = ojkAlloc<LiquidBlock>(*backEndData->perFrameMemory);
	*data = {};

	data->isLiquid = 1.0;
	data->height = tess.shader->liquid.height;
	data->choppy = tess.shader->liquid.choppy;
	data->speed = tess.shader->liquid.speed;
	data->freq = tess.shader->liquid.freq;
	data->depth = tess.shader->liquid.depth;
	data->time = tess.shaderTime;
	VectorCopy(tess.shader->liquid.water_color, data->water_color);
	VectorCopy(tess.shader->liquid.fog_color, data->fog_color);
	
	//ri.Printf(PRINT_ALL, "water_color should be: %f %f %f\n", tess.shader->liquid.water_color[0], tess.shader->liquid.water_color[1], tess.shader->liquid.water_color[2]);
	//ri.Printf(PRINT_ALL, "water_color is: %f %f %f\n", data2.water_color_r, data2.water_color_g, data2.water_color_b);

	DrawItem item = {};
	item.renderState.stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	item.renderState.cullType = cullType;
	item.renderState.depthRange = RB_GetDepthRange(backEnd.currentEntity, input->shader);
	item.program = liquidShader;
	item.ibo = input->externalIBO ? input->externalIBO : backEndData->currentFrame->dynamicIbo;

	item.numAttributes = vertexArrays.numVertexArrays;
	item.attributes = ojkAllocArray<vertexAttribute_t>(
		*backEndData->perFrameMemory, vertexArrays.numVertexArrays);
	memcpy(item.attributes, attribs, sizeof(*item.attributes)*vertexArrays.numVertexArrays);

	item.numUniformBlockBindings = 1;
	item.uniformBlockBindings = ojkAllocArray<UniformBlockBinding>(*backEndData->perFrameMemory, item.numUniformBlockBindings);
	item.uniformBlockBindings[0].data = data;
	item.uniformBlockBindings[0].block = UNIFORM_BLOCK_LIQUID;

	item.uniformData = uniformDataWriter.Finish(*backEndData->perFrameMemory);
	// FIXME: This is a bit ugly with the casting
	item.samplerBindings = samplerBindingsWriter.Finish(
		*backEndData->perFrameMemory, (int *)&item.numSamplerBindings);

	RB_FillDrawCommand(item.draw, GL_TRIANGLES, 1, input);

	int sortStage = backEnd.renderPass != MAIN_PASS ? input->currentDistanceBucket : 1;
	uint32_t key = RB_CreateSortKey(item, sortStage, input->shader->sort);
	RB_AddDrawItem(backEndData->currentPass, key, item);
}

static void RB_IterateStagesGeneric( shaderCommands_t *input, const VertexArraysProperties *vertexArrays )
{
	deform_t deformType;
	genFunc_t deformGen;
	float deformParams[7];
	Pass *renderPass = backEndData->currentPass;

	ComputeDeformValues(&deformType, &deformGen, deformParams);

	bool renderToCubemap = tr.renderCubeFbo && glState.currentFBO == tr.renderCubeFbo;
	bool renderSolid = backEnd.renderPass == MAIN_PASS && input->shader->sort == SS_OPAQUE;
	
	cullType_t cullType = RB_GetCullType(&backEnd.viewParms, backEnd.currentEntity, input->shader->cullType);

	// HACK: Not sure why this is needed
	if (renderToCubemap)
		cullType = CT_TWO_SIDED;

	vertexAttribute_t attribs[ATTR_INDEX_MAX] = {};
	GL_VertexArraysToAttribs(attribs, ARRAY_LEN(attribs), vertexArrays);

	UniformDataWriter uniformDataWriter;
	SamplerBindingsWriter samplerBindingsWriter;

	for ( int stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = input->xstages[stage];
		shaderProgram_t *sp;
		vec4_t texMatrix;
		vec4_t texOffTurb;
		int stateBits;
		colorGen_t forceRGBGen = CGEN_BAD;
		alphaGen_t forceAlphaGen = AGEN_IDENTITY;
		int index = 0;
		bool forceRefraction = false;
		bool useAlphaTestGE192 = false;
		bool disintegrate = false;

		if ( !pStage )
		{
			break;
		}

		if ( pStage->ss )
		{
			continue;
		}

		stateBits = pStage->stateBits;

		if (backEnd.currentEntity)
		{
			assert(backEnd.currentEntity->e.renderfx >= 0);

			if ( backEnd.currentEntity->e.renderfx & ( RF_DISINTEGRATE1 | RF_DISINTEGRATE2 ))
			{
				// we want to be able to rip a hole in the thing being
				// disintegrated, and by doing the depth-testing it avoids some
				// kinds of artefacts, but will probably introduce others?
				stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK_TRUE;
				useAlphaTestGE192 = true;
				disintegrate = true;
			}

			if (backEnd.currentEntity->e.renderfx & RF_ALPHA_FADE)
			{
				// eg ForceSpeed Doppelgängers
				if (backEnd.currentEntity->e.shaderRGBA[3] < 255)
				{
					stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
					forceAlphaGen = AGEN_ENTITY;
				}
			}

			if ( backEnd.currentEntity->e.renderfx & RF_RGB_TINT )
			{//want to use RGBGen from ent
				forceRGBGen = CGEN_ENTITY;
			}

			// pretty bad way of removing sabers from main pass
			// we do this for ssr, because sabers should be analytical lights
			if (backEnd.currentEntity->e.reType == RT_SABER_GLOW || 
				backEnd.currentEntity->e.reType == RT_LINE || 
				!strcmp(input->shader->name, "gfx/effects/sabers/swordTrail") ||
				!strcmp(input->shader->name, "gfx/effects/sabers/saberBlur") || 
				!strcmp(input->shader->name, "gfx/effects/sabers/blackSaberBlur") || 
				!strcmp(input->shader->name, "SFX_Sabers/saber_trail") || 
				!strcmp(input->shader->name, "SFX_Sabers/black_trail"))
				renderPass = backEndData->currentPostPass;

			if ((input->shader == tr.distortionShader) || backEnd.currentEntity->e.renderfx & RF_DISTORTION)
			{
				forceRefraction = true;
				renderPass = backEndData->currentPostPass;
			}
			/*if ( backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA )
			{
				stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
				if ( backEnd.currentEntity->e.renderfx & RF_ALPHA_DEPTH )
				{
					// depth write, so faces through the model will be stomped
					// over by nearer ones. this works because we draw
					// RF_FORCE_ENT_ALPHA stuff after everything else, including
					// standard alpha surfs.
					stateBits |= GLS_DEPTHMASK_TRUE;
				}
			}*/
		}

		sp = SelectShaderProgram(stage, pStage, pStage->glslShaderGroup, forceRefraction);
		assert(sp);

		uniformDataWriter.Start(sp);

		uniformDataWriter.SetUniformMatrix4x4( UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);
		uniformDataWriter.SetUniformVec3(UNIFORM_VIEWORIGIN, backEnd.viewParms.ori.origin);
		uniformDataWriter.SetUniformVec3(UNIFORM_LOCALVIEWORIGIN, backEnd.ori.viewOrigin);

		if (backEnd.renderPass == PRE_PASS)
			uniformDataWriter.SetUniformMatrix4x4(UNIFORM_PREVVIEWPROJECTIONMATRIX, tr.preViewProjectionMatrix);

		if (glState.skeletalAnimation)
		{
			uniformDataWriter.SetUniformMatrix4x3(UNIFORM_BONE_MATRICES, &glState.boneMatrices[0][0], glState.numBones);
		}

		uniformDataWriter.SetUniformFloat(UNIFORM_VERTEXLERP, glState.vertexAttribsInterpolation);
		
		uniformDataWriter.SetUniformInt(UNIFORM_DEFORMTYPE, deformType);
		if (deformType != DEFORM_NONE)
		{
			uniformDataWriter.SetUniformInt(UNIFORM_DEFORMFUNC, deformGen);
			uniformDataWriter.SetUniformFloat(UNIFORM_DEFORMPARAMS, deformParams, 7);
			uniformDataWriter.SetUniformFloat(UNIFORM_TIME, tess.shaderTime);
		}

		if ( input->fogNum ) {
			const fog_t *fog = tr.world->fogs + input->fogNum;

			uniformDataWriter.SetUniformVec4(UNIFORM_COLOR, fog->color);
			uniformDataWriter.SetUniformVec4(UNIFORM_FOGPLANE, fog->surface);
			uniformDataWriter.SetUniformInt(UNIFORM_FOGHASPLANE, fog->hasSurface);
			uniformDataWriter.SetUniformFloat(UNIFORM_FOGDEPTHTOOPAQUE, fog->parms.depthForOpaque);
			uniformDataWriter.SetUniformVec3(UNIFORM_VIEWORIGIN, backEnd.refdef.vieworg);

			vec4_t fogColorMask;
			ComputeFogColorMask(pStage, fogColorMask);
			uniformDataWriter.SetUniformVec4(UNIFORM_FOGCOLORMASK, fogColorMask);
		}

		float volumetricBaseValue = -1.0f;
		if ( backEnd.currentEntity->e.renderfx & RF_VOLUMETRIC )
		{
			volumetricBaseValue = backEnd.currentEntity->e.shaderRGBA[0] / 255.0f;
			uniformDataWriter.SetUniformVec3(UNIFORM_VIEWFORWARD, backEnd.refdef.viewaxis[0]);
		}
		else
		{
			vec4_t baseColor;
			vec4_t vertColor;
			
			ComputeShaderColors(pStage, baseColor, vertColor, stateBits, &forceRGBGen, &forceAlphaGen);

			if ((backEnd.refdef.colorScale != 1.0f) && !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
			{
				// use VectorScale to only scale first three values, not alpha
				VectorScale(baseColor, backEnd.refdef.colorScale, baseColor);
				VectorScale(vertColor, backEnd.refdef.colorScale, vertColor);
			}

			if (backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA)
			{
				vertColor[3] = backEnd.currentEntity->e.shaderRGBA[3] / 255.0f;
			}

			uniformDataWriter.SetUniformVec4(UNIFORM_BASECOLOR, baseColor);
			uniformDataWriter.SetUniformVec4(UNIFORM_VERTCOLOR, vertColor);
		}

		uniformDataWriter.SetUniformFloat(UNIFORM_FX_VOLUMETRIC_BASE, volumetricBaseValue);

		if (pStage->rgbGen == CGEN_LIGHTING_DIFFUSE ||
			pStage->rgbGen == CGEN_LIGHTING_DIFFUSE_ENTITY)
		{
			vec4_t vec;

			VectorScale(backEnd.currentEntity->ambientLight, 1.0f / 255.0f, vec);
			uniformDataWriter.SetUniformVec3(UNIFORM_AMBIENTLIGHT, vec);

			VectorScale(backEnd.currentEntity->directedLight, 1.0f / 255.0f, vec);
			uniformDataWriter.SetUniformVec3(UNIFORM_DIRECTEDLIGHT, vec);

			VectorCopy(backEnd.currentEntity->lightDir, vec);
			vec[3] = 0.0f;
			uniformDataWriter.SetUniformVec4(UNIFORM_LIGHTORIGIN, vec);
			uniformDataWriter.SetUniformVec3(UNIFORM_MODELLIGHTDIR, backEnd.currentEntity->modelLightDir);

			uniformDataWriter.SetUniformFloat(UNIFORM_LIGHTRADIUS, 0.0f);
		}

		if (pStage->alphaGen == AGEN_PORTAL)
		{
			uniformDataWriter.SetUniformFloat(UNIFORM_PORTALRANGE, tess.shader->portalRange);
		}
		if (disintegrate)
		{
			vec4_t disintegrationInfo;
			disintegrationInfo[0] = backEnd.currentEntity->e.oldorigin[0];
			disintegrationInfo[1] = backEnd.currentEntity->e.oldorigin[1];
			disintegrationInfo[2] = backEnd.currentEntity->e.oldorigin[2];
			disintegrationInfo[3] = (backEnd.refdef.time - backEnd.currentEntity->e.endTime) * 0.045f;
			disintegrationInfo[3] *= disintegrationInfo[3];
			if (backEnd.currentEntity->e.renderfx & RF_DISINTEGRATE1)
				forceAlphaGen = AGEN_DISINTEGRATE1;
			else
				forceAlphaGen = AGEN_DISINTEGRATE2;
			uniformDataWriter.SetUniformVec4(UNIFORM_DISINTEGRATION, disintegrationInfo);
		}

		uniformDataWriter.SetUniformInt(UNIFORM_COLORGEN, forceRGBGen);
		uniformDataWriter.SetUniformInt(UNIFORM_ALPHAGEN, forceAlphaGen);

		ComputeTexMods( pStage, TB_DIFFUSEMAP, texMatrix, texOffTurb );
		uniformDataWriter.SetUniformVec4(UNIFORM_DIFFUSETEXMATRIX, texMatrix);
		uniformDataWriter.SetUniformVec4(UNIFORM_DIFFUSETEXOFFTURB, texOffTurb);

		uniformDataWriter.SetUniformInt(UNIFORM_TCGEN0, pStage->bundle[0].tcGen);
		uniformDataWriter.SetUniformInt(UNIFORM_TCGEN1, pStage->bundle[1].tcGen);
		if (pStage->bundle[0].tcGen == TCGEN_VECTOR)
		{
			uniformDataWriter.SetUniformVec3(UNIFORM_TCGEN0VECTOR0, pStage->bundle[0].tcGenVectors[0]);
			uniformDataWriter.SetUniformVec3(UNIFORM_TCGEN0VECTOR1, pStage->bundle[0].tcGenVectors[1]);
		}

		matrix_t invModelMatrix;
		matrix_t transInvModelMatrix;
		Matrix16Inverse(backEnd.ori.modelMatrix, invModelMatrix);
		Matrix16Transpose(invModelMatrix, transInvModelMatrix);

		uniformDataWriter.SetUniformMatrix4x4(UNIFORM_MODELMATRIX, backEnd.ori.modelMatrix);
		uniformDataWriter.SetUniformMatrix4x4(UNIFORM_NORMALMATRIX, transInvModelMatrix);

		uniformDataWriter.SetUniformVec4(UNIFORM_NORMALSCALE, pStage->normalScale);
		{
			vec4_t realTimeSpecGloss;
			realTimeSpecGloss[0] = pStage->specularScale[0];
			realTimeSpecGloss[1] = pStage->specularScale[1];
			realTimeSpecGloss[2] = pStage->specularScale[2];
			realTimeSpecGloss[3] = pStage->specularScale[3] * r_glossScale->value;
			uniformDataWriter.SetUniformVec4(UNIFORM_SPECULARSCALE, realTimeSpecGloss);
		}

		int alphaTestFunction = useAlphaTestGE192 ? ATEST_CMP_GE : pStage->alphaTestCmp;
		uniformDataWriter.SetUniformInt(UNIFORM_ALPHA_TEST_FUNCTION, alphaTestFunction);
		float alphaTestValue = useAlphaTestGE192 ? 0.75f : pStage->alphaTestValue;
		uniformDataWriter.SetUniformFloat(UNIFORM_ALPHA_TEST_VALUE, alphaTestValue);

		//
		// do multitexture
		//
		bool enableCubeMaps =
			(r_cubeMapping->integer && !(tr.viewParms.flags & VPF_NOCUBEMAPS) && input->cubemapIndex);
		bool enableShpericalHarmonics = 
			(r_cubeMapping->integer && !(tr.viewParms.flags & VPF_NOCUBEMAPS) && input->cubemapIndex && tr.numfinishedSphericalHarmonics == tr.numSphericalHarmonics);

		if (backEnd.renderPass != MAIN_PASS)
		{
			if (pStage->alphaTestCmp == ATEST_CMP_NONE)
				samplerBindingsWriter.AddStaticImage(tr.whiteImage, 0);
			else if ( pStage->bundle[TB_COLORMAP].image[0] != 0 )
				samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[TB_COLORMAP], TB_COLORMAP);

			if (backEnd.renderPass == PRE_PASS) 
			{
				vec4_t enableTextures = {};
				if (pStage->bundle[TB_NORMALMAP].image[0] != 0) {
					samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[TB_NORMALMAP], TB_NORMALMAP);
					enableTextures[0] = 1.0f;
				}
				if (pStage->bundle[TB_SPECULARMAP].image[0] != 0) {
					samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[TB_SPECULARMAP], TB_SPECULARMAP);
					enableTextures[2] = 1.0f;
				}
				uniformDataWriter.SetUniformVec4(UNIFORM_ENABLETEXTURES, enableTextures);
			}
		}
		else if ( pStage->glslShaderGroup == tr.lightallShader )
		{
			int i;
			vec4_t enableTextures = {};

			if (r_sunlightMode->integer &&
					(backEnd.viewParms.flags & VPF_USESUNLIGHT) &&
					(pStage->glslShaderIndex & LIGHTDEF_LIGHTTYPE_MASK))
			{
				samplerBindingsWriter.AddStaticImage(tr.screenShadowImage, TB_SHADOWMAP);
				uniformDataWriter.SetUniformVec3(UNIFORM_PRIMARYLIGHTAMBIENT, backEnd.refdef.sunAmbCol);
				uniformDataWriter.SetUniformVec3(UNIFORM_PRIMARYLIGHTCOLOR,   backEnd.refdef.sunCol);
				uniformDataWriter.SetUniformVec4(UNIFORM_PRIMARYLIGHTORIGIN,  backEnd.refdef.sunDir);
			}

			if ((r_lightmap->integer == 1 || r_lightmap->integer == 2) &&
					pStage->bundle[TB_LIGHTMAP].image[0])
			{
				for (i = 0; i < NUM_TEXTURE_BUNDLES; i++)
				{
					if (i == TB_LIGHTMAP)
						samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[TB_LIGHTMAP], i);
					else
						samplerBindingsWriter.AddStaticImage(tr.whiteImage, i);
				}
			}
			else if (r_lightmap->integer == 3 && pStage->bundle[TB_DELUXEMAP].image[0])
			{
				for (i = 0; i < NUM_TEXTURE_BUNDLES; i++)
				{
					if (i == TB_LIGHTMAP)
						samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[TB_DELUXEMAP], i);
					else
						samplerBindingsWriter.AddStaticImage(tr.whiteImage, i);
				}
			}
			else
			{
				qboolean light = (qboolean)((pStage->glslShaderIndex & LIGHTDEF_LIGHTTYPE_MASK) != 0);
				qboolean allowVertexLighting = (qboolean)!(r_normalMapping->integer || r_specularMapping->integer);

				if (pStage->bundle[TB_DIFFUSEMAP].image[0])
					samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[TB_DIFFUSEMAP], TB_DIFFUSEMAP);

				if (pStage->bundle[TB_LIGHTMAP].image[0])
					samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[TB_LIGHTMAP], TB_LIGHTMAP);

				// bind textures that are sampled and used in the glsl shader, and
				// bind whiteImage to textures that are sampled but zeroed in the glsl shader
				//
				// alternatives:
				//  - use the last bound texture
				//     -> costs more to sample a higher res texture then throw out the result
				//  - disable texture sampling in glsl shader with #ifdefs, as before
				//     -> increases the number of shaders that must be compiled
				//
				if (light && !allowVertexLighting)
				{
					vec2_t lightScales;
					lightScales[0] = r_ambientScale->value;
					lightScales[1] = r_directedScale->value;

					if (tr.world)
					{
						uniformDataWriter.SetUniformVec3(UNIFORM_LIGHTGRIDORIGIN, tr.world->lightGridOrigin);
						uniformDataWriter.SetUniformVec3(UNIFORM_LIGHTGRIDCELLINVERSESIZE, tr.world->lightGridInverseSize);
						uniformDataWriter.SetUniformVec3(UNIFORM_LIGHTGRIDLIGHTSCALE, lightScales);
					}

					if (pStage->bundle[TB_NORMALMAP].image[0])
					{
						samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[TB_NORMALMAP], TB_NORMALMAP);
					}
					else if (r_normalMapping->integer)
					{
						samplerBindingsWriter.AddStaticImage(tr.whiteImage, TB_NORMALMAP);
					}

					if (pStage->bundle[TB_DELUXEMAP].image[0])
					{
						samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[TB_DELUXEMAP], TB_DELUXEMAP);
						enableTextures[1] = 1.0f;
					}
					else if (r_deluxeMapping->integer)
					{
						samplerBindingsWriter.AddStaticImage(tr.whiteImage, TB_DELUXEMAP);
					}

					if (pStage->bundle[TB_SPECULARMAP].image[0])
					{
						samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[TB_SPECULARMAP], TB_SPECULARMAP);
						enableTextures[2] = 1.0f;
					}
					else if (r_specularMapping->integer)
					{
						samplerBindingsWriter.AddStaticImage(tr.whiteImage, TB_SPECULARMAP);
					}

					if ( renderSolid ) {
						samplerBindingsWriter.AddStaticImage(tr.diffuseLightingImage, TB_DIFFUSELIGHTBUFFER);
						samplerBindingsWriter.AddStaticImage(tr.specularLightingImage, TB_SPECLIGHTBUFFER);
					}
					else {
						samplerBindingsWriter.AddStaticImage( NULL , TB_DIFFUSELIGHTBUFFER);
						samplerBindingsWriter.AddStaticImage( NULL , TB_SPECLIGHTBUFFER);
					}
				}

				if (enableShpericalHarmonics)
					enableTextures[0] = 1.0f;

				if ( enableCubeMaps )
				{
					enableTextures[3] =  1.0f;
				}
			}

			uniformDataWriter.SetUniformVec4(UNIFORM_ENABLETEXTURES, enableTextures);
		}
		else if ( pStage->bundle[1].image[0] != 0 )
		{
			samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[0], 0);
			samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[1], 1);
		}
		else 
		{
			//
			// set state
			//
			samplerBindingsWriter.AddAnimatedImage(&pStage->bundle[0], 0);
		}

		//
		// testing cube map
		//
		if ( enableCubeMaps )
		{
			vec4_t vec;
			cubemap_t *cubemap = &tr.cubemaps[input->cubemapIndex - 1];

			samplerBindingsWriter.AddStaticImage(cubemap->image, TB_CUBEMAP);

			VectorSubtract(cubemap->origin, backEnd.viewParms.ori.origin, vec);
			vec[3] = 1.0f;

			VectorScale4(vec, 1.0f / cubemap->parallaxRadius, vec);

			uniformDataWriter.SetUniformVec4(UNIFORM_CUBEMAPINFO, vec);

			if (tr.numfinishedSphericalHarmonics == tr.numSphericalHarmonics && tr.numSphericalHarmonics > 0) {
				//TODO: speed up this process, maybe ambient pre pass
				int index = R_SHForPoint(backEnd.currentEntity->e.lightingOrigin);
				sphericalHarmonic_t *sh = &tr.sphericalHarmonicsCoefficients[
					index
				];
				float coefficients[27];
				for (int i = 0; i < 9; i++) {
					coefficients[i * 3 + 0] = sh->coefficents[i][0];
					coefficients[i * 3 + 1] = sh->coefficents[i][1];
					coefficients[i * 3 + 2] = sh->coefficents[i][2];
				}
				uniformDataWriter.SetUniformVec3(UNIFORM_SPHERICAL_HARMONIC, coefficients, 9);
			}
		}

		LiquidBlock *data;
		if (forceRefraction)
		{
			samplerBindingsWriter.AddStaticImage(tr.prevRenderImage, TB_COLORMAP);
			samplerBindingsWriter.AddStaticImage(tr.renderDepthImage, TB_COLORMAP2);

			stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			cullType = CT_TWO_SIDED;
			vec4_t viewInfo;
			float alpha = 0;
			if (backEnd.currentEntity->e.shaderRGBA[3] > 10)
				alpha = (backEnd.currentEntity->e.shaderRGBA[3]) / 255.0f;
			float zmax = backEnd.viewParms.zFar;
			float zmin = r_znear->value;
			float x = tr.prevRenderImage->width;
			float y = tr.prevRenderImage->height;
			VectorSet4(viewInfo, zmax / zmin, zmax, x, alpha);

			//we need the normal in projection space not in worldspace
			matrix_t invModelViewProjectionMatrix;
			matrix_t transInvModelViewProjectionMatrix;
			Matrix16Inverse(glState.modelviewProjection, invModelViewProjectionMatrix);
			Matrix16Transpose(invModelViewProjectionMatrix, transInvModelViewProjectionMatrix);

			uniformDataWriter.SetUniformVec4(UNIFORM_VIEWINFO, viewInfo);
			uniformDataWriter.SetUniformMatrix4x4(UNIFORM_NORMALMATRIX, transInvModelViewProjectionMatrix);

			data = ojkAlloc<LiquidBlock>(*backEndData->perFrameMemory);
			*data = {};

			data->isLiquid = 0.0;
			data->height = tess.shader->liquid.height;
			data->choppy = tess.shader->liquid.choppy;
			data->speed = tess.shader->liquid.speed;
			data->freq = tess.shader->liquid.freq;
			data->depth = tess.shader->liquid.depth;
			data->time = tess.shaderTime;
		}

		CaptureDrawData(input, pStage, index, stage);

		DrawItem item = {};
		item.renderState.stateBits = stateBits;
		item.renderState.cullType = cullType;
		item.renderState.depthRange = RB_GetDepthRange(backEnd.currentEntity, input->shader);
		item.program = sp;
		item.ibo = input->externalIBO ? input->externalIBO : backEndData->currentFrame->dynamicIbo;

		item.numAttributes = vertexArrays->numVertexArrays;
		item.attributes = ojkAllocArray<vertexAttribute_t>(
			*backEndData->perFrameMemory, vertexArrays->numVertexArrays);
		memcpy(item.attributes, attribs, sizeof(*item.attributes)*vertexArrays->numVertexArrays);

		if (forceRefraction)
		{
			item.numUniformBlockBindings = 1;
			item.uniformBlockBindings = ojkAllocArray<UniformBlockBinding>(*backEndData->perFrameMemory, item.numUniformBlockBindings);
			item.uniformBlockBindings[0].data = data;
			item.uniformBlockBindings[0].block = UNIFORM_BLOCK_LIQUID;
		}

		item.uniformData = uniformDataWriter.Finish(*backEndData->perFrameMemory);
		// FIXME: This is a bit ugly with the casting
		item.samplerBindings = samplerBindingsWriter.Finish(
			*backEndData->perFrameMemory, (int *)&item.numSamplerBindings);

		RB_FillDrawCommand(item.draw, GL_TRIANGLES, 1, input);

		int sortStage = backEnd.renderPass != MAIN_PASS ? input->currentDistanceBucket : stage;
		uint32_t key = RB_CreateSortKey(item, sortStage, input->shader->sort);
		RB_AddDrawItem(renderPass, key, item);

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap ) )
		{
			break;
		}

		if (backEnd.renderPass != MAIN_PASS)
			break;
	}
}


static void RB_RenderShadowmap( shaderCommands_t *input, const VertexArraysProperties *vertexArrays )
{
	deform_t deformType;
	genFunc_t deformGen;
	float deformParams[7];

	ComputeDeformValues(&deformType, &deformGen, deformParams);

	cullType_t cullType = RB_GetCullType(&backEnd.viewParms, backEnd.currentEntity, input->shader->cullType);

	vertexAttribute_t attribs[ATTR_INDEX_MAX] = {};
	GL_VertexArraysToAttribs(attribs, ARRAY_LEN(attribs), vertexArrays);

	UniformDataWriter uniformDataWriter;

	shaderProgram_t *sp = &tr.shadowmapShader;
	uniformDataWriter.Start(sp);
	uniformDataWriter.SetUniformMatrix4x4(UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);
	uniformDataWriter.SetUniformFloat(UNIFORM_VERTEXLERP, glState.vertexAttribsInterpolation);
	uniformDataWriter.SetUniformInt(UNIFORM_DEFORMTYPE, deformType);
	uniformDataWriter.SetUniformInt(UNIFORM_DEFORMFUNC, deformGen);
	uniformDataWriter.SetUniformFloat(UNIFORM_DEFORMPARAMS, deformParams, 7);
	uniformDataWriter.SetUniformFloat(UNIFORM_TIME, tess.shaderTime);

	DrawItem item = {};
	//item.renderState.stateBits = stateBits;
	item.renderState.cullType = cullType;
	item.renderState.depthRange = RB_GetDepthRange(backEnd.currentEntity, input->shader);
	item.program = sp;
	item.ibo = input->externalIBO ? input->externalIBO : backEndData->currentFrame->dynamicIbo;

	item.numAttributes = vertexArrays->numVertexArrays;
	item.attributes = ojkAllocArray<vertexAttribute_t>(
		*backEndData->perFrameMemory, vertexArrays->numVertexArrays);
	memcpy(item.attributes, attribs, sizeof(*item.attributes)*vertexArrays->numVertexArrays);

	item.uniformData = uniformDataWriter.Finish(*backEndData->perFrameMemory);

	RB_FillDrawCommand(item.draw, GL_TRIANGLES, 1, input);

	int sortStage = backEnd.renderPass != MAIN_PASS ? input->currentDistanceBucket : 0;
	uint32_t key = RB_CreateSortKey(item, sortStage, input->shader->sort);
	RB_AddDrawItem(backEndData->currentPass, key, item);
}

/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input = &tess;
	if (!input->numVertexes || !input->numIndexes)
	{
		return;
	}

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	//
	// update vertex buffer data
	// 
	uint32_t vertexAttribs = RB_CalcShaderVertexAttribs( input->shader );
	if ( input->useInternalVBO )
	{
		RB_DeformTessGeometry();
		RB_UpdateVBOs(vertexAttribs);
	}
	else
	{
		backEnd.pc.c_staticVboDraws++;
	}

	//
	// vertex arrays
	//
	VertexArraysProperties vertexArrays;
	if ( input->useInternalVBO )
	{
		CalculateVertexArraysProperties(vertexAttribs, &vertexArrays);
		for ( int i = 0; i < vertexArrays.numVertexArrays; i++ )
		{
			int attributeIndex = vertexArrays.enabledAttributes[i];
			vertexArrays.offsets[attributeIndex] += backEndData->currentFrame->dynamicVboCommitOffset;
		}
	}
	else
	{
		CalculateVertexArraysFromVBO(vertexAttribs, glState.currentVBO, &vertexArrays);
	}

	if (input->shader == tr.shadowShader && backEnd.renderPass == MAIN_PASS && r_shadows->integer == 2) {
		RB_ShadowTessEnd( input, &vertexArrays );
	}
	else if (backEnd.renderPass != MAIN_PASS)
	{
		RB_IterateStagesGeneric( input, &vertexArrays );
	}
	else if (backEnd.viewParms.flags & VPF_SHADOWMAP)
	{
		if ( input->shader->sort == SS_OPAQUE )
		{
			RB_RenderShadowmap(input, &vertexArrays);
		}
	}
	else
	{
		RB_IterateStagesGeneric( input, &vertexArrays );

		//
		// pshadows!
		//
		if (r_shadows->integer == 4 &&
				tess.pshadowBits &&
				tess.shader->sort <= SS_OPAQUE &&
				!(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY)))
		{
			ProjectPshadowVBOGLSL(input, &vertexArrays);
		}

		// 
		// now do any dynamic lighting needed
		//
		if ( r_debugVisuals->integer == 0 &&
			 tess.dlightBits &&
			 r_dlightMode->integer &&
			 input->shader->sort != SS_OPAQUE)
		{
			ForwardDlight(input, &vertexArrays);
		}

		//
		// now do fog
		//
		const fog_t *fog = nullptr;
		if ( tr.world )
		{
			if ( tr.world->globalFog )
				fog = tr.world->globalFog;
			else if ( tess.fogNum )
				fog = tr.world->fogs + tess.fogNum;
		}

		if ( fog && tess.shader->fogPass ) {
			RB_FogPass( &tess, fog, &vertexArrays );
		}
	}

	RB_CommitInternalBufferData();
}

void RB_BinTriangleCounts( void )
{
	int numTriangles = tess.numIndexes / 3;
	if ( numTriangles < 20 )
		backEnd.pc.c_triangleCountBins[TRI_BIN_0_19]++;
	else if ( numTriangles < 50 )
		backEnd.pc.c_triangleCountBins[TRI_BIN_20_49]++;
	else if ( numTriangles < 100 )
		backEnd.pc.c_triangleCountBins[TRI_BIN_50_99]++;
	else if ( numTriangles < 300 )
		backEnd.pc.c_triangleCountBins[TRI_BIN_100_299]++;
	else if ( numTriangles < 600 )
		backEnd.pc.c_triangleCountBins[TRI_BIN_300_599]++;
	else if ( numTriangles < 1000 )
		backEnd.pc.c_triangleCountBins[TRI_BIN_1000_1499]++;
	else if ( numTriangles < 1500 )
		backEnd.pc.c_triangleCountBins[TRI_BIN_1500_1999]++;
	else if ( numTriangles < 2000 )
		backEnd.pc.c_triangleCountBins[TRI_BIN_2000_2999]++;
	else
		backEnd.pc.c_triangleCountBins[TRI_BIN_3000_PLUS]++;
}

/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0 || input->numVertexes == 0) {
		return;
	}

	if (input->indexes[SHADER_MAX_INDEXES-1] != 0) {
		ri.Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit");
	}	
	if (input->xyz[SHADER_MAX_VERTEXES-1][0] != 0) {
		ri.Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit");
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	if (skyboxportal)
	{
		// world
		if (!(backEnd.refdef.rdflags & RDF_SKYBOXPORTAL))
		{
			if (tess.currentStageIteratorFunc == RB_StageIteratorSky)
			{	// don't process these tris at all
				return;
			}
		}
		// portal sky
		else
		{
			if (!drawskyboxportal)
			{
				if (!(tess.currentStageIteratorFunc == RB_StageIteratorSky))
				{	// /only/ process sky tris
					return;
				}
			}
		}
	}

	//
	// update performance counters
	//
	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	RB_BinTriangleCounts();

	//
	// call off to shader specific tess end function
	//
	tess.currentStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( r_showtris->integer ) {
		DrawTris (input);
	}
	if ( r_shownormals->integer ) {
		DrawNormals (input);
	}
	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.firstIndex = 0;
	tess.multiDrawPrimitives = 0;
	tess.externalIBO = nullptr;

	GLimp_LogComment( "----------\n" );
}
