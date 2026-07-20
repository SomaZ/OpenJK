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
#pragma once

#include "tr_local.h"

//===============================================================================
static byte* fileBaseIBSP;
/*
===============
R_ColorShiftLightingBytes

===============
*/
static	void R_ColorShiftLightingBytes( byte in[4], byte out[4] ) {
	int		shift, r, g, b;

	// shift the color data based on overbright range
	shift = Q_max( 0, r_mapOverBrightBits->integer - tr.overbrightBits );

	// shift the data based on overbright range
	r = in[0] << shift;
	g = in[1] << shift;
	b = in[2] << shift;

	// normalize by color instead of saturating to white
	if ( ( r | g | b ) > 255 ) {
		int		max;

		max = r > g ? r : g;
		max = max > b ? max : b;
		r = r * 255 / max;
		g = g * 255 / max;
		b = b * 255 / max;
	}

	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in[3];
}


/*
===============
R_ColorShiftLightingFloats

===============
*/
static void R_ColorShiftLightingFloats(float in[4], float out[4], float scale, bool overbrightBits )
{
	float r, g, b;

	if (overbrightBits)
		scale *= pow(2.0f, r_mapOverBrightBits->integer - tr.overbrightBits);

	r = in[0] * scale;
	g = in[1] * scale;
	b = in[2] * scale;

	if (!glRefConfig.floatLightmap)
	{
		if (r > 1.0f || g > 1.0f || b > 1.0f)
		{
			float high = Q_max(Q_max(r, g), b);

			r /= high;
			g /= high;
			b /= high;
		}
	}

	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in[3];
}

static void ColorToRGBA16F(const vec3_t color, unsigned short rgba16f[4])
{
	rgba16f[0] = FloatToHalf(color[0]);
	rgba16f[1] = FloatToHalf(color[1]);
	rgba16f[2] = FloatToHalf(color[2]);
	rgba16f[3] = FloatToHalf(1.0f);
}

static void HSVtoRGB(float h, float s, float v, float rgb[3])
{
	int i;
	float f;
	float p, q, t;

	h *= 5;

	i = floor(h);
	f = h - i;

	p = v * (1 - s);
	q = v * (1 - s * f);
	t = v * (1 - s * (1 - f));

	switch (i)
	{
	case 0:
		rgb[0] = v;
		rgb[1] = t;
		rgb[2] = p;
		break;
	case 1:
		rgb[0] = q;
		rgb[1] = v;
		rgb[2] = p;
		break;
	case 2:
		rgb[0] = p;
		rgb[1] = v;
		rgb[2] = t;
		break;
	case 3:
		rgb[0] = p;
		rgb[1] = q;
		rgb[2] = v;
		break;
	case 4:
		rgb[0] = t;
		rgb[1] = p;
		rgb[2] = v;
		break;
	case 5:
		rgb[0] = v;
		rgb[1] = p;
		rgb[2] = q;
		break;
	}
}

typedef struct {
	int			shaderNum;
	int			fogNum;
	int			surfaceType;

	int			firstVert;
	int			numVerts;

	int			firstIndex;
	int			numIndexes;

	int			lightmapNum;
	int			lightmapX, lightmapY;
	int			lightmapWidth, lightmapHeight;

	vec3_t		lightmapOrigin;
	vec3_t		lightmapVecs[3];	// for patches, [0] and [1] are lodbounds

	int			patchWidth;
	int			patchHeight;
} dsurfaceIBSP_t;

