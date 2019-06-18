/*
===========================================================================
Copyright (C) 2019 Maximilian Krüger.

This file is part of OpenDF2 source code.

OpenDF2 source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenDF2 source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenDF2 source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_tbo.cpp
#include "tr_local.h"
#include "glext.h"

struct matricesBlock
{
	matrix_t modelMatrix;
	matrix_t normalMatrix;
};

struct shaderBlock
{
	uint16_t lightmode;
	uint16_t numStages;
};

struct stageBlock
{
	uint16_t index;
	uint16_t numTextures;
	uint16_t blend;
	uint16_t alphaTest;
};

struct textureBlock
{
	uint16_t page;
	uint16_t index;
};

struct materialBlock
{
	shaderBlock		shader;
	stageBlock		*stage;
	textureBlock	*texture;
};

/*
** GL_BindTBO
*/
void GL_BindTBO(textureBuffer_t *tbo)
{
	if (tbo == NULL)
		return;

	int texnum = tbo->texnum;
	int tmu = tbo->slot;

	if (glState.currenttextures[tmu] != texnum) {
		GL_SelectTexture(tmu);
		glState.currenttextures[tmu] = texnum;
		qglBindTexture(GL_TEXTURE_BUFFER, texnum);
	}
}

/*
** GL_BindTBO
*/
void GL_UnbindTBO(textureBuffer_t *tbo)
{
	if (tbo == NULL)
		return;

	int tmu = tbo->slot;

	if (glState.currenttextures[tmu] != 0) {
		GL_SelectTexture(tmu);
		glState.currenttextures[tmu] = 0;
		qglBindTexture(GL_TEXTURE_BUFFER, 0);
	}
}

struct tboBlockInfo_t
{
	int slot;
	const char *name;
	int glFormat;
	int usage;
	int size;
};

const int MatricesMaxSize =			sizeof(matricesBlock) * MAX_REFENTITIES/4;
const int MaterialsMaxSize =		sizeof(shaderBlock) * MAX_SHADERS *
									sizeof(stageBlock) * 4 + //just assume we have an average of 4 stages max else it would be MAX_SHADER_STAGES
									sizeof(textureBlock) * MAX_DRAWIMAGES;
//FIX ME: build a new representation of all analytical light types and cubemaps/envprobes
const int LightComponentsMaxSize =	sizeof(dlight_t) * MAX_DLIGHTS;

const tboBlockInfo_t tboBlocksInfo[TBO_COUNT] = {
	{ TB_TBO_MATRICES, "TBO_Matrices", GL_RGBA32F, GL_DYNAMIC_DRAW, MatricesMaxSize },
	{ TB_TBO_MATERIALS, "TBO_Materials", GL_RG16UI, GL_DYNAMIC_DRAW, MaterialsMaxSize },
	{ TB_TBO_LIGHTS, "TBO_LightComponents", GL_RGBA16F, GL_DYNAMIC_DRAW, LightComponentsMaxSize }
};

textureBuffer_t *CreateTBO(const char *name, int *data, int size, int slot, int format, int usage)
{
	if (tr.numTbos >= MAX_TBOS)
		return NULL;

	textureBuffer_t *textureBuffer = (textureBuffer_t *)R_Hunk_Alloc(sizeof(textureBuffer_t), qtrue);;

	GLuint tbo;
	qglGenBuffers(1, &tbo);
	qglBindBuffer(GL_TEXTURE_BUFFER, tbo);

	qglBufferData(GL_TEXTURE_BUFFER, size, data, usage);

	GLuint buffer_texture;
	qglGenTextures(1, &buffer_texture);
	qglBindTexture(GL_TEXTURE_BUFFER, buffer_texture);

	qglTexBuffer(GL_TEXTURE_BUFFER, format, tbo);

	GL_CheckErrors();

	qglBindTexture(GL_TEXTURE_BUFFER, 0);

	textureBuffer->tbonum = tbo;
	textureBuffer->texnum = buffer_texture;
	textureBuffer->size = size;
	textureBuffer->slot = slot;
	textureBuffer->numItems = 0;

	return textureBuffer;
}

