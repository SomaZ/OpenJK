/*[Vertex]*/
#if defined(USE_LIGHT) && !defined(USE_FAST_LIGHT) && !defined(USE_LIGHT_VECTOR)
#define PER_PIXEL_LIGHTING
#endif
in vec2 attr_TexCoord0;
#if defined(USE_LIGHTMAP) || defined(USE_TCGEN)
in vec2 attr_TexCoord1;
in vec2 attr_TexCoord2;
in vec2 attr_TexCoord3;
in vec2 attr_TexCoord4;
#endif
in vec4 attr_Color;

in vec3 attr_Position;
in vec3 attr_Normal;
in vec4 attr_Tangent;

#if defined(USE_VERTEX_ANIMATION)
in vec3 attr_Position2;
in vec3 attr_Normal2;
in vec4 attr_Tangent2;
#elif defined(USE_SKELETAL_ANIMATION)
in uvec4 attr_BoneIndexes;
in vec4 attr_BoneWeights;
#endif

#if defined(USE_LIGHT) && !defined(USE_LIGHT_VECTOR)
in vec3 attr_LightDirection;
#endif

layout(std140) uniform Camera
{
	vec4 u_ViewInfo;
	vec3 u_ViewOrigin;
	vec3 u_ViewForward;
	vec3 u_ViewLeft;
	vec3 u_ViewUp;
};

layout(std140) uniform Scene
{
	vec4 u_PrimaryLightOrigin;
	vec3 u_PrimaryLightAmbient;
	vec3 u_PrimaryLightColor;
	float u_PrimaryLightRadius;
};

layout(std140) uniform Entity
{
	mat4 u_ModelMatrix;
	mat4 u_ModelViewProjectionMatrix;
	vec4 u_LocalLightOrigin;
	vec3 u_AmbientLight;
	float u_LocalLightRadius;
	vec3 u_DirectedLight;
	float _u_FXVolumetricBase;
	vec3 u_ModelLightDir;
	float u_VertexLerp;
	vec3 u_LocalViewOrigin;
};
uniform float u_FXVolumetricBase;

layout(std140) uniform Bones
{
	mat3x4 u_BoneMatrices[20];
};

#if defined(USE_DELUXEMAP)
uniform vec4   u_EnableTextures; // x = normal, y = deluxe, z = specular, w = cube
#endif

#if defined(USE_TCGEN) || defined(USE_LIGHTMAP)
uniform int u_TCGen0;
uniform vec3 u_TCGen0Vector0;
uniform vec3 u_TCGen0Vector1;
uniform int u_TCGen1;
#endif

#if defined(USE_TCMOD)
uniform vec4 u_DiffuseTexMatrix;
uniform vec4 u_DiffuseTexOffTurb;
#endif

uniform vec4 u_BaseColor;
uniform vec4 u_VertColor;

out vec4 var_TexCoords;
out vec4 var_Color;
out vec3 var_N;

#if defined(PER_PIXEL_LIGHTING)
out vec4 var_Normal;
out vec4 var_Tangent;
out vec4 var_Bitangent;
#endif

#if defined(PER_PIXEL_LIGHTING)
out vec4 var_LightDir;
#endif

#if defined(USE_PRIMARY_LIGHT) || defined(USE_SHADOWMAP)
out vec4 var_PrimaryLightDir;
#endif

#if defined(USE_TCGEN) || defined(USE_LIGHTMAP)
vec2 GenTexCoords(int TCGen, vec3 position, vec3 normal, vec3 TCGenVector0, vec3 TCGenVector1)
{
	vec2 tex = attr_TexCoord0;

	switch (TCGen)
	{
		case TCGEN_LIGHTMAP:
			tex = attr_TexCoord1;
		break;

		case TCGEN_LIGHTMAP1:
			tex = attr_TexCoord2;
		break;

		case TCGEN_LIGHTMAP2:
			tex = attr_TexCoord3;
		break;

		case TCGEN_LIGHTMAP3:
			tex = attr_TexCoord4;
		break;

		case TCGEN_ENVIRONMENT_MAPPED:
		{
			vec3 viewer = normalize(u_LocalViewOrigin - position);
			vec2 ref = reflect(viewer, normal).yz;
			tex.s = ref.x * -0.5 + 0.5;
			tex.t = ref.y *  0.5 + 0.5;
		}
		break;

		case TCGEN_VECTOR:
		{
			tex = vec2(dot(position, TCGenVector0), dot(position, TCGenVector1));
		}
		break;
	}

	return tex;
}
#endif