/*
===============
R_LoadLightmaps

===============
*/
#define	DEFAULT_LIGHTMAP_SIZE	128
#define MAX_LIGHTMAP_PAGES 2
static	void R_LoadLightmapsIBSP(world_t* worldData, lump_t* l, lump_t* surfs) {
	byte* buf = NULL, * buf_p = NULL;
	dsurfaceIBSP_t* surf;
	int			len;
	byte* image;
	int			imageSize;
	int			i, j, numLightmaps = 0, textureInternalFormat = 0;
	float maxIntensity = 0;
	int numColorComponents = 3;

	bool hdr_capable = glRefConfig.floatLightmap && r_hdr->integer;

	tr.lightmapSize = DEFAULT_LIGHTMAP_SIZE;
	tr.hdrLighting = qfalse;
	tr.worldInternalLightmapping = qfalse;

	len = l->filelen;
	// test for external lightmaps
	if (!len) {
		for (i = 0, surf = (dsurfaceIBSP_t*)(fileBaseIBSP + surfs->fileofs);
			(unsigned)i < surfs->filelen / sizeof(dsurfaceIBSP_t);
			i++, surf++) {
			for (int j = 0; j < MAXLIGHTMAPS; j++)
			{
				numLightmaps = MAX(numLightmaps, LittleLong(surf->lightmapNum) + 1);
			}
		}
		buf = NULL;
	}
	else
	{
		numLightmaps = len / (tr.lightmapSize * tr.lightmapSize * 3);
		buf = fileBaseIBSP + l->fileofs;
		tr.worldInternalLightmapping = qtrue;
	}

	if (numLightmaps == 0)
		return;

	// test for hdr lighting
	if (hdr_capable && tr.worldInternalLightmapping)
	{
		char filename[MAX_QPATH];
		byte* externalLightmap = NULL;
		int lightmapWidth = tr.lightmapSize;
		int lightmapHeight = tr.lightmapSize;
		Com_sprintf(filename, sizeof(filename), "maps/%s/lm_%04d.hdr", worldData->baseName, 0);
		R_LoadHDRImage(filename, &externalLightmap, &lightmapWidth, &lightmapHeight);
		if (externalLightmap != NULL)
		{
			tr.worldInternalLightmapping = qfalse;
			Hunk_FreeTempMemory(externalLightmap);
		}
	}

	// we are about to upload textures
	R_IssuePendingRenderCommands();

	// check for deluxe mapping
	if (numLightmaps <= 1)
	{
		tr.worldDeluxeMapping = qfalse;
	}
	else
	{
		tr.worldDeluxeMapping = qtrue;
		tr.worldInternalDeluxeMapping = qtrue;
		// Check that none of the deluxe maps are referenced by any of the map surfaces.
		for (i = 0, surf = (dsurfaceIBSP_t*)(fileBaseIBSP + surfs->fileofs);
			tr.worldDeluxeMapping && (unsigned)i < surfs->filelen / sizeof(dsurface_t);
			i++, surf++) {
			for (int j = 0; j < MAXLIGHTMAPS; j++)
			{
				int lightmapNum = LittleLong(surf->lightmapNum);

				if (lightmapNum >= 0 && (lightmapNum & 1) != 0) {
					tr.worldDeluxeMapping = qfalse;
					tr.worldInternalDeluxeMapping = qfalse;
					break;
				}
			}
		}
		if (tr.worldDeluxeMapping == qtrue && (!len))
			numLightmaps++;
	}

	imageSize = tr.lightmapSize * tr.lightmapSize * 4 * 2;
	image = (byte*)Z_Malloc(imageSize, TAG_BSP, qfalse);

	if (tr.worldDeluxeMapping)
		numLightmaps >>= 1;

	if (tr.worldInternalLightmapping)
	{
		const int targetLightmapsPerX = (int)ceilf(sqrtf(numLightmaps));

		int lightmapsPerX = 1;
		while (lightmapsPerX < targetLightmapsPerX)
			lightmapsPerX *= 2;

		tr.lightmapsPerAtlasSide[0] = lightmapsPerX;
		tr.lightmapsPerAtlasSide[1] = (int)ceilf((float)numLightmaps / lightmapsPerX);

		tr.lightmapAtlasSize[0] = tr.lightmapsPerAtlasSide[0] * LIGHTMAP_WIDTH;
		tr.lightmapAtlasSize[1] = tr.lightmapsPerAtlasSide[1] * LIGHTMAP_HEIGHT;

		// FIXME: What happens if we need more?
		tr.numLightmaps = 1;
	}
	else
	{
		tr.numLightmaps = numLightmaps;
	}

	tr.lightmaps = (image_t**)Hunk_Alloc(tr.numLightmaps * sizeof(image_t*), h_low);

	if (tr.worldDeluxeMapping)
	{
		tr.deluxemaps = (image_t**)Hunk_Alloc(tr.numLightmaps * sizeof(image_t*), h_low);
	}

	if (hdr_capable)
		textureInternalFormat = GL_RGBA16F;
	else
		textureInternalFormat = GL_RGBA8;

	if (tr.worldInternalLightmapping)
	{
		for (i = 0; i < tr.numLightmaps; i++)
		{
			tr.lightmaps[i] = R_CreateImage(
				va("_lightmapatlas%d", i),
				NULL,
				tr.lightmapAtlasSize[0],
				tr.lightmapAtlasSize[1],
				IMGTYPE_COLORALPHA,
				IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE,
				textureInternalFormat);

			if (tr.worldDeluxeMapping)
			{
				tr.deluxemaps[i] = R_CreateImage(
					va("_fatdeluxemap%d", i),
					NULL,
					tr.lightmapAtlasSize[0],
					tr.lightmapAtlasSize[1],
					IMGTYPE_DELUXE,
					IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE,
					0);
			}
		}
	}

	for (i = 0; i < numLightmaps; i++)
	{
		int xoff = 0, yoff = 0;
		int lightmapnum = i;
		// expand the 24 bit on-disk to 32 bit

		if (tr.worldInternalLightmapping)
		{
			xoff = (i % tr.lightmapsPerAtlasSide[0]) * tr.lightmapSize;
			yoff = (i / tr.lightmapsPerAtlasSide[0]) * tr.lightmapSize;
			lightmapnum = 0;
		}

		// if (tr.worldLightmapping)
		{
			char filename[MAX_QPATH];
			byte* externalLightmap = NULL;
			float* hdrL = NULL;
			int lightmapWidth = tr.lightmapSize;
			int lightmapHeight = tr.lightmapSize;
			int bppc;
			bool foundLightmap = true;

			if (!tr.worldInternalLightmapping)
			{
				if (hdr_capable)
					Com_sprintf(filename, sizeof(filename), "maps/%s/lm_%04d.hdr", worldData->baseName, i * (tr.worldDeluxeMapping ? 2 : 1));
				else
					Com_sprintf(filename, sizeof(filename), "maps/%s/lm_%04d.tga", worldData->baseName, i * (tr.worldDeluxeMapping ? 2 : 1));

				bppc = 16;
				R_LoadHDRImage(filename, &externalLightmap, &lightmapWidth, &lightmapHeight);
				if (!externalLightmap)
				{
					bppc = 8;
					R_LoadImage(filename, &externalLightmap, &lightmapWidth, &lightmapHeight);
				}
			}

			if (externalLightmap)
			{
				int newImageSize = lightmapWidth * lightmapHeight * 4 * 2;
				if (tr.worldInternalLightmapping && (lightmapWidth != tr.lightmapSize || lightmapHeight != tr.lightmapSize))
				{
					ri.Printf(PRINT_ALL, "Error loading %s: non %dx%d lightmaps\n", filename, tr.lightmapSize, tr.lightmapSize);
					Z_Free(externalLightmap);
					externalLightmap = NULL;
					continue;
				}
				else if (newImageSize > imageSize)
				{
					Z_Free(image);
					imageSize = newImageSize;
					image = (byte*)Z_Malloc(imageSize, TAG_BSP, qfalse);
				}
				numColorComponents = 4;
			}
			if (!externalLightmap)
			{
				lightmapWidth = tr.lightmapSize;
				lightmapHeight = tr.lightmapSize;
				numColorComponents = 3;
			}

			foundLightmap = true;
			if (externalLightmap)
			{
				if (bppc > 8)
				{
					hdrL = (float*)externalLightmap;
					tr.hdrLighting = qtrue;
				}
				else
				{
					buf_p = externalLightmap;
				}
			}
			else if (buf)
			{
				if (tr.worldDeluxeMapping)
					buf_p = buf + (i * 2) * tr.lightmapSize * tr.lightmapSize * 3;
				else
					buf_p = buf + i * tr.lightmapSize * tr.lightmapSize * 3;
			}
			else
			{
				buf_p = NULL;
				foundLightmap = false;
			}

			if (foundLightmap)
			{
				for (j = 0; j < lightmapWidth * lightmapHeight; j++)
				{
					if (hdrL && hdr_capable)
					{
						vec4_t color;
						int column = (j % lightmapWidth);
						int rowIndex = (int)floor(j / lightmapHeight) * lightmapHeight;

						int index = column + rowIndex;

						memcpy(color, &hdrL[index * 3], 12);

						color[3] = 1.0f;

						R_ColorShiftLightingFloats(color, color, 1.0f, false);

						ColorToRGBA16F(color, (uint16_t*)(&image[j * 8]));
					}
					else if (buf_p && hdr_capable)
					{
						vec4_t color;

						//hack: convert LDR lightmap to HDR one
						color[0] = MAX(buf_p[j * numColorComponents + 0], 0.499f);
						color[1] = MAX(buf_p[j * numColorComponents + 1], 0.499f);
						color[2] = MAX(buf_p[j * numColorComponents + 2], 0.499f);

						// if under an arbitrary value (say 12) grey it out
						// this prevents weird splotches in dimly lit areas
						if (color[0] + color[1] + color[2] < 12.0f)
						{
							float avg = (color[0] + color[1] + color[2]) * 0.3333f;
							color[0] = avg;
							color[1] = avg;
							color[2] = avg;
						}
						color[3] = 1.0f;

						R_ColorShiftLightingFloats(color, color, 1.0f / 255.0f, false);

						ColorToRGBA16F(color, (unsigned short*)(&image[j * 8]));
					}
					else if (buf_p)
					{
						if (r_lightmap->integer == 2)
						{	// color code by intensity as development tool	(FIXME: check range)
							float r = buf_p[j * numColorComponents + 0];
							float g = buf_p[j * numColorComponents + 1];
							float b = buf_p[j * numColorComponents + 2];
							float intensity;
							float out[3] = { 0.0, 0.0, 0.0 };

							intensity = 0.33f * r + 0.685f * g + 0.063f * b;

							if (intensity > 255)
								intensity = 1.0f;
							else
								intensity /= 255.0f;

							if (intensity > maxIntensity)
								maxIntensity = intensity;

							HSVtoRGB(intensity, 1.00, 0.50, out);

							image[j * 4 + 0] = out[0] * 255;
							image[j * 4 + 1] = out[1] * 255;
							image[j * 4 + 2] = out[2] * 255;
							image[j * 4 + 3] = 255;
						}
						else
						{
							R_ColorShiftLightingBytes(&buf_p[j * numColorComponents], &image[j * 4]);
							image[j * 4 + 3] = 255;
						}
					}
				}

				if (tr.worldInternalLightmapping)
					R_UpdateSubImage(
						tr.lightmaps[lightmapnum],
						image,
						xoff,
						yoff,
						lightmapWidth,
						lightmapHeight);
				else
					tr.lightmaps[i] = R_CreateImage(
						va("*lightmap%d", i),
						image,
						lightmapWidth,
						lightmapHeight,
						IMGTYPE_COLORALPHA,
						IMGFLAG_NOLIGHTSCALE |
						IMGFLAG_NO_COMPRESSION |
						IMGFLAG_CLAMPTOEDGE,
						textureInternalFormat);
			}

			if (externalLightmap)
				Z_Free(externalLightmap);
		}

		if (tr.worldDeluxeMapping && buf)
		{
			buf_p = buf + (i * 2 + 1) * tr.lightmapSize * tr.lightmapSize * 3;

			for (j = 0; j < tr.lightmapSize * tr.lightmapSize; j++) {
				image[j * 4 + 0] = buf_p[j * 3 + 0];
				image[j * 4 + 1] = buf_p[j * 3 + 1];
				image[j * 4 + 2] = buf_p[j * 3 + 2];

				// make 0,0,0 into 127,127,127
				if ((image[j * 4 + 0] == 0) && (image[j * 4 + 1] == 0) && (image[j * 4 + 2] == 0))
				{
					image[j * 4 + 0] =
						image[j * 4 + 1] =
						image[j * 4 + 2] = 127;
				}

				image[j * 4 + 3] = 255;
			}

			if (tr.worldInternalLightmapping)
			{
				R_UpdateSubImage(
					tr.deluxemaps[lightmapnum],
					image,
					xoff,
					yoff,
					tr.lightmapSize,
					tr.lightmapSize);
			}
			else
			{
				tr.deluxemaps[i] = R_CreateImage(
					va("*deluxemap%d", i),
					image,
					tr.lightmapSize,
					tr.lightmapSize,
					IMGTYPE_DELUXE,
					IMGFLAG_NOLIGHTSCALE |
					IMGFLAG_NO_COMPRESSION |
					IMGFLAG_CLAMPTOEDGE,
					0);
			}
		}
		else if (r_deluxeMapping->integer)
		{
			char filename[MAX_QPATH];
			byte* externalLightmap = NULL;
			int lightmapWidth = tr.lightmapSize;
			int lightmapHeight = tr.lightmapSize;

			// try loading additional deluxemaps
			if (tr.worldDeluxeMapping)
				Com_sprintf(filename, sizeof(filename), "maps/%s/lm_%04d.tga", worldData->baseName, i * 2 + 1);
			else
				Com_sprintf(filename, sizeof(filename), "maps/%s/dm_%04d.tga", worldData->baseName, i);

			R_LoadImage(filename, &externalLightmap, &lightmapWidth, &lightmapHeight);
			if (!externalLightmap)
				continue;

			if (tr.worldInternalLightmapping && (lightmapWidth != tr.lightmapSize || lightmapHeight != tr.lightmapSize))
			{
				ri.Printf(PRINT_ALL, "Error loading %s: non %dx%d deluxemaps\n", filename, tr.lightmapSize, tr.lightmapSize);
				Z_Free(externalLightmap);
				externalLightmap = NULL;
				continue;
			}

			int newImageSize = lightmapWidth * lightmapHeight * 4 * 2;
			if (newImageSize > imageSize)
			{
				Z_Free(image);
				imageSize = newImageSize;
				image = (byte*)Z_Malloc(imageSize, TAG_BSP, qfalse);
			}

			buf_p = externalLightmap;

			for (j = 0; j < lightmapWidth * lightmapHeight; j++) {
				image[j * 4 + 0] = buf_p[j * 4 + 0];
				image[j * 4 + 1] = buf_p[j * 4 + 1];
				image[j * 4 + 2] = buf_p[j * 4 + 2];

				// make 0,0,0 into 127,127,127
				if ((image[j * 4 + 0] == 0) && (image[j * 4 + 1] == 0) && (image[j * 4 + 2] == 0))
				{
					image[j * 4 + 0] =
						image[j * 4 + 1] =
						image[j * 4 + 2] = 127;
				}

				image[j * 4 + 3] = 255;
			}

			if (!tr.deluxemaps)
			{
				tr.deluxemaps = (image_t**)Hunk_Alloc(tr.numLightmaps * sizeof(image_t*), h_low);
				if (tr.worldInternalLightmapping)
				{
					tr.deluxemaps[lightmapnum] = R_CreateImage(
						va("_fatdeluxemap%d", i),
						NULL,
						tr.lightmapAtlasSize[0],
						tr.lightmapAtlasSize[1],
						IMGTYPE_DELUXE,
						IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE,
						0);
				}
			}

			if (tr.worldInternalLightmapping)
			{
				R_UpdateSubImage(
					tr.deluxemaps[lightmapnum],
					image,
					xoff,
					yoff,
					lightmapWidth,
					lightmapHeight);
			}
			else
			{
				tr.deluxemaps[i] = R_CreateImage(
					va("*deluxemap%d", i),
					image,
					lightmapWidth,
					lightmapHeight,
					IMGTYPE_DELUXE,
					IMGFLAG_NOLIGHTSCALE |
					IMGFLAG_NO_COMPRESSION |
					IMGFLAG_CLAMPTOEDGE,
					0);
			}

			Z_Free(externalLightmap);
			externalLightmap = NULL;
		}
	}

	if (r_lightmap->integer == 2) {
		ri.Printf(PRINT_ALL, "Brightest lightmap value: %d\n", (int)(maxIntensity * 255));
	}

	Z_Free(image);

	if (tr.deluxemaps)
		tr.worldDeluxeMapping = qtrue;
}

