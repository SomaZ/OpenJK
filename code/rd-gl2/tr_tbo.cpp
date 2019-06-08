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
along with OpenDF2a source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_tbo.cpp
#include "tr_local.h"
#include "glext.h"

void RB_CreateTBO(const char *name, int *data, int size, int format)
{
	GLuint tbo;
	qglGenBuffers(1, &tbo);
	qglBindBuffer(GL_TEXTURE_BUFFER, tbo);

	qglBufferData(GL_TEXTURE_BUFFER, sizeof(int) * size, data, GL_STATIC_DRAW);

	GLuint buffer_texture;
	qglGenTextures(1, &buffer_texture);
	qglBindTexture(GL_TEXTURE_BUFFER, buffer_texture);

	qglTexBuffer(GL_TEXTURE_BUFFER, format, tbo);

	qglBindTexture(GL_TEXTURE_BUFFER, 0);
}

void RB_DeleteTBO(GLuint *tbo, GLuint *buffer_texture)
{
	qglBindBuffer(1, *tbo);
	qglDeleteBuffers(1, tbo);
	qglDeleteTextures(1, buffer_texture);
}

void RB_SetTBOData(GLuint *tbo, int *data, int size)
{
	qglBindBuffer(GL_TEXTURE_BUFFER, *tbo);
	int *buffer = (int*) qglMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
	for (int i = 0; i < size; i++)
	{
		buffer[i] = data[i];
	}
	qglUnmapBuffer(GL_TEXTURE_BUFFER);
	qglBindBuffer(GL_TEXTURE_BUFFER, 0);
}