#if defined(USE_TCMOD)
vec2 ModTexCoords(vec2 st, vec3 position, vec4 texMatrix, vec4 offTurb)
{
	float amplitude = offTurb.z;
	float phase = offTurb.w * 2.0 * M_PI;
	vec2 st2;
	st2.x = st.x * texMatrix.x + (st.y * texMatrix.z + offTurb.x);
	st2.y = st.x * texMatrix.y + (st.y * texMatrix.w + offTurb.y);

	vec2 offsetPos = vec2(position.x + position.z, position.y);

	vec2 texOffset = sin(offsetPos * (2.0 * M_PI / 1024.0) + vec2(phase));

	return st2 + texOffset * amplitude;	
}
#endif

float CalcLightAttenuation(in bool isPoint, float normDist)
{
	// zero light at 1.0, approximating q3 style
	// also don't attenuate directional light
	float attenuation = 1.0 + mix(0.0, 0.5 * normDist - 1.5, isPoint);
	return clamp(attenuation, 0.0, 1.0);
}

mat4 GetBoneMatrix(uint index)
{
	mat3x4 bone = u_BoneMatrices[index];
	return mat4(
		bone[0].x, bone[1].x, bone[2].x, 0.0,
		bone[0].y, bone[1].y, bone[2].y, 0.0,
		bone[0].z, bone[1].z, bone[2].z, 0.0,
		bone[0].w, bone[1].w, bone[2].w, 1.0);
}

void main()
{
#if defined(USE_VERTEX_ANIMATION)
	vec3 position  = mix(attr_Position,    attr_Position2,    u_VertexLerp);
	vec3 normal    = mix(attr_Normal,      attr_Normal2,      u_VertexLerp);
	vec3 tangent   = mix(attr_Tangent.xyz, attr_Tangent2.xyz, u_VertexLerp);
#elif defined(USE_SKELETAL_ANIMATION)
	vec4 position4 = vec4(0.0);
	vec4 normal4 = vec4(0.0);
	vec4 originalPosition = vec4(attr_Position, 1.0);
	vec4 originalNormal = vec4(attr_Normal - vec3 (0.5), 0.0);
#if defined(PER_PIXEL_LIGHTING)
	vec4 tangent4 = vec4(0.0);
	vec4 originalTangent = vec4(attr_Tangent.xyz - vec3(0.5), 0.0);
#endif

	for (int i = 0; i < 4; i++)
	{
		uint boneIndex = attr_BoneIndexes[i];
		mat4 boneMatrix = GetBoneMatrix(boneIndex);
		position4 += (boneMatrix * originalPosition) * attr_BoneWeights[i];
		normal4 += (boneMatrix * originalNormal) * attr_BoneWeights[i];
#if defined(PER_PIXEL_LIGHTING)
		tangent4 += (boneMatrix * originalTangent) * attr_BoneWeights[i];
#endif
	}

	vec3 position = position4.xyz;
	vec3 normal = normalize (normal4.xyz);
#if defined(PER_PIXEL_LIGHTING)
	vec3 tangent = normalize (tangent4.xyz);
#endif
#else
	vec3 position  = attr_Position;
	vec3 normal    = attr_Normal;
  #if defined(PER_PIXEL_LIGHTING)
	vec3 tangent   = attr_Tangent.xyz;
  #endif
#endif

#if !defined(USE_SKELETAL_ANIMATION)
	normal  = normal  * 2.0 - vec3(1.0);
  #if defined(PER_PIXEL_LIGHTING)
	tangent = tangent * 2.0 - vec3(1.0);
  #endif
#endif

#if defined(USE_TCGEN)
	vec2 texCoords = GenTexCoords(u_TCGen0, position, normal, u_TCGen0Vector0, u_TCGen0Vector1);
#else
	vec2 texCoords = attr_TexCoord0.st;
#endif

#if defined(USE_TCMOD)
	var_TexCoords.xy = ModTexCoords(texCoords, position, u_DiffuseTexMatrix, u_DiffuseTexOffTurb);
#else
	var_TexCoords.xy = texCoords;
#endif

	gl_Position = u_ModelViewProjectionMatrix * vec4(position, 1.0);

	position  = (u_ModelMatrix * vec4(position, 1.0)).xyz;
	normal    = (u_ModelMatrix * vec4(normal,   0.0)).xyz;
  #if defined(PER_PIXEL_LIGHTING)
	tangent   = (u_ModelMatrix * vec4(tangent,  0.0)).xyz;
  #endif

#if defined(PER_PIXEL_LIGHTING)
	vec3 bitangent = cross(normal, tangent) * (attr_Tangent.w * 2.0 - 1.0);
#endif

#if defined(USE_LIGHT_VECTOR)
	vec3 L = vec3(0.0);
#elif defined(PER_PIXEL_LIGHTING)
	vec3 L = attr_LightDirection * 2.0 - vec3(1.0);
	L = (u_ModelMatrix * vec4(L, 0.0)).xyz;
#endif

#if defined(USE_LIGHTMAP)
	var_TexCoords.zw = GenTexCoords(u_TCGen1, vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0));
