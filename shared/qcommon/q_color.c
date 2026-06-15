#include "q_color.h"

vec4_t		colorBlack	= {0, 0, 0, 1};
vec4_t		colorRed	= {1, 0, 0, 1};
vec4_t		colorGreen	= {0, 1, 0, 1};
vec4_t		colorBlue	= {0, 0, 1, 1};
vec4_t		colorYellow	= {1, 1, 0, 1};
vec4_t		colorOrange = {1, 0.5, 0, 1};
vec4_t		colorMagenta= {1, 0, 1, 1};
vec4_t		colorCyan	= {0, 1, 1, 1};
vec4_t		colorWhite	= {1, 1, 1, 1};
vec4_t		colorLtGrey	= {0.75, 0.75, 0.75, 1};
vec4_t		colorMdGrey	= {0.5, 0.5, 0.5, 1};
vec4_t		colorDkGrey	= {0.25, 0.25, 0.25, 1};

vec4_t		colorLtBlue	= {0.367f, 0.261f, 0.722f, 1};
vec4_t		colorDkBlue	= {0.199f, 0.0f,   0.398f, 1};

vec4_t g_color_table[Q_COLOR_BITS+1] = {
	{ 0.0, 0.0, 0.0, 1.0 },	// black
	{ 1.0, 0.0, 0.0, 1.0 },	// red
	{ 0.0, 1.0, 0.0, 1.0 },	// green
	{ 1.0, 1.0, 0.0, 1.0 },	// yellow
	{ 0.0, 0.0, 1.0, 1.0 },	// blue
	{ 0.0, 1.0, 1.0, 1.0 },	// cyan
	{ 1.0, 0.0, 1.0, 1.0 },	// magenta
	{ 1.0, 1.0, 1.0, 1.0 },	// white
	{ 1.0, 0.5, 0.0, 1.0 }, // orange
	{ 0.5, 0.5, 0.5, 1.0 },	// md.grey
};