static float FatPackU(float input, int lightmapnum)
{
	if (lightmapnum < 0)
		return input;

	if (tr.worldInternalDeluxeMapping)
		lightmapnum >>= 1;

	if (tr.lightmapAtlasSize[0] > 0)
	{
		const int lightmapXOffset = lightmapnum % tr.lightmapsPerAtlasSide[0];
		const float invLightmapSide = 1.0f / tr.lightmapsPerAtlasSide[0];

		return (lightmapXOffset * invLightmapSide) + (input * invLightmapSide);
	}

	return input;
}

static float FatPackV(float input, int lightmapnum)
{
	if (lightmapnum < 0)
		return input;

	if (tr.worldInternalDeluxeMapping)
		lightmapnum >>= 1;

	if (tr.lightmapAtlasSize[1] > 0)
	{
		const int lightmapYOffset = lightmapnum / tr.lightmapsPerAtlasSide[0];
		const float invLightmapSide = 1.0f / tr.lightmapsPerAtlasSide[1];

		return (lightmapYOffset * invLightmapSide) + (input * invLightmapSide);
	}

	return input;
}


static int FatLightmap(int lightmapnum)
{
	if (lightmapnum < 0)
		return lightmapnum;

	if (tr.worldInternalDeluxeMapping)
		lightmapnum >>= 1;

	if (tr.lightmapAtlasSize[0] > 0)
		return 0;

	return lightmapnum;
}

/*
===============
ShaderForShaderNum
===============
*/
static shader_t *ShaderForShaderNum( const world_t *worldData, int shaderNum, const int *lightmapNums, const byte *lightmapStyles, const byte *vertexStyles ) {
	shader_t	*shader;
	dshader_t	*dsh;
	const byte	*styles = lightmapStyles;

	int _shaderNum = LittleLong( shaderNum );
	if ( _shaderNum < 0 || _shaderNum >= worldData->numShaders ) {
		ri.Error( ERR_DROP, "ShaderForShaderNum: bad num %i", _shaderNum );
	}
	dsh = &worldData->shaders[ _shaderNum ];

	if ( lightmapNums[0] == LIGHTMAP_BY_VERTEX ) {
		styles = vertexStyles;
	}

	if ( r_vertexLight->integer ) {
		lightmapNums = lightmapsVertex;
		styles = vertexStyles;
	}

	if ( r_fullbright->integer ) {
		lightmapNums = lightmapsFullBright;
	}

	shader = R_FindShader( dsh->shader, lightmapNums, styles, qtrue );

	// if the shader had errors, just use default shader
	if ( shader->defaultShader ) {
		return tr.defaultShader;
	}

	return shader;
}

typedef struct {
	vec3_t		xyz;
	float		st[2];
	float		lightmap[2];
	vec3_t		normal;
	byte		color[4];
} drawVertIBSP_t;

const byte ibspLightmapStyles[MAXLIGHTMAPS] = {
	0x00,
	0xff,
	0xff,
	0xff
};