#endif

	if ( u_FXVolumetricBase > 0.0 )
	{
		vec3 viewForward = u_ViewForward.xyz;

		float d = clamp(dot(normalize(viewForward), normalize(normal)), 0.0, 1.0);
		d = d * d;
		d = d * d;

		var_Color = vec4(u_FXVolumetricBase * (1.0 - d));
	}
	else
	{
		var_Color = u_VertColor * attr_Color + u_BaseColor;
	}

#if defined(USE_PRIMARY_LIGHT) || defined(USE_SHADOWMAP)
	var_PrimaryLightDir.xyz = u_PrimaryLightOrigin.xyz - (position * u_PrimaryLightOrigin.w);
	var_PrimaryLightDir.w = u_PrimaryLightRadius * u_PrimaryLightRadius;
#endif

#if defined(PER_PIXEL_LIGHTING)
	var_LightDir = vec4(L, 0.0);
  #if defined(USE_DELUXEMAP)
	var_LightDir -= u_EnableTextures.y * var_LightDir;
  #endif
#endif

#if defined(PER_PIXEL_LIGHTING)
	vec3 viewDir = u_ViewOrigin.xyz - position;

	// store view direction in tangent space to save on outs
	var_Normal    = vec4(normal,    viewDir.x);
	var_Tangent   = vec4(tangent,   viewDir.y);
	var_Bitangent = vec4(bitangent, viewDir.z);
#endif
}

/*[Fragment]*/
#if defined(USE_LIGHT) && !defined(USE_VERTEX_LIGHTING) && !defined(USE_LIGHT_VECTOR)
#define PER_PIXEL_LIGHTING
#endif
layout(std140) uniform Scene
{
	vec4 u_PrimaryLightOrigin;
	vec3 u_PrimaryLightAmbient;
	vec3 u_PrimaryLightColor;
	float u_PrimaryLightRadius;
};

layout(std140) uniform Camera
{
	vec4 u_ViewInfo;
	vec3 u_ViewOrigin;
	vec3 u_ViewForward;
	vec3 u_ViewLeft;
	vec3 u_ViewUp;
};

layout(std140) uniform Entity
{
	mat4 u_ModelMatrix;
	mat4 u_ModelViewProjectionMatrix;
	vec4 u_LocalLightOrigin;
	vec3 u_AmbientLight;
	float u_LocalLightRadius;
	vec3 u_DirectedLight;
	float _u_FXVolumetricBase;
	vec3 u_ModelLightDir;
	float u_VertexLerp;
	vec3 u_LocalViewOrigin;
};

uniform sampler2D u_DiffuseMap;

#if defined(USE_LIGHTMAP)
uniform sampler2D u_LightMap;
#endif

#if defined(USE_NORMALMAP)
uniform sampler2D u_NormalMap;
#endif

#if defined(USE_DELUXEMAP)
uniform sampler2D u_DeluxeMap;
#endif

#if defined(USE_SPECULARMAP)
uniform sampler2D u_SpecularMap;
#endif

#if defined(USE_SHADOWMAP)
uniform sampler2D u_ShadowMap;
#endif

#if defined(USE_CUBEMAP)
uniform samplerCube u_CubeMap;
uniform sampler2D u_EnvBrdfMap;
#endif

#if defined(USE_NORMALMAP) || defined(USE_DELUXEMAP) || defined(USE_SPECULARMAP) || defined(USE_CUBEMAP)
// y = deluxe, w = cube
uniform vec4 u_EnableTextures;
#endif