vec4_t colorTable[CT_MAX] =
{
	{0, 0, 0, 0},			// CT_NONE
	{0, 0, 0, 1},			// CT_BLACK
	{1, 0, 0, 1},			// CT_RED
	{0, 1, 0, 1},			// CT_GREEN
	{0, 0, 1, 1},			// CT_BLUE
	{1, 1, 0, 1},			// CT_YELLOW
	{1, 0, 1, 1},			// CT_MAGENTA
	{0, 1, 1, 1},			// CT_CYAN
	{1, 1, 1, 1},			// CT_WHITE
	{0.75f, 0.75f, 0.75f, 1},	// CT_LTGREY
	{0.50f, 0.50f, 0.50f, 1},	// CT_MDGREY
	{0.25f, 0.25f, 0.25f, 1},	// CT_DKGREY
	{0.15f, 0.15f, 0.15f, 1},	// CT_DKGREY2

	{0.992f, 0.652f, 0.0f,  1},	// CT_VLTORANGE -- needs values
	{0.810f, 0.530f, 0.0f,  1},	// CT_LTORANGE
	{0.610f, 0.330f, 0.0f,  1},	// CT_DKORANGE
	{0.402f, 0.265f, 0.0f,  1},	// CT_VDKORANGE

	{0.503f, 0.375f, 0.996f, 1},	// CT_VLTBLUE1
	{0.367f, 0.261f, 0.722f, 1},	// CT_LTBLUE1
	{0.199f, 0.0f,   0.398f, 1},	// CT_DKBLUE1
	{0.160f, 0.117f, 0.324f, 1},	// CT_VDKBLUE1

	{0.300f, 0.628f, 0.816f, 1},	// CT_VLTBLUE2 -- needs values
	{0.300f, 0.628f, 0.816f, 1},	// CT_LTBLUE2
	{0.191f, 0.289f, 0.457f, 1},	// CT_DKBLUE2
	{0.125f, 0.250f, 0.324f, 1},	// CT_VDKBLUE2

	{0.796f, 0.398f, 0.199f, 1},	// CT_VLTBROWN1 -- needs values
	{0.796f, 0.398f, 0.199f, 1},	// CT_LTBROWN1
	{0.558f, 0.207f, 0.027f, 1},	// CT_DKBROWN1
	{0.328f, 0.125f, 0.035f, 1},	// CT_VDKBROWN1

	{0.996f, 0.796f, 0.398f, 1},	// CT_VLTGOLD1 -- needs values
	{0.996f, 0.796f, 0.398f, 1},	// CT_LTGOLD1
	{0.605f, 0.441f, 0.113f, 1},	// CT_DKGOLD1
	{0.386f, 0.308f, 0.148f, 1},	// CT_VDKGOLD1

	{0.648f, 0.562f, 0.784f, 1},	// CT_VLTPURPLE1 -- needs values
	{0.648f, 0.562f, 0.784f, 1},	// CT_LTPURPLE1
	{0.437f, 0.335f, 0.597f, 1},	// CT_DKPURPLE1
	{0.308f, 0.269f, 0.375f, 1},	// CT_VDKPURPLE1

	{0.816f, 0.531f, 0.710f, 1},	// CT_VLTPURPLE2 -- needs values
	{0.816f, 0.531f, 0.710f, 1},	// CT_LTPURPLE2
	{0.566f, 0.269f, 0.457f, 1},	// CT_DKPURPLE2
	{0.343f, 0.226f, 0.316f, 1},	// CT_VDKPURPLE2

	{0.929f, 0.597f, 0.929f, 1},	// CT_VLTPURPLE3
	{0.570f, 0.371f, 0.570f, 1},	// CT_LTPURPLE3
	{0.355f, 0.199f, 0.355f, 1},	// CT_DKPURPLE3
	{0.285f, 0.136f, 0.230f, 1},	// CT_VDKPURPLE3

	{0.953f, 0.378f, 0.250f, 1},	// CT_VLTRED1
	{0.953f, 0.378f, 0.250f, 1},	// CT_LTRED1
	{0.593f, 0.121f, 0.109f, 1},	// CT_DKRED1
	{0.429f, 0.171f, 0.113f, 1},	// CT_VDKRED1
	{.25f, 0, 0, 1},				// CT_VDKRED
	{.70f, 0, 0, 1},				// CT_DKRED

	{0.717f, 0.902f, 1.0f,   1},	// CT_VLTAQUA
	{0.574f, 0.722f, 0.804f, 1},	// CT_LTAQUA
	{0.287f, 0.361f, 0.402f, 1},	// CT_DKAQUA
	{0.143f, 0.180f, 0.201f, 1},	// CT_VDKAQUA

	{0.871f, 0.386f, 0.375f, 1},	// CT_LTPINK
	{0.435f, 0.193f, 0.187f, 1},	// CT_DKPINK
	{	  0,    .5f,    .5f, 1},	// CT_LTCYAN
	{	  0,   .25f,   .25f, 1},	// CT_DKCYAN
	{   .179f, .51f,   .92f, 1},	// CT_LTBLUE3
	{   .199f, .71f,   .92f, 1},	// CT_LTBLUE3
	{   .5f,   .05f,    .4f, 1},	// CT_DKBLUE3

	{   0.0f,   .613f,  .097f, 1},	// CT_HUD_GREEN
	{   0.835f, .015f,  .015f, 1},	// CT_HUD_RED
	{	.567f,	.685f,	1.0f,	.75f},	// CT_ICON_BLUE
	{	.515f,	.406f,	.507f,	1},	// CT_NO_AMMO_RED

	{   1.0f,   .658f,  .062f, 1},	// CT_HUD_ORANGE
	{	0.549f, .854f,  1.0f,  1.0f},	//	CT_TITLE
};

unsigned ColorBytes3 (float r, float g, float b) {
	unsigned i;

	( (byte *)&i )[0] = (byte)(r * 255);
	( (byte *)&i )[1] = (byte)(g * 255);
	( (byte *)&i )[2] = (byte)(b * 255);

	return i;
}

unsigned ColorBytes4 (float r, float g, float b, float a) {
	unsigned i;

	( (byte *)&i )[0] = (byte)(r * 255);
	( (byte *)&i )[1] = (byte)(g * 255);
	( (byte *)&i )[2] = (byte)(b * 255);
	( (byte *)&i )[3] = (byte)(a * 255);

	return i;
}