/*
===============
ParseFace
===============
*/
static void ParseFace( const world_t *worldData, dsurfaceIBSP_t*ds, drawVertIBSP_t*verts, packedTangentSpace_t *tangentSpace, float *hdrVertColors, msurface_t *surf, int *indexes  ) {
	int			i, j;
	srfBspSurface_t	*cv;
	glIndex_t  *tri;
	int			numVerts, numIndexes, badTriangles;
	int realLightmapNum[MAXLIGHTMAPS];

	realLightmapNum[0] = FatLightmap(LittleLong(ds->lightmapNum));
	realLightmapNum[1] = -3;
	realLightmapNum[2] = -3;
	realLightmapNum[3] = -3;

	surf->numSurfaceSprites = 0;
	surf->surfaceSprites = nullptr;

	// get fog volume
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	if (!surf->fogIndex && worldData->globalFog != nullptr)
	{
		surf->fogIndex = worldData->globalFogIndex;
	}

	// get shader value
	surf->shader = ShaderForShaderNum( worldData, ds->shaderNum, realLightmapNum, ibspLightmapStyles, ibspLightmapStyles);
	if ( r_singleShader->integer && !surf->shader->isSky ) {
		surf->shader = tr.defaultShader;
	}

	numVerts = LittleLong(ds->numVerts);
	numIndexes = LittleLong(ds->numIndexes);

	//cv = Hunk_Alloc(sizeof(*cv), h_low);
	cv = (srfBspSurface_t *)surf->data;
	cv->surfaceType = SF_FACE;

	cv->numIndexes = numIndexes;
	cv->indexes = (glIndex_t *)Hunk_Alloc(numIndexes * sizeof(cv->indexes[0]), h_low);

	cv->numVerts = numVerts;
	cv->verts = (srfVert_t *)Hunk_Alloc(numVerts * sizeof(cv->verts[0]), h_low);

	// copy vertexes
	surf->cullinfo.type = CULLINFO_PLANE | CULLINFO_BOX;
	ClearBounds(surf->cullinfo.bounds[0], surf->cullinfo.bounds[1]);
	verts += LittleLong(ds->firstVert);
	if (tangentSpace)
		tangentSpace += LittleLong(ds->firstVert);
	for (i = 0; i < numVerts; i++)
	{
		vec4_t color;

		for (j = 0; j < 3; j++)
		{
			cv->verts[i].xyz[j] = LittleFloat(verts[i].xyz[j]);
			cv->verts[i].normal[j] = LittleFloat(verts[i].normal[j]);
		}

		if (tangentSpace)
		{
			for (j = 0; j < 4; j++)
				cv->verts[i].tangent[j] = LittleFloat(tangentSpace[i].tangentAndSign[j]);
		}

		AddPointToBounds(cv->verts[i].xyz, surf->cullinfo.bounds[0], surf->cullinfo.bounds[1]);

		for (j = 0; j < 2; j++)
		{
			cv->verts[i].st[j] = LittleFloat(verts[i].st[j]);
		}


		cv->verts[i].lightmap[0][0] = FatPackU(
			LittleFloat(verts[i].lightmap[0]), ds->lightmapNum);
		cv->verts[i].lightmap[0][1] = FatPackV(
			LittleFloat(verts[i].lightmap[1]), ds->lightmapNum);

		float scale = 1.0f / 255.0f;
		if (hdrVertColors)
		{
			float* hdrColor = hdrVertColors + (ds->firstVert + i) * 3;
			color[0] = hdrColor[0];
			color[1] = hdrColor[1];
			color[2] = hdrColor[2];
			scale = 1.0f;
		}
		else
		{
			//hack: convert LDR vertex colors to HDR
			if (r_hdr->integer)
			{
				color[0] = MAX(verts[i].color[0], 0.499f);
				color[1] = MAX(verts[i].color[1], 0.499f);
				color[2] = MAX(verts[i].color[2], 0.499f);
			}
			else
			{
				color[0] = verts[i].color[0];
				color[1] = verts[i].color[1];
				color[2] = verts[i].color[2];
			}
		}
		color[3] = verts[i].color[3] / 255.0f;

		R_ColorShiftLightingFloats(color, cv->verts[i].vertexColors[0], scale, hdrVertColors != NULL);

	}

	// copy triangles
	badTriangles = 0;
	indexes += LittleLong(ds->firstIndex);
	for(i = 0, tri = cv->indexes; i < numIndexes; i += 3, tri += 3)
	{
		for(j = 0; j < 3; j++)
		{
			tri[j] = LittleLong(indexes[i + j]);

			if(tri[j] >= (unsigned)numVerts)
			{
				ri.Error(ERR_DROP, "Bad index in face surface");
			}
		}

		if ((tri[0] == tri[1]) || (tri[1] == tri[2]) || (tri[0] == tri[2]))
		{
			tri -= 3;
			badTriangles++;
		}
	}

	if (badTriangles)
	{
		ri.Printf(PRINT_WARNING, "Face has bad triangles, originally shader %s %d tris %d verts, now %d tris\n", surf->shader->name, numIndexes / 3, numVerts, numIndexes / 3 - badTriangles);
		cv->numIndexes -= badTriangles * 3;
	}

	// take the plane information from the lightmap vector
	for ( i = 0 ; i < 3 ; i++ ) {
		cv->cullPlane.normal[i] = LittleFloat( ds->lightmapVecs[2][i] );
	}
	cv->cullPlane.dist = DotProduct( cv->verts[0].xyz, cv->cullPlane.normal );
	SetPlaneSignbits( &cv->cullPlane );
	cv->cullPlane.type = PlaneTypeForNormal( cv->cullPlane.normal );
	surf->cullinfo.plane = cv->cullPlane;

	surf->data = (surfaceType_t *)cv;
}


/*
===============
ParseMesh
===============
*/
static void ParseMesh ( const world_t *worldData, dsurfaceIBSP_t *ds, drawVertIBSP_t*verts, packedTangentSpace_t *tangentSpace, float *hdrVertColors, msurface_t *surf ) {
	srfBspSurface_t	*grid;
	int				i, j;
	int				width, height, numPoints;
	srfVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE];
	vec3_t			bounds[2];
	vec3_t			tmpVec;
	static surfaceType_t	skipData = SF_SKIP;
	int realLightmapNum[MAXLIGHTMAPS];

	realLightmapNum[0] = FatLightmap(LittleLong(ds->lightmapNum));
	realLightmapNum[1] = -3;
	realLightmapNum[2] = -3;
	realLightmapNum[3] = -3;

	surf->numSurfaceSprites = 0;
	surf->surfaceSprites = nullptr;

	// get fog volume
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	if (!surf->fogIndex && worldData->globalFog != nullptr)
	{
		surf->fogIndex = worldData->globalFogIndex;
	}

	// get shader value
	surf->shader = ShaderForShaderNum( worldData, ds->shaderNum, realLightmapNum, ibspLightmapStyles, ibspLightmapStyles );
	if ( r_singleShader->integer && !surf->shader->isSky ) {
		surf->shader = tr.defaultShader;
	}

	// we may have a nodraw surface, because they might still need to
	// be around for movement clipping
	if ( worldData->shaders[ LittleLong( ds->shaderNum ) ].surfaceFlags & SURF_NODRAW ) {
		surf->data = &skipData;
		return;
	}

	width = LittleLong( ds->patchWidth );
	height = LittleLong( ds->patchHeight );

	if(width < 0 || width > MAX_PATCH_SIZE || height < 0 || height > MAX_PATCH_SIZE)
		ri.Error(ERR_DROP, "ParseMesh: bad size");

	verts += LittleLong( ds->firstVert );
	if (tangentSpace)
		tangentSpace += LittleLong(ds->firstVert);
	numPoints = width * height;
	for (i = 0; i < numPoints; i++)
	{
		vec4_t color;

		for (j = 0; j < 3; j++)
		{
			points[i].xyz[j] = LittleFloat(verts[i].xyz[j]);
			points[i].normal[j] = LittleFloat(verts[i].normal[j]);
		}

		if (tangentSpace)
		{
			for (j = 0; j < 4; j++)
				points[i].tangent[j] = LittleFloat(tangentSpace[i].tangentAndSign[j]);
		}

		for (j = 0; j < 2; j++)
		{
			points[i].st[j] = LittleFloat(verts[i].st[j]);
		}


		points[i].lightmap[j][0] = FatPackU(LittleFloat(verts[i].lightmap[0]), ds->lightmapNum);
		points[i].lightmap[j][1] = FatPackV(LittleFloat(verts[i].lightmap[1]), ds->lightmapNum);

		float scale = 1.0f / 255.0f;
		if (hdrVertColors)
		{
			float* hdrColor = hdrVertColors + (ds->firstVert + i) * 3;
			color[0] = hdrColor[0];
			color[1] = hdrColor[1];
			color[2] = hdrColor[2];
			scale = 1.0f;
		}
		else
		{
			//hack: convert LDR vertex colors to HDR
			if (r_hdr->integer)
			{
				color[0] = MAX(verts[i].color[0], 0.499f);
				color[1] = MAX(verts[i].color[1], 0.499f);
				color[2] = MAX(verts[i].color[2], 0.499f);
			}
			else
			{
				color[0] = verts[i].color[0];
				color[1] = verts[i].color[1];
				color[2] = verts[i].color[2];
			}
		}
		color[3] = verts[i].color[3] / 255.0f;

		R_ColorShiftLightingFloats(color, points[i].vertexColors[0], scale, hdrVertColors != NULL);

	}

	// pre-tesseleate
	grid = R_SubdividePatchToGrid( width, height, points );
	surf->data = (surfaceType_t *)grid;

	// copy the level of detail origin, which is the center
	// of the group of all curves that must subdivide the same
	// to avoid cracking
	for ( i = 0 ; i < 3 ; i++ ) {
		bounds[0][i] = LittleFloat( ds->lightmapVecs[0][i] );
		bounds[1][i] = LittleFloat( ds->lightmapVecs[1][i] );
	}
	VectorAdd( bounds[0], bounds[1], bounds[1] );
	VectorScale( bounds[1], 0.5f, grid->lodOrigin );
	VectorSubtract( bounds[0], grid->lodOrigin, tmpVec );
	grid->lodRadius = VectorLength( tmpVec );
}