uniform vec4 u_NormalScale;
uniform vec4 u_SpecularScale;

#if defined(PER_PIXEL_LIGHTING) && defined(USE_CUBEMAP)
uniform vec4 u_CubeMapInfo;
#endif

#if defined(USE_ALPHA_TEST)
uniform int u_AlphaTestType;
#endif


in vec4 var_TexCoords;
in vec4 var_Color;

#if defined(PER_PIXEL_LIGHTING)
in vec4 var_Normal;
in vec4 var_Tangent;
in vec4 var_Bitangent;
in vec4 var_LightDir;
#endif

#if defined(USE_PRIMARY_LIGHT) || defined(USE_SHADOWMAP)
in vec4 var_PrimaryLightDir;
#endif

out vec4 out_Color;
out vec4 out_Glow;

#define EPSILON 0.00000001

#if defined(USE_PARALLAXMAP)
float SampleDepth(sampler2D normalMap, vec2 t)
{
  #if defined(SWIZZLE_NORMALMAP)
	return 1.0 - texture(normalMap, t).r;
  #else
	return 1.0 - texture(normalMap, t).a;
  #endif
}

float RayIntersectDisplaceMap(vec2 dp, vec2 ds, sampler2D normalMap)
{
	const int linearSearchSteps = 16;
	const int binarySearchSteps = 6;

	// current size of search window
	float size = 1.0 / float(linearSearchSteps);

	// current depth position
	float depth = 0.0;

	// best match found (starts with last position 1.0)
	float bestDepth = 1.0;

	// search front to back for first point inside object
	for(int i = 0; i < linearSearchSteps - 1; ++i)
	{
		depth += size;
		
		float t = SampleDepth(normalMap, dp + ds * depth);
		
		if(bestDepth > 0.996)		// if no depth found yet
			if(depth >= t)
				bestDepth = depth;	// store best depth
	}

	depth = bestDepth;
	
	// recurse around first point (depth) for closest match
	for(int i = 0; i < binarySearchSteps; ++i)
	{
		size *= 0.5;

		float t = SampleDepth(normalMap, dp + ds * depth);
		
		if(depth >= t)
		{
			bestDepth = depth;
			depth -= 2.0 * size;
		}

		depth += size;
	}

	return bestDepth;
}
#endif

vec3 CalcFresnel( in vec3 f0, in vec3 f90, in float LH )
{
	return f0 + (f90 - f0) * pow(1.0 - LH, 5.0);
}

float CalcGGX( in float NH, in float roughness )
{
	float alphaSq = roughness*roughness;
	float f = (NH * alphaSq - NH) * NH + 1.0;
	return alphaSq / (f * f);
}

float CalcVisibility( in float NL, in float NE, in float roughness )
{
	float alphaSq = roughness*roughness;

	float lambdaE = NL * sqrt((-NE * alphaSq + NE) * NE + alphaSq);
	float lambdaL = NE * sqrt((-NL * alphaSq + NL) * NL + alphaSq);

	return 0.5 / (lambdaE + lambdaL);
}

// http://www.frostbite.com/2014/11/moving-frostbite-to-pbr/
vec3 CalcSpecular(
	in vec3 specular,
	in float NH,
	in float NL,
	in float NE,
	in float LH,
	in float roughness
)
{
	vec3  F = CalcFresnel(specular, vec3(1.0), LH);
	float D = CalcGGX(NH, roughness);
	float V = CalcVisibility(NL, NE, roughness);

	return D * F * V;
}

vec3 CalcDiffuse(
	in vec3 diffuse,
	in float NE,
	in float NL,
	in float LH,
	in float roughness
)
{
	return diffuse;
}

float CalcLightAttenuation(float point, float normDist)
{
	// zero light at 1.0, approximating q3 style
	// also don't attenuate directional light
	float attenuation = (0.5 * normDist - 1.5) * point + 1.0;
	return clamp(attenuation, 0.0, 1.0);
}

vec2 GetParallaxOffset(in vec2 texCoords, in vec3 E, in mat3 tangentToWorld )
{
#if defined(USE_PARALLAXMAP)
	vec3 offsetDir = normalize(E * tangentToWorld);
	offsetDir.xy *= -u_NormalScale.a / offsetDir.z;

	return offsetDir.xy * RayIntersectDisplaceMap(texCoords, offsetDir.xy, u_NormalMap);
#else
	return vec2(0.0);
#endif
}