float NormalizeColor( const vec3_t in, vec3_t out ) {
	float	max;

	max = in[0];
	if ( in[1] > max ) {
		max = in[1];
	}
	if ( in[2] > max ) {
		max = in[2];
	}

	if ( !max ) {
		VectorClear( out );
	} else {
		out[0] = in[0] / max;
		out[1] = in[1] / max;
		out[2] = in[2] / max;
	}
	return max;
}

vec3_t defaultColors[10] =
{
	{0.0f, 0.0f, 0.0f},
	{1.0f, 0.0f, 0.0f},
	{0.0f, 1.0f, 0.0f},
	{1.0f, 1.0f, 0.0f},
	{0.0f, 0.0f, 1.0f},
	{0.0f, 1.0f, 1.0f},
	{1.0f, 0.0f, 1.0f},
	{1.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f},
	{1.0f, 0.0f, 0.0f},
};
vec3_t uagColors[43] =
{
	{ 0.000f, 0.000f, 0.000f }, // '0' 0x30 48:   0   0   0
	{ 1.000f, 0.000f, 0.000f }, // '1' 0x31 49: 255   0   0
	{ 0.000f, 1.000f, 0.000f }, // '2' 0x32 50:   0 255   0
	{ 1.000f, 1.000f, 0.000f }, // '3' 0x33 51: 255 255   0
	{ 0.000f, 0.000f, 1.000f }, // '4' 0x34 52:   0   0 255
	{ 0.000f, 1.000f, 1.000f }, // '5' 0x35 53:   0 255 255
	{ 1.000f, 0.000f, 1.000f }, // '6' 0x36 54: 255   0 255
	{ 1.000f, 1.000f, 1.000f }, // '7' 0x37 55: 255 255 255
// '8'-'@' will be skipped, but '8' and '9' will appear in the console as black and red (as in original jamp)
	{ 0.000f, 0.000f, 0.000f }, // '8' 0x38 56:   0   0   0
	{ 1.000f, 0.000f, 0.000f }, // '9' 0x39 57: 255   0   0
	{ 1.000f, 1.000f, 1.000f }, // ':' 0x3A 58: 255 255 255
	{ 1.000f, 1.000f, 1.000f }, // ';' 0x3B 59: 255 255 255
	{ 1.000f, 1.000f, 1.000f }, // '<' 0x3C 60: 255 255 255
	{ 1.000f, 1.000f, 1.000f }, // '=' 0x3D 61: 255 255 255
	{ 1.000f, 1.000f, 1.000f }, // '>' 0x3E 62: 255 255 255
	{ 1.000f, 1.000f, 1.000f }, // '?' 0x3F 63: 255 255 255
	{ 1.000f, 1.000f, 1.000f }, // '@' 0x40 64: 255 255 255
// end of skip
	{ 1.000f, 0.561f, 0.561f }, // 'A' 0x41 65: 255 143 143
	{ 0.500f, 0.039f, 0.039f }, // 'B' 0x42 66: 127  10  10
	{ 0.500f, 0.302f, 0.027f }, // 'C' 0x43 67: 127  77   7
	{ 1.000f, 0.584f, 0.000f }, // 'D' 0x44 68: 255 149   0
	{ 0.843f, 0.757f, 0.545f }, // 'E' 0x45 69: 215 193 139
	{ 1.000f, 1.000f, 0.612f }, // 'F' 0x46 70: 255 255 156
	{ 0.647f, 1.000f, 0.655f }, // 'G' 0x47 71: 165 255 167
	{ 0.541f, 0.863f, 0.035f }, // 'H' 0x48 72: 138 220   9
	{ 0.004f, 0.475f, 0.004f }, // 'I' 0x49 73:   1 121   1
	{ 0.439f, 0.447f, 0.176f }, // 'J' 0x4A 74: 112 114  45
	{ 0.388f, 0.490f, 0.321f }, // 'K' 0x4B 75:  99 125  82
	{ 0.000f, 0.902f, 0.659f }, // 'L' 0x4C 76:   0 230 168
	{ 0.612f, 0.996f, 1.000f }, // 'M' 0x4D 77: 156 254 255
	{ 0.000f, 0.580f, 0.580f }, // 'N' 0x4E 78:   0 148 148
	{ 0.321f, 0.482f, 0.510f }, // 'O' 0x4F 79:  82 123 130
	{ 0.000f, 0.667f, 1.000f }, // 'P' 0x50 80:   0 170 255
	{ 0.494f, 0.500f, 1.000f }, // 'Q' 0x51 81: 126 127 255
	{ 0.306f, 0.306f, 0.573f }, // 'R' 0x52 82:  78  78 146
	{ 0.518f, 0.000f, 1.000f }, // 'S' 0x53 83: 132   0 255
	{ 0.769f, 0.518f, 1.000f }, // 'T' 0x54 84: 196 132 255
	{ 0.510f, 0.329f, 0.506f }, // 'U' 0x55 85: 130  84 129
	{ 0.988f, 0.608f, 0.937f }, // 'V' 0x56 86: 252 155 239
	{ 1.000f, 0.000f, 0.500f }, // 'W' 0x57 87: 255   0 127
	{ 0.596f, 0.000f, 0.588f }, // 'X' 0x58 88: 152   0 150
	{ 0.725f, 0.725f, 0.725f }, // 'Y' 0x59 89: 185 185 185
	{ 0.329f, 0.329f, 0.329f }, // 'Z' 0x5A 90:  84  84  84
};
vec3_t rpmodColors[75] = // RPMod: add more text colors
{
	{ 0.0f, 0.0f, 0.0f }, // 0: black
	{ 1.0f, 0.0f, 0.0f }, // 1: red
	{ 0.0f, 1.0f, 0.0f }, // 2: green
	{ 1.0f, 1.0f, 0.0f }, // 3: yellow
	{ 0.0f, 0.0f, 1.0f }, // 4: blue
	{ 0.0f, 1.0f, 1.0f }, // 5: cyan
	{ 1.0f, 0.0f, 1.0f }, // 6: magenta
	{ 1.0f, 1.0f, 1.0f }, // 7: white
	{ 1.0f, 0.5f, 0.0f }, // 8: orange
	{ 0.5f, 0.0f, 0.8f }, // 9: purple
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 0.59f, 0.29f, 0.00f }, // A
	{ 1.00f, 0.80f, 0.64f }, // B
	{ 0.00f, 0.80f, 0.80f }, // C
	{ 0.00f, 0.00f, 0.50f }, // D
	{ 0.31f, 0.15f, 0.51f }, // E
	{ 1.00f, 0.75f, 0.80f }, // F
	{ 0.00f, 0.48f, 0.65f }, // G
	{ 0.00f, 0.29f, 0.33f }, // H
	{ 0.97f, 0.51f, 0.47f }, // I
	{ 0.00f, 0.50f, 0.50f }, // J
	{ 0.50f, 0.00f, 0.00f }, // K
	{ 0.64f, 0.35f, 0.32f }, // L
	{ 0.25f, 0.51f, 0.43f }, // M
	{ 0.50f, 0.00f, 1.00f }, // N
	{ 0.75f, 0.75f, 0.75f }, // O
	{ 0.21f, 0.27f, 0.31f }, // P
	{ 0.40f, 0.60f, 0.80f }, // Q
	{ 0.89f, 0.26f, 0.20f }, // R
	{ 0.77f, 0.38f, 0.06f }, // S
	{ 0.85f, 0.56f, 0.35f }, // T
	{ 0.91f, 0.38f, 0.00f }, // U
	{ 0.80f, 0.80f, 1.00f }, // V
	{ 0.69f, 0.88f, 0.90f }, // W
	{ 0.68f, 0.85f, 0.90f }, // X
	{ 0.54f, 0.81f, 0.94f }, // Y
	{ 0.00f, 0.00f, 1.00f }, // Z
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 1.0f, 1.0f, 1.0f }, // (padding)
	{ 0.25f, 0.00f, 1.00f }, // a
	{ 0.00f, 0.00f, 0.55f }, // b
	{ 0.11f, 0.16f, 0.32f }, // c
	{ 1.00f, 1.00f, 0.80f }, // d
	{ 0.94f, 0.80f, 0.00f }, // e
	{ 1.00f, 0.83f, 0.00f }, // f
	{ 1.00f, 0.94f, 0.00f }, // g
	{ 1.00f, 1.00f, 0.47f }, // h
	{ 0.98f, 0.85f, 0.37f }, // i
	{ 1.00f, 0.84f, 0.00f }, // j
	{ 0.76f, 0.69f, 0.57f }, // k
	{ 0.50f, 0.50f, 0.00f }, // l
	{ 0.88f, 0.07f, 0.37f }, // m
	{ 1.00f, 0.08f, 0.58f }, // n
	{ 1.00f, 0.44f, 1.00f }, // o
	{ 0.40f, 0.01f, 0.24f }, // p
	{ 0.47f, 0.32f, 0.66f }, // q
	{ 0.44f, 0.16f, 0.39f }, // r
	{ 0.47f, 0.09f, 0.29f }, // s
	{ 0.56f, 0.59f, 0.47f }, // t
	{ 0.53f, 0.66f, 0.42f }, // u
	{ 0.34f, 0.51f, 0.01f }, // v
	{ 0.68f, 1.00f, 0.18f }, // w
	{ 0.27f, 0.30f, 0.22f }, // x
	{ 0.00f, 0.34f, 0.25f }, // y
	{ 0.54f, 0.60f, 0.36f }, // z
};