/*
===============
ParseTriSurf
===============
*/
static void ParseTriSurf( const world_t *worldData, dsurfaceIBSP_t *ds, drawVertIBSP_t*verts, packedTangentSpace_t *tangentSpace, float *hdrVertColors, msurface_t *surf, int *indexes ) {
	srfBspSurface_t *cv;
	glIndex_t  *tri;
	int             i, j;
	int             numVerts, numIndexes, badTriangles;
	int realLightmapNum[MAXLIGHTMAPS];

	realLightmapNum[0] = FatLightmap(LittleLong(ds->lightmapNum));
	realLightmapNum[1] = -3;
	realLightmapNum[2] = -3;
	realLightmapNum[3] = -3;

	surf->numSurfaceSprites = 0;
	surf->surfaceSprites = nullptr;

	// get fog volume
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	if (!surf->fogIndex && worldData->globalFog != nullptr)
	{
		surf->fogIndex = worldData->globalFogIndex;
	}

	// get shader
	surf->shader = ShaderForShaderNum( worldData, ds->shaderNum, realLightmapNum, ibspLightmapStyles, ibspLightmapStyles );
	if ( r_singleShader->integer && !surf->shader->isSky ) {
		surf->shader = tr.defaultShader;
	}

	numVerts = LittleLong(ds->numVerts);
	numIndexes = LittleLong(ds->numIndexes);

	//cv = Hunk_Alloc(sizeof(*cv), h_low);
	cv = (srfBspSurface_t *)surf->data;
	cv->surfaceType = SF_TRIANGLES;

	cv->numIndexes = numIndexes;
	cv->indexes = (glIndex_t *)Hunk_Alloc(numIndexes * sizeof(cv->indexes[0]), h_low);

	cv->numVerts = numVerts;
	cv->verts = (srfVert_t *)Hunk_Alloc(numVerts * sizeof(cv->verts[0]), h_low);

	surf->data = (surfaceType_t *) cv;

	// copy vertexes
	surf->cullinfo.type = CULLINFO_BOX;
	ClearBounds(surf->cullinfo.bounds[0], surf->cullinfo.bounds[1]);
	verts += LittleLong(ds->firstVert);
	if (tangentSpace)
		tangentSpace += LittleLong(ds->firstVert);
	for (i = 0; i < numVerts; i++)
	{
		vec4_t color;

		for (j = 0; j < 3; j++)
		{
			cv->verts[i].xyz[j] = LittleFloat(verts[i].xyz[j]);
			cv->verts[i].normal[j] = LittleFloat(verts[i].normal[j]);
		}

		if (tangentSpace)
		{
			for (j = 0; j < 4; j++)
				cv->verts[i].tangent[j] = LittleFloat(tangentSpace[i].tangentAndSign[j]);
		}

		AddPointToBounds(cv->verts[i].xyz, surf->cullinfo.bounds[0], surf->cullinfo.bounds[1]);

		for (j = 0; j < 2; j++)
		{
			cv->verts[i].st[j] = LittleFloat(verts[i].st[j]);
		}

		cv->verts[i].lightmap[j][0] = FatPackU(
			LittleFloat(verts[i].lightmap[0]), ds->lightmapNum);
		cv->verts[i].lightmap[j][1] = FatPackV(
			LittleFloat(verts[i].lightmap[1]), ds->lightmapNum);

		float scale = 1.0f / 255.0f;
		if (hdrVertColors)
		{
			float* hdrColor = hdrVertColors + ((ds->firstVert + i) * 3);
			color[0] = hdrColor[0];
			color[1] = hdrColor[1];
			color[2] = hdrColor[2];
			scale = 1.0f;
		}
		else
		{
			//hack: convert LDR vertex colors to HDR
			if (r_hdr->integer)
			{
				color[0] = MAX(verts[i].color[0], 0.499f);
				color[1] = MAX(verts[i].color[1], 0.499f);
				color[2] = MAX(verts[i].color[2], 0.499f);
			}
			else
			{
				color[0] = verts[i].color[0];
				color[1] = verts[i].color[1];
				color[2] = verts[i].color[2];
			}
		}
		color[3] = verts[i].color[3] / 255.0f;

		R_ColorShiftLightingFloats(color, cv->verts[i].vertexColors[0], scale, hdrVertColors != NULL);
	}

	// copy triangles
	badTriangles = 0;
	indexes += LittleLong(ds->firstIndex);
	for(i = 0, tri = cv->indexes; i < numIndexes; i += 3, tri += 3)
	{
		for(j = 0; j < 3; j++)
		{
			tri[j] = LittleLong(indexes[i + j]);

			if(tri[j] >= (unsigned)numVerts)
			{
				ri.Error(ERR_DROP, "Bad index in face surface");
			}
		}

		if ((tri[0] == tri[1]) || (tri[1] == tri[2]) || (tri[0] == tri[2]))
		{
			tri -= 3;
			badTriangles++;
		}
	}

	if (badTriangles)
	{
		ri.Printf(PRINT_WARNING, "Trisurf has bad triangles, originally shader %s %d tris %d verts, now %d tris\n", surf->shader->name, numIndexes / 3, numVerts, numIndexes / 3 - badTriangles);
		cv->numIndexes -= badTriangles * 3;
	}
}

/*
===============
ParseFlare
===============
*/
static void ParseFlare( const world_t *worldData, dsurfaceIBSP_t *ds, drawVertIBSP_t*verts, msurface_t *surf, int *indexes ) {
	srfFlare_t		*flare;
	int				i;

	surf->numSurfaceSprites = 0;
	surf->surfaceSprites = nullptr;

	// get fog volume
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	if (!surf->fogIndex && worldData->globalFog != nullptr)
	{
		surf->fogIndex = worldData->globalFogIndex;
	}

	// get shader
	surf->shader = ShaderForShaderNum( worldData, ds->shaderNum, lightmapsVertex, ibspLightmapStyles, ibspLightmapStyles );
	if ( r_singleShader->integer && !surf->shader->isSky ) {
		surf->shader = tr.defaultShader;
	}

	//flare = Hunk_Alloc( sizeof( *flare ), h_low );
	flare = (srfFlare_t *)surf->data;
	flare->surfaceType = SF_FLARE;

	if (surf->shader == tr.defaultShader)
		flare->shader = tr.flareShader;
	else
		flare->shader = surf->shader;

	surf->data = (surfaceType_t *)flare;

	for ( i = 0 ; i < 3 ; i++ ) {
		flare->origin[i] = LittleFloat( ds->lightmapOrigin[i] );
		flare->color[i] = LittleFloat( ds->lightmapVecs[0][i] );
		flare->normal[i] = LittleFloat( ds->lightmapVecs[2][i] );
	}
}