vec3 CalcIBLContribution(
	in float roughness,
	in vec3 N,
	in vec3 E,
	in vec3 viewOrigin,
	in vec3 viewDir,
	in float NE,
	in vec3 specular
)
{
#if defined(USE_CUBEMAP)
	vec3 EnvBRDF = texture(u_EnvBrdfMap, vec2(roughness, NE)).rgb;

	vec3 R = reflect(E, N);

	// parallax corrected cubemap (cheaper trick)
	// from http://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/
	vec3 eyeToCubeCenter = (u_CubeMapInfo.xyz - viewOrigin) * 0.001;
	vec3 parallax = eyeToCubeCenter + u_CubeMapInfo.w * viewDir * 0.001;

	vec3 cubeLightColor = textureLod(u_CubeMap, R + parallax, roughness * ROUGHNESS_MIPS).rgb * u_EnableTextures.w;

	return cubeLightColor * (specular.rgb * EnvBRDF.x + EnvBRDF.y);
#else
	return vec3(0.0);
#endif
}

vec3 CalcNormal( in vec3 vertexNormal, in vec2 texCoords, in mat3 tangentToWorld )
{
	vec3 N = vertexNormal;

#if defined(USE_NORMALMAP)
  #if defined(SWIZZLE_NORMALMAP)
	N.xy = texture(u_NormalMap, texCoords).ag - vec2(0.5);
  #else
	N.xy = texture(u_NormalMap, texCoords).rg - vec2(0.5);
  #endif

	N.xy *= u_NormalScale.xy;
	N.z = sqrt(clamp((0.25 - N.x * N.x) - N.y * N.y, 0.0, 1.0));
	N = tangentToWorld * N;
#endif

	return normalize(N);
}

vec3 sRGBToLinear( in vec3 srgb )
{
	vec3 lo = srgb / 12.92;
	vec3 hi = pow(((srgb + vec3(0.055)) / 1.055), vec3(2.4));
	return mix(lo, hi, greaterThan(srgb, vec3(0.04045)));
}