int Q_parseColor( const char *p, const vec3_t *numberColors, float *color ) {
	char c = *p++;
	if (((c >= '0' && c <='9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) && numberColors == &(rpmodColors[0])) {
		if (!color)
			return 1;
		memcpy( color, numberColors + c - '0', sizeof( vec3_t ));
		return 1;
	} else if (((c >= '0' && c <='9') || (c >= 'A' && c <= 'Z')) && numberColors == &(uagColors[0])) {
		if (!color)
			return 1;
		memcpy( color, numberColors + c - '0', sizeof( vec3_t ));
		return 1;
	} else if (c >= '0' && c <='9') {
		if (!color)
			return 1;
		memcpy( color, numberColors + c - '0', sizeof( vec3_t ));
		return 1;
/*	} else if ( ( c >= 'a' && c < 'u') || (c >= 'A' && c < 'U') ) {
		int deg;
		float angle, v;
		if (!color)
			return 1;
		deg = (((c|32) - 'a') * 360) / 24;
		angle = (DEG2RAD(deg % 120));
		v = ((cos(angle) / cos((M_PI / 3) - angle)) + 1) / 3;
		if ( deg <= 120) {
			color[0] = v;
			color[1] = 1-v;
			color[2] = 0;
		} else if ( deg <= 240) {
			color[0] = 0;
			color[1] = v;
			color[2] = 1-v;
		} else {
			color[0] = 1-v;
			color[1] = 0;
			color[2] = v;
		}
		return 1;*/
	} else if ( c == 'u' || c == 'U' || c == 'v' || c == 'V'
				|| c == 'w' || c == 'W' || c == 'x' || c == 'X'
				|| c == 'y' || c == 'Y' || c == 'z' || c == 'Z'
				|| c == '#') {
		int i;
		int val;
		for (i = 0;i<6;i++) {
            int readHex;
			c = p[i];
			if ( c >= '0' && c<= '9') {
                readHex = c - '0';
			} else if ( c >= 'a' && c<= 'f') {
				readHex = 0xa + c - 'a';
			} else if ( c >= 'A' && c<= 'F') {
				readHex = 0xa + c - 'A';
			} else {
/*				if (color) {
					color[0] = color[1] = color[2] = 1.0f;
				}*/
				return -1;
			}
			if (!color)
				continue;
			if ( i & 1) {
				val|= readHex;
				color[i >> 1] = val * (1 / 255.0f);
			} else {
				val = readHex << 4;
			}
		}
		return 7;
	}
/*	if (color) {
		color[0] = color[1] = color[2] = 1.0f;
	}*/
	return -1;
}

int Q_parseColorString( const char *p, float *color, colorTable_t cTable) {
	char c;
	const vec3_t *table;
	if (!p )
		return 0;
	c = *p;
	if (!c || c!= Q_COLOR_ESCAPE || (c== Q_COLOR_ESCAPE && !p[1]))
		return 0;
	switch (cTable) {
	default:
	case CT_DEFAULT:
		table = defaultColors;
		break;
	case CT_UAG:
		table = uagColors;
		break;
	case CT_RPMOD:
		table = rpmodColors;
		break;
	}
	return 1 + Q_parseColor( p+1, table, color );
}