/*
=================
R_MergedWidthPoints

returns true if there are grid points merged on a width edge
=================
*/
static int R_MergedWidthPoints(srfBspSurface_t *grid, int offset) {
	int i, j;

	for (i = 1; i < grid->width-1; i++) {
		for (j = i + 1; j < grid->width-1; j++) {
			if ( fabs(grid->verts[i + offset].xyz[0] - grid->verts[j + offset].xyz[0]) > .1) continue;
			if ( fabs(grid->verts[i + offset].xyz[1] - grid->verts[j + offset].xyz[1]) > .1) continue;
			if ( fabs(grid->verts[i + offset].xyz[2] - grid->verts[j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
R_MergedHeightPoints

returns true if there are grid points merged on a height edge
=================
*/
static int R_MergedHeightPoints(srfBspSurface_t *grid, int offset) {
	int i, j;

	for (i = 1; i < grid->height-1; i++) {
		for (j = i + 1; j < grid->height-1; j++) {
			if ( fabs(grid->verts[grid->width * i + offset].xyz[0] - grid->verts[grid->width * j + offset].xyz[0]) > .1) continue;
			if ( fabs(grid->verts[grid->width * i + offset].xyz[1] - grid->verts[grid->width * j + offset].xyz[1]) > .1) continue;
			if ( fabs(grid->verts[grid->width * i + offset].xyz[2] - grid->verts[grid->width * j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
R_FixSharedVertexLodError_r

NOTE: never sync LoD through grid edges with merged points!

FIXME: write generalized version that also avoids cracks between a patch and one that meets half way?
=================
*/
static void R_FixSharedVertexLodError_r( world_t *worldData, int start, srfBspSurface_t *grid1 ) {
	int j, k, l, m, n, offset1, offset2, touch;
	srfBspSurface_t *grid2;

	for ( j = start; j < worldData->numsurfaces; j++ ) {
		//
		grid2 = (srfBspSurface_t *) worldData->surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// if the LOD errors are already fixed for this patch
		if ( grid2->lodFixed == 2 ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		touch = qfalse;
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = (grid1->height-1) * grid1->width;
			else offset1 = 0;
			if (R_MergedWidthPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->width-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = grid1->width-1;
			else offset1 = 0;
			if (R_MergedHeightPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->height-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		if (touch) {
			grid2->lodFixed = 2;
			R_FixSharedVertexLodError_r ( worldData, start, grid2 );
			//NOTE: this would be correct but makes things really slow
			//grid2->lodFixed = 1;
		}
	}
}

/*
=================
R_FixSharedVertexLodError

This function assumes that all patches in one group are nicely stitched together for the highest LoD.
If this is not the case this function will still do its job but won't fix the highest LoD cracks.
=================
*/
static void R_FixSharedVertexLodError( world_t *worldData ) {
	int i;
	srfBspSurface_t *grid1;

	for ( i = 0; i < worldData->numsurfaces; i++ ) {
		//
		grid1 = (srfBspSurface_t *) worldData->surfaces[i].data;
		// if this surface is not a grid
		if ( grid1->surfaceType != SF_GRID )
			continue;
		//
		if ( grid1->lodFixed )
			continue;
		//
		grid1->lodFixed = 2;
		// recursively fix other patches in the same LOD group
		R_FixSharedVertexLodError_r( worldData, i + 1, grid1);
	}
}


/*
===============
R_StitchPatches
===============
*/
static int R_StitchPatches( world_t *worldData, int grid1num, int grid2num ) {
	float *v1, *v2;
	srfBspSurface_t *grid1, *grid2;
	int k, l, m, n, offset1, offset2, row, column;

	grid1 = (srfBspSurface_t *) worldData->surfaces[grid1num].data;
	grid2 = (srfBspSurface_t *) worldData->surfaces[grid2num].data;
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height-1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->width-2; k += 2) {

			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
									grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData->surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
					//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData->surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width-1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->height-2; k += 2) {
			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
									grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData->surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
									grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData->surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height-1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = grid1->width-1; k > 1; k -= 2) {

			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
										grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData->surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k+1]);
					if (!grid2)
						break;
					grid2->lodStitched = qfalse;
					worldData->surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width-1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = grid1->height-1; k > 1; k -= 2) {
			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
										grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData->surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					worldData->surfaces[grid2num].data = (surfaceType_t *) grid2;
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
===============
R_TryStitchPatch

This function will try to stitch patches in the same LoD group together for the highest LoD.

Only single missing vertice cracks will be fixed.

Vertices will be joined at the patch side a crack is first found, at the other side
of the patch (on the same row or column) the vertices will not be joined and cracks
might still appear at that side.
===============
*/
static int R_TryStitchingPatch( world_t *worldData, int grid1num ) {
	int j, numstitches;
	srfBspSurface_t *grid1, *grid2;

	numstitches = 0;
	grid1 = (srfBspSurface_t *) worldData->surfaces[grid1num].data;
	for ( j = 0; j < worldData->numsurfaces; j++ ) {
		//
		grid2 = (srfBspSurface_t *) worldData->surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		while (R_StitchPatches(worldData, grid1num, j))
		{
			numstitches++;
		}
	}
	return numstitches;
}

/*
===============
R_StitchAllPatches
===============
*/
static void R_StitchAllPatches( world_t *worldData ) {
	int i, stitched, numstitches;
	srfBspSurface_t *grid1;

	numstitches = 0;
	do
	{
		stitched = qfalse;
		for ( i = 0; i < worldData->numsurfaces; i++ ) {
			//
			grid1 = (srfBspSurface_t *) worldData->surfaces[i].data;
			// if this surface is not a grid
			if ( grid1->surfaceType != SF_GRID )
				continue;
			//
			if ( grid1->lodStitched )
				continue;
			//
			grid1->lodStitched = qtrue;
			stitched = qtrue;
			//
			numstitches += R_TryStitchingPatch( worldData, i );
		}
	}
	while (stitched);
	ri.Printf( PRINT_ALL, "stitched %d LoD cracks\n", numstitches );
}

/*
===============
R_MovePatchSurfacesToHunk
===============
*/
static void R_MovePatchSurfacesToHunk( world_t *worldData ) {
	int i, size;
	srfBspSurface_t *grid, *hunkgrid;

	for ( i = 0; i < worldData->numsurfaces; i++ ) {
		//
		grid = (srfBspSurface_t *) worldData->surfaces[i].data;
		// if this surface is not a grid
		if ( grid->surfaceType != SF_GRID )
			continue;
		//
		size = sizeof(*grid);
		hunkgrid = (srfBspSurface_t *)Hunk_Alloc(size, h_low);
		Com_Memcpy(hunkgrid, grid, size);

		hunkgrid->widthLodError = (float *)Hunk_Alloc( grid->width * 4, h_low );
		Com_Memcpy( hunkgrid->widthLodError, grid->widthLodError, grid->width * 4 );

		hunkgrid->heightLodError = (float *)Hunk_Alloc( grid->height * 4, h_low );
		Com_Memcpy( hunkgrid->heightLodError, grid->heightLodError, grid->height * 4 );

		hunkgrid->numIndexes = grid->numIndexes;
		hunkgrid->indexes = (glIndex_t *)Hunk_Alloc(grid->numIndexes * sizeof(glIndex_t), h_low);
		Com_Memcpy(hunkgrid->indexes, grid->indexes, grid->numIndexes * sizeof(glIndex_t));

		hunkgrid->numVerts = grid->numVerts;
		hunkgrid->verts = (srfVert_t *)Hunk_Alloc(grid->numVerts * sizeof(srfVert_t), h_low);
		Com_Memcpy(hunkgrid->verts, grid->verts, grid->numVerts * sizeof(srfVert_t));

		R_FreeSurfaceGridMesh( grid );

		worldData->surfaces[i].data = (surfaceType_t *) hunkgrid;
	}
}

/*
===============
R_LoadSurfaces
===============
*/
static	void R_LoadSurfacesIBSP( world_t *worldData, lump_t *surfs, lump_t *verts, lump_t *indexLump ) {
	dsurfaceIBSP_t*in;
	msurface_t	*out;
	drawVertIBSP_t*dv;
	int			*indexes;
	int			count;
	int			numFaces, numMeshes, numTriSurfs, numFlares;
	int			i;
	float *hdrVertColors = NULL;

	numFaces = 0;
	numMeshes = 0;
	numTriSurfs = 0;
	numFlares = 0;

	if (surfs->filelen % sizeof(*in))
		ri.Error (ERR_DROP, "R_LoadSurfacesIBSP: funny in lump size in %s",worldData->name);
	count = surfs->filelen / sizeof(*in);

	dv = (drawVertIBSP_t*)(fileBaseIBSP + verts->fileofs);
	if (verts->filelen % sizeof(*dv))
		ri.Error (ERR_DROP, "R_LoadSurfacesIBSP: funny dv lump size in %s",worldData->name);

	indexes = (int *)(fileBaseIBSP + indexLump->fileofs);
	if ( indexLump->filelen % sizeof(*indexes))
		ri.Error (ERR_DROP, "R_LoadSurfacesIBSP: funny indexes lump size in %s",worldData->name);

	out = (msurface_t *)Hunk_Alloc ( count * sizeof(*out), h_low );

	worldData->surfaces = out;
	worldData->numsurfaces = count;
	worldData->surfacesViewCount = (int *)Hunk_Alloc ( count * sizeof(*worldData->surfacesViewCount), h_low );
	worldData->surfacesDlightBits = (int *)Hunk_Alloc ( count * sizeof(*worldData->surfacesDlightBits), h_low );
	worldData->surfacesPshadowBits = (int *)Hunk_Alloc ( count * sizeof(*worldData->surfacesPshadowBits), h_low );

	// load hdr vertex colors
	if (r_hdr->integer)
	{
		char filename[MAX_QPATH];
		int size;

		Com_sprintf( filename, sizeof( filename ), "maps/%s/vertlight.raw", worldData->baseName);
		//ri.Printf(PRINT_ALL, "looking for %s\n", filename);

		size = ri.FS_ReadFile(filename, (void **)&hdrVertColors);

		if (hdrVertColors)
		{
			//ri.Printf(PRINT_ALL, "Found!\n");
			if ((unsigned)size != sizeof(float) * 3 * verts->filelen / sizeof(*dv))
				ri.Error(ERR_DROP, "Bad size for %s (%i, expected %i)!", filename, size, (int)((sizeof(float)) * 3 * verts->filelen / sizeof(*dv)));
		}
	}

	// load vertex tangent space
	packedTangentSpace_t *tangentSpace = NULL;
	char filename[MAX_QPATH];
	Com_sprintf(filename, sizeof(filename), "maps/%s.tspace", worldData->baseName);
	int size = ri.FS_ReadFile(filename, (void **)&tangentSpace);

	if (tangentSpace)
	{
		assert((size_t)size == (verts->filelen / sizeof(*dv)) * sizeof(float) * 4);

		if ((unsigned)size != sizeof(tangentSpace[0]) * verts->filelen / sizeof(*dv))
			ri.Error(ERR_DROP, "Bad size for %s (%i, expected %i)!", filename, size, (int)(sizeof(float) * 4 * verts->filelen / sizeof(*dv)));
	}


	// Two passes, allocate surfaces first, then load them full of data
	// This ensures surfaces are close together to reduce L2 cache misses when using VBOs,
	// which don't actually use the verts and indexes
	in = (dsurfaceIBSP_t*)(fileBaseIBSP + surfs->fileofs);
	out = worldData->surfaces;
	for ( i = 0 ; i < count ; i++, in++, out++ ) {
		switch ( LittleLong( in->surfaceType ) ) {
			case MST_PATCH:
				// FIXME: do this
				break;
			case MST_TRIANGLE_SOUP:
				out->data = (surfaceType_t *)Hunk_Alloc( sizeof(srfBspSurface_t), h_low);
				break;
			case MST_PLANAR:
				out->data = (surfaceType_t *)Hunk_Alloc( sizeof(srfBspSurface_t), h_low);
				break;
			case MST_FLARE:
				out->data = (surfaceType_t *)Hunk_Alloc( sizeof(srfFlare_t), h_low);
				break;
			default:
				break;
		}
	}

	in = (dsurfaceIBSP_t*)(fileBaseIBSP + surfs->fileofs);
	out = worldData->surfaces;
	for ( i = 0 ; i < count ; i++, in++, out++ ) {
		switch ( LittleLong( in->surfaceType ) ) {
		case MST_PATCH:
			ParseMesh ( worldData, in, dv, tangentSpace, hdrVertColors, out );
			{
				srfBspSurface_t *surface = (srfBspSurface_t *)out->data;

				out->cullinfo.type = CULLINFO_BOX | CULLINFO_SPHERE;
				VectorCopy(surface->cullBounds[0], out->cullinfo.bounds[0]);
				VectorCopy(surface->cullBounds[1], out->cullinfo.bounds[1]);
				VectorCopy(surface->cullOrigin, out->cullinfo.localOrigin);
				out->cullinfo.radius = surface->cullRadius;
			}
			numMeshes++;
			break;
		case MST_TRIANGLE_SOUP:
			ParseTriSurf( worldData, in, dv, tangentSpace, hdrVertColors, out, indexes );
			numTriSurfs++;
			break;
		case MST_PLANAR:
			ParseFace( worldData, in, dv, tangentSpace, hdrVertColors, out, indexes );
			numFaces++;
			break;
		case MST_FLARE:
			ParseFlare( worldData, in, dv, out, indexes );
			{
				out->cullinfo.type = CULLINFO_NONE;
			}
			numFlares++;
			break;
		default:
			ri.Error( ERR_DROP, "Bad surfaceType" );
		}
	}

	if (hdrVertColors)
	{
		ri.FS_FreeFile(hdrVertColors);
	}

	if ( r_patchStitching->integer ) {
		R_StitchAllPatches(worldData);
	}

	R_FixSharedVertexLodError(worldData);

	if ( r_patchStitching->integer ) {
		R_MovePatchSurfacesToHunk(worldData);
	}

	ri.Printf( PRINT_ALL, "...loaded %d faces, %i meshes, %i trisurfs, %i flares\n",
		numFaces, numMeshes, numTriSurfs, numFlares );
}

/*
=================
R_LoadShaders
=================
*/
static	void R_LoadShadersIBSP(byte* fileBase, world_t *worldData, lump_t *l ) {
	int		i, count;
	dshader_t	*in, *out;

	fileBaseIBSP = fileBase;

	in = (dshader_t *)(fileBaseIBSP + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Error (ERR_DROP, "R_LoadShadersIBSP: funny lump size in %s",worldData->name);
	count = l->filelen / sizeof(*in);
	out = (dshader_t *)Hunk_Alloc ( count*sizeof(*out), h_low );

	worldData->shaders = out;
	worldData->numShaders = count;

	Com_Memcpy( out, in, count*sizeof(*out) );

	for ( i=0 ; i<count ; i++ ) {
		// TODO: Check surface and content flag mismatches between ibsp and rbsp and correct here!
		out[i].surfaceFlags = LittleLong( out[i].surfaceFlags );
		out[i].contentFlags = LittleLong( out[i].contentFlags );
	}
}

typedef struct {
	int			planeNum;			// positive plane side faces out of the leaf
	int			shaderNum;
} dbrushsideIBSP_t;

/*
=================
R_LoadFogs

=================
*/
static void R_LoadFogsIBSP( world_t *worldData, lump_t *l, lump_t *brushesLump, lump_t *sidesLump ) {
	int			i;
	fog_t		*out;
	dfog_t		*fogs;
	dbrush_t 	*brushes, *brush;
	dbrushsideIBSP_t *sides;
	int			count, brushesCount, sidesCount;
	int			sideNum;
	int			planeNum;
	shader_t	*shader;
	float		d;
	int			firstSide;

	fogs = (dfog_t *)(fileBaseIBSP + l->fileofs);
	if (l->filelen % sizeof(*fogs)) {
		ri.Error (ERR_DROP, "R_LoadFogsIBSP: funny fog lump size in %s",worldData->name);
	}
	count = l->filelen / sizeof(*fogs);

	// create fog strucutres for them
	worldData->numfogs = count + 1;
	worldData->fogs = (fog_t *)Hunk_Alloc ( worldData->numfogs*sizeof(*out), h_low);
	worldData->globalFog = nullptr;
	worldData->globalFogIndex = -1;
	out = worldData->fogs + 1;

	if ( !count ) {
		return;
	}

	brushes = (dbrush_t *)(fileBaseIBSP + brushesLump->fileofs);
	if (brushesLump->filelen % sizeof(*brushes)) {
		ri.Error (ERR_DROP, "R_LoadFogsIBSP: funny brush lump size in %s",worldData->name);
	}
	brushesCount = brushesLump->filelen / sizeof(*brushes);

	sides = (dbrushsideIBSP_t*)(fileBaseIBSP + sidesLump->fileofs);
	if (sidesLump->filelen % sizeof(*sides)) {
		ri.Error (ERR_DROP, "R_LoadFogsIBSP: funny sides lump size in %s",worldData->name);
	}
	sidesCount = sidesLump->filelen / sizeof(*sides);

	for ( i=0 ; i<count ; i++, fogs++) {
		out->originalBrushNumber = LittleLong( fogs->brushNum );

		if ( out->originalBrushNumber == -1 )
		{
			out->bounds[0][0] = out->bounds[0][1] = out->bounds[0][2] = MIN_WORLD_COORD;
			out->bounds[1][0] = out->bounds[1][1] = out->bounds[1][2] = MAX_WORLD_COORD;
			firstSide = -1;

			worldData->globalFog = worldData->fogs + i + 1;
			worldData->globalFogIndex = i + 1;
		}
		else
		{
			if ( (unsigned)out->originalBrushNumber >= (unsigned)brushesCount ) {
				ri.Error( ERR_DROP, "fog brushNumber out of range" );
			}
			brush = brushes + out->originalBrushNumber;

			firstSide = LittleLong( brush->firstSide );

				if ( (unsigned)firstSide > (unsigned)sidesCount - 6 ) {
				ri.Error( ERR_DROP, "fog brush sideNumber out of range" );
			}

			// brushes are always sorted with the axial sides first
			sideNum = firstSide + 0;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[0][0] = -worldData->planes[ planeNum ].dist;

			sideNum = firstSide + 1;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[1][0] = worldData->planes[ planeNum ].dist;

			sideNum = firstSide + 2;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[0][1] = -worldData->planes[ planeNum ].dist;

			sideNum = firstSide + 3;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[1][1] = worldData->planes[ planeNum ].dist;

			sideNum = firstSide + 4;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[0][2] = -worldData->planes[ planeNum ].dist;

			sideNum = firstSide + 5;
			planeNum = LittleLong( sides[ sideNum ].planeNum );
			out->bounds[1][2] = worldData->planes[ planeNum ].dist;
		}

		// get information from the shader for fog parameters
		shader = R_FindShader( fogs->shader, lightmapsNone, stylesDefault, qtrue );

		if (shader->defaultShader == qtrue || shader->fogParms.depthForOpaque == 0)
		{
			ri.Printf(PRINT_WARNING, "Couldn't find proper shader for bsp fog %i %s\n", i, fogs->shader);
			shader->fogParms.depthForOpaque = 65535.f;
		}

		out->parms = shader->fogParms;

		VectorSet4(out->color,
			shader->fogParms.color[0] * tr.identityLight,
			shader->fogParms.color[1] * tr.identityLight,
			shader->fogParms.color[2] * tr.identityLight,
			1.0);

		d = shader->fogParms.depthForOpaque < 1 ? 1 : shader->fogParms.depthForOpaque;
		out->tcScale = 1.0f / d;

		// set the gradient vector
		sideNum = LittleLong( fogs->visibleSide );

		out->hasSurface = (out->originalBrushNumber == -1) ? qfalse : qtrue;
		if ( sideNum != -1 ) {
			planeNum = LittleLong( sides[ firstSide + sideNum ].planeNum );
			VectorSubtract( vec3_origin, worldData->planes[ planeNum ].normal, out->surface );
			out->surface[3] = -worldData->planes[ planeNum ].dist;
		}

		out++;
	}

}

typedef struct
{
	byte		ambientLight[3];
	byte		directLight[3];
	byte		latLong[2];
} mgridIBSP_t;

/*
================
R_LoadLightGrid

================
*/
static void R_LoadLightGridIBSP( world_t *worldData, lump_t *l ) {
	int		i;
	vec3_t	maxs;
	float	*wMins, *wMaxs;

	worldData->lightGridInverseSize[0] = 1.0f / worldData->lightGridSize[0];
	worldData->lightGridInverseSize[1] = 1.0f / worldData->lightGridSize[1];
	worldData->lightGridInverseSize[2] = 1.0f / worldData->lightGridSize[2];

	wMins = worldData->bmodels[0].bounds[0];
	wMaxs = worldData->bmodels[0].bounds[1];

	for ( i = 0 ; i < 3 ; i++ ) {
		worldData->lightGridOrigin[i] = worldData->lightGridSize[i] * ceil( wMins[i] / worldData->lightGridSize[i] );
		maxs[i] = worldData->lightGridSize[i] * floor( wMaxs[i] / worldData->lightGridSize[i] );
		worldData->lightGridBounds[i] = (maxs[i] - worldData->lightGridOrigin[i])/worldData->lightGridSize[i] + 1;
	}

	int numGridDataElements = l->filelen / sizeof(*worldData->lightGridData);

	worldData->lightGridData = (mgrid_t*)Hunk_Alloc( l->filelen, h_low );
	memset(worldData->lightGridData, 0, l->filelen);
	mgridIBSP_t* ibspGrid = (mgridIBSP_t*)(fileBaseIBSP + l->fileofs);

	// deal with overbright bits
	for ( i = 0 ; i < numGridDataElements ; i++ )
	{
		memcpy(worldData->lightGridData[i].ambientLight[0], ibspGrid[i].ambientLight, *ibspGrid[i].ambientLight);
		memcpy(worldData->lightGridData[i].directLight[0], ibspGrid[i].directLight, *ibspGrid[i].directLight);
		memcpy(worldData->lightGridData[i].latLong, ibspGrid[i].latLong, *ibspGrid[i].latLong);
		worldData->lightGridData[i].styles[0] = 0x00;
		worldData->lightGridData[i].styles[1] = 0xff;
		worldData->lightGridData[i].styles[2] = 0xff;
		worldData->lightGridData[i].styles[3] = 0xff;

		R_ColorShiftLightingBytes(
			worldData->lightGridData[i].ambientLight[0],
			worldData->lightGridData[i].ambientLight[0]);
		R_ColorShiftLightingBytes(
			worldData->lightGridData[i].directLight[0],
			worldData->lightGridData[i].directLight[0]);
	}

	// load hdr lightgrid
	if (r_hdr->integer)
	{
		char filename[MAX_QPATH];
		float *hdrLightGrid;
		int size;

		Com_sprintf( filename, sizeof( filename ), "maps/%s/lightgrid.raw", worldData->baseName);
		//ri.Printf(PRINT_ALL, "looking for %s\n", filename);

		size = ri.FS_ReadFile(filename, (void **)&hdrLightGrid);

		if (hdrLightGrid)
		{
			if ((unsigned)size != sizeof(float) * 6 * worldData->lightGridBounds[0] * worldData->lightGridBounds[1] * worldData->lightGridBounds[2])
			{
				ri.Error(ERR_DROP, "Bad size for %s (%i, expected %i)!", filename, size, (int)(sizeof(float)) * 6 * worldData->lightGridBounds[0] * worldData->lightGridBounds[1] * worldData->lightGridBounds[2]);
			}

			worldData->hdrLightGrid = (float *)Hunk_Alloc(size, h_low);

			for (i = 0; i < worldData->lightGridBounds[0] * worldData->lightGridBounds[1] * worldData->lightGridBounds[2]; i++)
			{
				worldData->hdrLightGrid[i * 6    ] = hdrLightGrid[i * 6    ];
				worldData->hdrLightGrid[i * 6 + 1] = hdrLightGrid[i * 6 + 1];
				worldData->hdrLightGrid[i * 6 + 2] = hdrLightGrid[i * 6 + 2];
				worldData->hdrLightGrid[i * 6 + 3] = hdrLightGrid[i * 6 + 3];
				worldData->hdrLightGrid[i * 6 + 4] = hdrLightGrid[i * 6 + 4];
				worldData->hdrLightGrid[i * 6 + 5] = hdrLightGrid[i * 6 + 5];
			}
		}

		if (hdrLightGrid)
			ri.FS_FreeFile(hdrLightGrid);
	}
}


/*
================
R_LoadLightGridArray

================
*/
static void R_LoadLightGridArrayIBSP(world_t* worldData) {
	worldData->numGridArrayElements = worldData->lightGridBounds[0] * worldData->lightGridBounds[1] * worldData->lightGridBounds[2];
	worldData->lightGridData = NULL;
	//worldData->lightGridArray = (unsigned short*)Hunk_Alloc(l->filelen, h_low);
}


static void R_BuildLightGridTextureIBSP(world_t *world)
{
	if (!r_volumetricFog->integer)
	{
		return;
	}

	// Upload light grid as a 3D texture
	// For volumetric fog, we don't need directionality, so just merge ambient and direct contributions
	// I tried using the directionality with phase function and it looked bad. Created like visable noodles in the air.
	// Potentiall add the seperated 3d images for other things, but currently there's no need.
	byte *lightBase = NULL;
	uint16_t *lightHDRBase = NULL;
	if (world->hdrLightGrid)
	{
		lightHDRBase = (uint16_t *)Z_Malloc(world->numGridArrayElements * sizeof(uint16_t) * 4, TAG_TEMP_WORKSPACE, qtrue);
	}
	else
	{
		lightBase = (byte *)Z_Malloc(world->numGridArrayElements * sizeof(byte) * 4, TAG_TEMP_WORKSPACE, qtrue);
	}

	if (world->lightGridData)
	{
		uint16_t *lightHDR = NULL;
		byte *light = NULL;
		if (world->hdrLightGrid)
		{
			lightHDR = lightHDRBase;
		}
		else
		{
			light = lightBase;
		}

		for (int i = 0; i < world->numGridArrayElements; i++)
		{
			if (world->hdrLightGrid)
			{
				float *hdrData = world->hdrLightGrid + (i * 6);

				lightHDR[0] = FloatToHalf(hdrData[0] + hdrData[3]);
				lightHDR[1] = FloatToHalf(hdrData[1] + hdrData[4]);
				lightHDR[2] = FloatToHalf(hdrData[2] + hdrData[5]);
				lightHDR[3] = FloatToHalf(1.0f);

				lightHDR += 4;
			}
			else
			{
				mgrid_t *data = world->lightGridData + world->lightGridArray[i];

				light[0] = MAX(data->ambientLight[0][0], data->directLight[0][0]);
				light[1] = MAX(data->ambientLight[0][1], data->directLight[0][1]);
				light[2] = MAX(data->ambientLight[0][2], data->directLight[0][2]);
				light[3] = 255;

				light += 4;
			}
		}

		if (world->hdrLightGrid)
		{
			world->volumetricLightMaps[0] = R_CreateImage3D(
				"*volumetricLightmap0", (byte*)lightHDRBase,
				world->lightGridBounds[0],
				world->lightGridBounds[1],
				world->lightGridBounds[2],
				GL_RGB16F);
		}
		else
		{
			world->volumetricLightMaps[0] = R_CreateImage3D(
				"*volumetricLightmap0", lightBase,
				world->lightGridBounds[0],
				world->lightGridBounds[1],
				world->lightGridBounds[2],
				GL_RGB8);
		}
	}
	else
	{
		world->volumetricLightMaps[0] = NULL;
	}

	if (world->hdrLightGrid)
	{
		Z_Free(lightHDRBase);
	}
	else
	{
		Z_Free(lightBase);
	}

	return;
}