void main()
{
	vec3 viewDir, lightColor, ambientColor;
	vec3 L, N, E;

#if defined(PER_PIXEL_LIGHTING)
	mat3 tangentToWorld = mat3(var_Tangent.xyz, var_Bitangent.xyz, var_Normal.xyz);
	viewDir = vec3(var_Normal.w, var_Tangent.w, var_Bitangent.w);
	E = normalize(viewDir);
	L = var_LightDir.xyz;
  #if defined(USE_DELUXEMAP)
	L += (texture(u_DeluxeMap, var_TexCoords.zw).xyz - vec3(0.5)) * u_EnableTextures.y;
  #endif
	float sqrLightDist = dot(L, L);
#endif

#if defined(USE_LIGHTMAP)
	vec4 lightmapColor = texture(u_LightMap, var_TexCoords.zw);
  #if defined(RGBM_LIGHTMAP)
	lightmapColor.rgb *= lightmapColor.a;
  #endif
	//lightmapColor.rgb = sRGBToLinear(lightmapColor.rgb);
#endif

	vec2 texCoords = var_TexCoords.xy;
#if defined(PER_PIXEL_LIGHTING)
	texCoords += GetParallaxOffset(texCoords, E, tangentToWorld);
#endif

	vec4 diffuse = texture(u_DiffuseMap, texCoords);
#if defined(USE_ALPHA_TEST)
	if (u_AlphaTestType == ALPHA_TEST_GT0)
	{
		if (diffuse.a == 0.0)
			discard;
	}
	else if (u_AlphaTestType == ALPHA_TEST_LT128)
	{
		if (diffuse.a >= 0.5)
			discard;
	}
	else if (u_AlphaTestType == ALPHA_TEST_GE128)
	{
		if (diffuse.a < 0.5)
			discard;
	}
	else if (u_AlphaTestType == ALPHA_TEST_GE192)
	{
		if (diffuse.a < 0.75)
			discard;
	}
#endif

	//diffuse.rgb = sRGBToLinear(diffuse.rgb);

#if defined(PER_PIXEL_LIGHTING)
	float attenuation;

  #if defined(USE_LIGHTMAP)
	lightColor = lightmapColor.rgb * var_Color.rgb;
	ambientColor = vec3 (0.0);
	attenuation = 1.0;
  #elif defined(USE_LIGHT_VECTOR)
	lightColor = vec3(0.0);
	ambientColor = vec3(0.0);
	attenuation = 0.0;
  #elif defined(USE_LIGHT_VERTEX)
	lightColor = var_Color.rgb;
	ambientColor = vec3 (0.0);
	attenuation = 1.0;
  #endif

	N = CalcNormal(var_Normal.xyz, texCoords, tangentToWorld);
	L /= sqrt(sqrLightDist);

  #if defined(USE_SHADOWMAP)
	vec2 shadowTex = gl_FragCoord.xy * r_FBufScale;
	float shadowValue = texture(u_ShadowMap, shadowTex).r;

	// surfaces not facing the light are always shadowed
	vec3 primaryLightDir = normalize(var_PrimaryLightDir.xyz);
	shadowValue = mix(0.0, shadowValue, dot(N, primaryLightDir) > 0.0);

    #if defined(SHADOWMAP_MODULATE)
	lightColor = mix(u_PrimaryLightAmbient * lightColor, lightColor, shadowValue);
    #endif
  #endif

  #if defined(USE_LIGHTMAP) || defined(USE_LIGHT_VERTEX)
	ambientColor = lightColor;
	float surfNL = clamp(dot(N, L), 0.0, 1.0);

	// Scale the incoming light to compensate for the baked-in light angle
	// attenuation.
	lightColor /= max(surfNL, 0.25);

	// Recover any unused light as ambient, in case attenuation is over 4x or
	// light is below the surface
	ambientColor = clamp(ambientColor - lightColor * surfNL, 0.0, 1.0);
  #endif

	vec4 specular = vec4(1.0);
  #if defined(USE_SPECULARMAP)
	specular = texture(u_SpecularMap, texCoords);
	//specular.rgb = sRGBToLinear(specular.rgb);
  #endif
	specular *= u_SpecularScale;

	// diffuse is actually base color, and red of specular is metalness
	const vec3 DIELECTRIC_SPECULAR = vec3(0.04);
	const vec3 METAL_DIFFUSE       = vec3(0.0);

	float metalness = specular.r;
	float roughness = max(specular.a, 0.02);
	specular.rgb = mix(DIELECTRIC_SPECULAR, diffuse.rgb,   metalness);
	diffuse.rgb  = mix(diffuse.rgb,         METAL_DIFFUSE, metalness);

	vec3  H  = normalize(L + E);
	float NE = abs(dot(N, E)) + 1e-5;
	float NL = clamp(dot(N, L), 0.0, 1.0);
	float LH = clamp(dot(L, H), 0.0, 1.0);

	vec3  Fd = CalcDiffuse(diffuse.rgb, NE, NL, LH, roughness);
	vec3  Fs = vec3(0.0);

	vec3 reflectance = Fd + Fs;

	out_Color.rgb  = lightColor * reflectance * (attenuation * NL);
	out_Color.rgb += ambientColor * diffuse.rgb;
	
  #if defined(USE_PRIMARY_LIGHT)
	vec3  L2   = normalize(var_PrimaryLightDir.xyz);
	vec3  H2   = normalize(L2 + E);
	float NL2  = clamp(dot(N,  L2), 0.0, 1.0);
	float L2H2 = clamp(dot(L2, H2), 0.0, 1.0);
	float NH2  = clamp(dot(N,  H2), 0.0, 1.0);

	reflectance  = CalcDiffuse(diffuse.rgb, NE, NL2, L2H2, roughness);
	reflectance += CalcSpecular(specular.rgb, NH2, NL2, NE, L2H2, roughness);

	lightColor = u_PrimaryLightColor * var_Color.rgb;
    #if defined(USE_SHADOWMAP)
	lightColor *= shadowValue;
    #endif

	out_Color.rgb += lightColor * reflectance * NL2;
  #endif
	
	out_Color.rgb += CalcIBLContribution(
		roughness, N, E, u_ViewOrigin, viewDir, NE, specular.rgb);
#else
	lightColor = var_Color.rgb;
  #if defined(USE_LIGHTMAP) 
	lightColor *= lightmapColor.rgb;
  #endif

    out_Color.rgb = diffuse.rgb * lightColor;
#endif
	
	out_Color.a = diffuse.a * var_Color.a;

#if defined(USE_GLOW_BUFFER)
	out_Glow = out_Color;
#else
	out_Glow = vec4(0.0);
#endif
}