textureBuffer_t *CreateTBO(tboBlockInfo_t tboInfo)
{
	return CreateTBO(tboInfo.name, NULL, tboInfo.size, tboInfo.slot, tboInfo.glFormat, tboInfo.usage);
}

void DeleteTBO(textureBuffer_t *tbo)
{
	qglBindBuffer(1, tbo->tbonum);
	qglDeleteBuffers(1, &tbo->tbonum);
	qglDeleteTextures(1, &tbo->texnum);
}

void R_CreateBuildinTBOs(void)
{
	for (int i = 0; i < TBO_COUNT; i++)
	{
		tr.tbos[tr.numTbos++] = CreateTBO(tboBlocksInfo[i]);
	}
}

void R_DeleteBuildinTBOs(void)
{
	for (int i = 0; i < tr.numTbos; i++)
	{
		DeleteTBO(tr.tbos[i]);
	}
}

void R_SetTBOData(textureBuffer_t *tbo, int* data, int numComponents)
{
	if (tbo == NULL)
		return;

	qglBindBuffer(GL_TEXTURE_BUFFER, tbo->tbonum);
	int *buffer = (int*)qglMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
	for (int i = 0; i < numComponents; i++)
	{
		buffer[i] = data[i];
	}
	tbo->numItems += numComponents;
	qglUnmapBuffer(GL_TEXTURE_BUFFER);
	qglBindBuffer(GL_TEXTURE_BUFFER, 0);
}

matrix_t worldModelMatrix = {	1.0f, 0.0f, 0.0f, 0.0f,
								0.0f, 1.0f, 0.0f, 0.0f,
								0.0f, 0.0f, 1.0f, 0.0f,
								0.0f, 0.0f, 0.0f, 1.0f };

void R_TBOUpdateModelMatricesBuffer(const trRefdef_t *refDef)
{
	if (tr.tbos[TBO_MATRICES] == NULL)
		return;

	textureBuffer_t *tbo = tr.tbos[TBO_MATRICES];
	GL_UnbindTBO(tbo);

	qglBindBuffer(GL_TEXTURE_BUFFER, tbo->tbonum);
	matricesBlock *buffer = (matricesBlock*)qglMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);

	assert(buffer != nullptr);

	matrix_t invModelMatrix;
	matrix_t transInvModelMatrix;
	Matrix16Inverse(worldModelMatrix, invModelMatrix);
	Matrix16Transpose(invModelMatrix, transInvModelMatrix);

	Matrix16Copy(worldModelMatrix, buffer[0].modelMatrix);
	Matrix16Copy(invModelMatrix, buffer[0].normalMatrix);

	for (int i = 0; i < refDef->num_entities; i++)
	{
		refEntity_t ent = refDef->entities[i].e;

		if (ent.reType != RT_MODEL)
			continue;

		matrix_t modelMatrix = {	ent.axis[0][0], ent.axis[0][1], ent.axis[0][2], 0.f,
									ent.axis[1][0], ent.axis[1][1], ent.axis[1][2], 0.f,
									ent.axis[2][0], ent.axis[2][1], ent.axis[2][2], 0.f,
									ent.origin[0],  ent.origin[1],  ent.origin[2],  1.f
		};

		Matrix16Inverse(modelMatrix, invModelMatrix);
		Matrix16Transpose(invModelMatrix, transInvModelMatrix);

		assert(ent.hash - 1 < MatricesMaxSize);

		Matrix16Copy(modelMatrix, buffer[ent.hash - 1].modelMatrix);
		Matrix16Copy(transInvModelMatrix, buffer[ent.hash - 1].normalMatrix);
	}

	qglUnmapBuffer(GL_TEXTURE_BUFFER);
	qglBindBuffer(GL_TEXTURE_BUFFER, 0);

	GL_BindTBO(tbo);
}