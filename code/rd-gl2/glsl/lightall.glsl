/*[Vertex]*/
#if defined(USE_LIGHT) && !defined(USE_FAST_LIGHT)
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

#if defined(USE_DELUXEMAP)
uniform vec4   u_EnableTextures; // x = normal, y = deluxe, z = specular, w = cube
#endif

#if defined(PER_PIXEL_LIGHTING)
uniform vec3 u_ViewOrigin;
#endif

#if defined(USE_TCGEN) || defined(USE_LIGHTMAP)
uniform int u_TCGen0;
uniform vec3 u_TCGen0Vector0;
uniform vec3 u_TCGen0Vector1;
uniform vec3 u_LocalViewOrigin;
uniform int u_TCGen1;
#endif

#if defined(USE_TCMOD)
uniform vec4 u_DiffuseTexMatrix;
uniform vec4 u_DiffuseTexOffTurb;
#endif

uniform mat4 u_ModelViewProjectionMatrix;
uniform vec4 u_BaseColor;
uniform vec4 u_VertColor;
uniform mat4 u_ModelMatrix;
uniform mat4 u_NormalMatrix;

uniform int u_ColorGen;
uniform int u_AlphaGen;
uniform vec4 u_Disintegration; // origin, threshhold

#if defined(USE_VERTEX_ANIMATION)
uniform float u_VertexLerp;
#elif defined(USE_SKELETAL_ANIMATION)
uniform mat4x3 u_BoneMatrices[20];
#endif

#if defined(USE_LIGHT_VECTOR)
uniform vec4 u_LightOrigin;
uniform float u_LightRadius;
uniform vec3 u_DirectedLight;
uniform vec3 u_AmbientLight;
#endif

#if defined(USE_PRIMARY_LIGHT) || defined(USE_SHADOWMAP)
uniform vec4 u_PrimaryLightOrigin;
uniform float u_PrimaryLightRadius;
#endif

uniform vec3 u_ViewForward;
uniform float u_FXVolumetricBase;

out vec4 var_TexCoords;
out vec4 var_Color;

#if defined(PER_PIXEL_LIGHTING)
out vec3 var_Position;
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

vec4 CalcDisintegration(vec3 position)
{
	vec4 color = vec4(1.0);
	if (u_ColorGen == CGEN_DISINTEGRATION_1)
	{
		vec3 delta = u_Disintegration.xyz - position;
		float distance = dot(delta, delta);
		if (distance < u_Disintegration.w)
		{
			color = vec4(0.0);
		}
		else if (distance < u_Disintegration.w + 60.0)
		{
			color = vec4(0.0, 0.0, 0.0, 1.0);
		}
		else if (distance < u_Disintegration.w + 150.0)
		{
			color = vec4(0.435295, 0.435295, 0.435295, 1.0);
		}
		else if (distance < u_Disintegration.w + 180.0)
		{
			color = vec4(0.6862745, 0.6862745, 0.6862745, 1.0);
		}
		return color;
	}
	else if (u_ColorGen == CGEN_DISINTEGRATION_2)
	{
		vec3 delta = u_Disintegration.xyz - position;
		float distance = dot(delta, delta);
		if (distance < u_Disintegration.w)
		{
			color = vec4(0.0);
		}
		return color;
	}
	return color;
}

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

float CalcLightAttenuation(float distance, float radius)
{
	float d = pow(distance / radius, 4.0);
	float attenuation = clamp(1.0 - d, 0.0, 1.0);
	attenuation *= attenuation;
	attenuation /= distance * distance + 1.0;
	// don't attenuate directional light
	attenuation = attenuation + float(radius < 1.0);

	return clamp(attenuation, 0.0, 1.0);
}

void main()
{
#if defined(USE_VERTEX_ANIMATION)
	vec3 position  = mix(attr_Position,    attr_Position2,    u_VertexLerp);
	vec3 normal    = mix(attr_Normal,      attr_Normal2,      u_VertexLerp);
	vec3 tangent   = mix(attr_Tangent.xyz, attr_Tangent2.xyz, u_VertexLerp);
#elif defined(USE_SKELETAL_ANIMATION)
	mat4x3 influence =
		u_BoneMatrices[attr_BoneIndexes[0]] * attr_BoneWeights[0] +
        u_BoneMatrices[attr_BoneIndexes[1]] * attr_BoneWeights[1] +
        u_BoneMatrices[attr_BoneIndexes[2]] * attr_BoneWeights[2] +
        u_BoneMatrices[attr_BoneIndexes[3]] * attr_BoneWeights[3];

    vec3 position = influence * vec4(attr_Position, 1.0);
    vec3 normal = normalize(influence * vec4(attr_Normal - vec3(0.5), 0.0));
	#if defined(PER_PIXEL_LIGHTING)
		vec3 tangent = normalize(influence * vec4(attr_Tangent.xyz - vec3(0.5), 0.0));
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

    if ( u_FXVolumetricBase > 0.0 )
	{
		vec3 viewForward = u_ViewForward;

		float d = clamp(dot(normalize(viewForward), normalize(normal)), 0.0, 1.0);
		d = d * d;
		d = d * d;

		var_Color = vec4(u_FXVolumetricBase * (1.0 - d));
	}
	else
	{
		var_Color = u_VertColor * attr_Color + u_BaseColor;
	}

	var_Color *= CalcDisintegration(position);

	gl_Position = u_ModelViewProjectionMatrix * vec4(position, 1.0);

	position  = (u_ModelMatrix * vec4(position, 1.0)).xyz;
	normal    = mat3(u_NormalMatrix) * normal;
  #if defined(PER_PIXEL_LIGHTING)
	tangent   = mat3(u_NormalMatrix) * tangent;
	vec3 bitangent = cross(normal, tangent) * (attr_Tangent.w * 2.0 - 1.0);
  #endif

#if defined(USE_LIGHT_VECTOR)
	vec3 L = u_LightOrigin.xyz - (position * u_LightOrigin.w);
#elif defined(PER_PIXEL_LIGHTING)
	vec3 L = attr_LightDirection * 2.0 - vec3(1.0);
	L = (u_ModelMatrix * vec4(L, 0.0)).xyz;
#endif

#if defined(USE_LIGHTMAP)
	var_TexCoords.zw = GenTexCoords(u_TCGen1, vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0));
#endif

#if defined(USE_LIGHT_VECTOR) && defined(USE_FAST_LIGHT)
	float lightDist = length(L);
	float attenuation = CalcLightAttenuation(lightDist, u_LightRadius);
	float NL = clamp(dot(normalize(normal), L) / lightDist, 0.0, 1.0);

	var_Color.rgb *= u_DirectedLight * (u_LightRadius + float(u_LightRadius < 1.0)) * (attenuation * NL) + u_AmbientLight;
#endif

#if defined(USE_PRIMARY_LIGHT) || defined(USE_SHADOWMAP)
	var_PrimaryLightDir.xyz = u_PrimaryLightOrigin.xyz - (position * u_PrimaryLightOrigin.w);
	var_PrimaryLightDir.w = u_PrimaryLightRadius * u_PrimaryLightRadius;
#endif

#if defined(PER_PIXEL_LIGHTING)
  #if defined(USE_LIGHT_VECTOR)
	var_LightDir = vec4(L, u_LightRadius);
  #else
	var_LightDir = vec4(L, 0.0);
  #endif
  #if defined(USE_DELUXEMAP)
	var_LightDir -= u_EnableTextures.y * var_LightDir;
  #endif
#endif

#if defined(PER_PIXEL_LIGHTING)
	vec3 viewDir = u_ViewOrigin - position;
	var_Position = position;
	// store view direction in tangent space to save on outs
	var_Normal    = vec4(normal,    viewDir.x);
	var_Tangent   = vec4(tangent,   viewDir.y);
	var_Bitangent = vec4(bitangent, viewDir.z);
#endif
}

/*[Fragment]*/
#if defined(USE_LIGHT) && !defined(USE_FAST_LIGHT)
#define PER_PIXEL_LIGHTING
uniform sampler2D u_ScreenDiffuseMap;
uniform sampler2D u_ScreenSpecularMap;
#endif

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
#if defined(EQUIRECTANGULAR_CUBEMAPS)
uniform sampler2D u_CubeMap;
#else
uniform samplerCube u_CubeMap;
#endif
#endif

#if defined(USE_NORMALMAP) || defined(USE_DELUXEMAP) || defined(USE_SPECULARMAP) || defined(USE_CUBEMAP)
// x = sphericalHarmonics, y = deluxe, z = specular, w = cube
uniform vec4      u_EnableTextures; 
#endif

#if defined(USE_LIGHT_VECTOR) && !defined(USE_FAST_LIGHT)
uniform vec3 u_DirectedLight;
uniform vec3 u_AmbientLight;
uniform samplerCubeShadow u_ShadowMap2;
#endif

#if defined(USE_PRIMARY_LIGHT) || defined(USE_SHADOWMAP)
uniform vec3  u_PrimaryLightColor;
uniform vec3  u_PrimaryLightAmbient;
#endif

#if defined(PER_PIXEL_LIGHTING)
uniform vec4      u_NormalScale;
uniform vec4      u_SpecularScale;
#endif

#if defined(PER_PIXEL_LIGHTING)
#if defined(USE_CUBEMAP)
uniform vec4      u_CubeMapInfo;
uniform vec3      u_SphericalHarmonic[9];
uniform sampler2D u_EnvBrdfMap;
#endif
#endif

uniform int u_AlphaTestFunction;
uniform float u_AlphaTestValue;

in vec4      var_TexCoords;
in vec4      var_Color;

#if defined(PER_PIXEL_LIGHTING)
in vec4   var_Normal;
in vec4   var_Tangent;
in vec4   var_Bitangent;
in vec4   var_LightDir;

in vec3	  var_Position;

uniform sampler3D u_LightGridDirectionMap;
uniform sampler3D u_LightGridDirectionalLightMap;
uniform sampler3D u_LightGridAmbientLightMap;
uniform vec3 u_LightGridOrigin;
uniform vec3 u_LightGridCellInverseSize;
uniform vec3 u_StyleColor;
uniform vec2 u_LightGridLightScale;

#define u_LightGridAmbientScale u_LightGridLightScale.x
#define u_LightGridDirectionalScale u_LightGridLightScale.y
#endif

#if defined(USE_PRIMARY_LIGHT) || defined(USE_SHADOWMAP)
in vec4      var_PrimaryLightDir;
#endif

out vec4 out_Color;
out vec4 out_Glow;

#define EPSILON 0.00000001

#if defined(USE_PARALLAXMAP)
float SampleDepth(sampler2D normalMap, vec2 t)
{
  return 1.0 - texture(normalMap, t).r;
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

	// texture depth at best depth
	float texDepth = 0.0;

	float prevT = SampleDepth(normalMap, dp);
	float prevTexDepth = prevT;

	// search front to back for first point inside object
	for(int i = 0; i < linearSearchSteps - 1; ++i)
	{
		depth += size;
		
		float t = SampleDepth(normalMap, dp + ds * depth);
		
		if(bestDepth > 0.996)		// if no depth found yet
			if(depth >= t)
			{
				bestDepth = depth;	// store best depth
				texDepth = t;
				prevTexDepth = prevT;
			}
		prevT = t;
	}

	depth = bestDepth;

#if !defined (USE_RELIEFMAP)
	float div = 1.0 / (1.0 + (prevTexDepth - texDepth) * float(linearSearchSteps));
	bestDepth -= (depth - size - prevTexDepth) * div;
#else
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
#endif

	return bestDepth;
}
#endif



vec3 CalcDiffuse(vec3 diffuseAlbedo)
{
#if defined(USE_BURLEY)
	// modified from https://disney-animation.s3.amazonaws.com/library/s2012_pbs_disney_brdf_notes_v2.pdf
	float fd90 = -0.5 + EH * EH * roughness;
	float burley = 1.0 + fd90 * 0.04 / NH;
	burley *= burley;
	return diffuseAlbedo * burley;
#else
	return diffuseAlbedo / M_PI;
#endif
}

vec3 F_Schlick(in vec3 SpecularColor, in float VH)
{
	float Fc = pow(1 - VH, 5);
	return clamp(50.0 * SpecularColor.g, 0.0, 1.0) * Fc + (1 - Fc) * SpecularColor; //hacky way to decide if reflectivity is too low (< 2%)
}

float D_GGX( in float NH, in float a )
{
	float a2 = a * a;
	float d = (NH * a2 - NH) * NH + 1;
	return a2 / (M_PI * d * d);
}

// Appoximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float V_SmithJointApprox(in float a, in float NV, in float NL)
{
	float Vis_SmithV = NL * (NV * (1 - a) + a);
	float Vis_SmithL = NV * (NL * (1 - a) + a);
	return 0.5 * (1.0 / (Vis_SmithV + Vis_SmithL));
}

// http://www.frostbite.com/2014/11/moving-frostbite-to-pbr/
vec3 CalcSpecular(
	in vec3 specular,
	in float NH,
	in float NL,
	in float NE,
	in float VH,
	in float roughness
)
{
	vec3  F = F_Schlick(specular, VH);
	float D = D_GGX(NH, roughness);
	float V = V_SmithJointApprox(roughness, NE, NL);
	return  D * F * V;
}

float CalcLightAttenuation(float distance, float radius)
{
	float d = pow(distance / radius, 4.0);
	float attenuation = clamp(1.0 - d, 0.0, 1.0);
	attenuation *= attenuation;
	attenuation /= distance * distance + 1.0;
	// don't attenuate directional light
	attenuation = attenuation + float(radius < 1.0);

	return clamp(attenuation, 0.0, 1.0);
}

vec3 CalcNormal( in vec3 vertexNormal, in vec2 texCoords, in mat3 tangentToWorld )
{
	vec3 N = vertexNormal;

#if defined(USE_NORMALMAP)
	N.xy = texture(u_NormalMap, texCoords).ag - vec2(0.5);
	N.xy *= u_NormalScale.xy;
	N.z = sqrt(clamp((0.25 - N.x * N.x) - N.y * N.y, 0.0, 1.0));
	N = tangentToWorld * N;
#endif

	return normalize(N);
}

#if defined(USE_LIGHT_VECTOR)
#define DEPTH_MAX_ERROR 0.000000059604644775390625

vec3 sampleOffsetDirections[20] = vec3[]
(
	vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
	vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
	vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
	vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
	vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1)
	);

float pcfShadow(samplerCubeShadow depthMap, vec3 L, float distance)
{
	float shadow = 0.0;
	int samples = 20;
	float diskRadius = 1.0;
	for (int i = 0; i < samples; ++i)
	{
		shadow += texture(depthMap, vec4(L + sampleOffsetDirections[i] * diskRadius, distance));
	}
	shadow /= float(samples);
	return shadow;
}

float getLightDepth(vec3 Vec, float f)
{
	vec3 AbsVec = abs(Vec);
	float Z = max(AbsVec.x, max(AbsVec.y, AbsVec.z));

	const float n = 1.0;

	float NormZComp = (f + n) / (f - n) - 2 * f*n / (Z* (f - n));

	return ((NormZComp + 1.0) * 0.5) - DEPTH_MAX_ERROR;
}

float getShadowValue(vec4 light)
{
	float distance = getLightDepth(light.xyz, light.w);
	return pcfShadow(u_ShadowMap2, light.xyz, distance);
}
#endif

#if defined(USE_CUBEMAP)
vec3 CalcSHColor(in vec3 normal)
{
	float Y00	= 0.282095;
	float Y11	= 0.488603 * normal.x;
	float Y10	= 0.488603 * normal.z;
	float Y1_1	= 0.488603 * normal.y;
	float Y21	= 1.092548 * normal.x * normal.z;
	float Y2_1	= 1.092548 * normal.y * normal.z;
	float Y2_2	= 1.092548 * normal.y * normal.x;
	float Y20	= 0.946176 * normal.z * normal.z - 0.315392;
	float Y22	= 0.546274 * (normal.x * normal.x - normal.y * normal.y);

	float A0 = M_PI;
	float A1 = (2.0/3.0) * M_PI;
	float A2 = M_PI / 4.0;

	vec3 L00	= u_SphericalHarmonic[0];
	vec3 L11	= u_SphericalHarmonic[1];
	vec3 L10	= u_SphericalHarmonic[2];
	vec3 L1_1	= u_SphericalHarmonic[3];
	vec3 L21	= u_SphericalHarmonic[4];
	vec3 L2_1	= u_SphericalHarmonic[5];
	vec3 L2_2	= u_SphericalHarmonic[6];
	vec3 L20	= u_SphericalHarmonic[7];
	vec3 L22	= u_SphericalHarmonic[8];

	vec3 color = A0*Y00*L00
		+ A1*Y1_1*L1_1 + A1*Y10*L10 + A1*Y11*L11
		+ A2*Y2_2*L2_2 + A2*Y2_1*L2_1 + A2*Y20*L20 + A2*Y21*L21 + A2*Y22*L22;

	return color;
}
#endif

void main()
{
	vec3 viewDir, lightColor, ambientColor, reflectance, position;
	vec3 L, N, E;
	float attenuation;

#if defined(PER_PIXEL_LIGHTING)
	mat3 tangentToWorld = mat3(var_Tangent.xyz, var_Bitangent.xyz, var_Normal.xyz);
	viewDir = vec3(var_Normal.w, var_Tangent.w, var_Bitangent.w);
	E = normalize(viewDir);

	position = var_Position;
	ivec3 gridSize = textureSize(u_LightGridDirectionalLightMap, 0);
	vec3 invGridSize = vec3(1.0) / vec3(gridSize);
	vec3 gridCell = (position - u_LightGridOrigin) * u_LightGridCellInverseSize * invGridSize;
#endif

	vec2 texCoords = var_TexCoords.xy;
	vec4 diffuse;

#if defined(PER_PIXEL_LIGHTING)
	float isLightgrid = float(var_LightDir.w < 1.0);
	L = var_LightDir.xyz;
  #if defined(USE_DELUXEMAP)
	//L = mix(texture(u_LightGridDirectionMap, gridCell).rgb, texture(u_DeluxeMap, var_TexCoords.zw).xyz, u_EnableTextures.y) - vec3(0.5);
	L += (texture(u_DeluxeMap, var_TexCoords.zw).xyz - vec3(0.5)) * u_EnableTextures.y;
  #endif

	#if defined(USE_LIGHT_VECTOR)
	  L = texture(u_LightGridDirectionMap, gridCell).rgb - vec3(0.5) * isLightgrid;
	  vec3 directedLight = texture(u_LightGridDirectionalLightMap, gridCell).rgb * isLightgrid;
	  vec3 ambientLight = texture(u_LightGridAmbientLightMap, gridCell).rgb * isLightgrid;
	#endif

	float sqrLightDist = dot(L, L);
	L /= sqrt(sqrLightDist);

  #if defined(USE_LIGHTMAP)
    vec4 lightmapColor = texture(u_LightMap, var_TexCoords.zw);
	lightColor	 = lightmapColor.rgb * var_Color.rgb;
	ambientColor = vec3 (0.0);
	attenuation  = 1.0;
  #elif defined(USE_LIGHT_VECTOR)
	lightColor = directedLight * var_Color.rgb;
	ambientColor = ambientLight * var_Color.rgb;
	attenuation  = 1.0;
  #elif defined(USE_LIGHT_VERTEX)
	lightColor	 = var_Color.rgb;
	ambientColor = vec3 (0.0);
	attenuation  = 1.0;
  #endif

#if defined(USE_PARALLAXMAP)
	vec3 offsetDir = viewDir * tangentToWorld;

	offsetDir.xy *= -u_NormalScale.a / offsetDir.z;

	texCoords += offsetDir.xy * RayIntersectDisplaceMap(texCoords, offsetDir.xy, u_NormalMap);
#endif

	diffuse = texture(u_DiffuseMap, texCoords);
	diffuse.a *= var_Color.a;

	if (u_AlphaTestFunction == ATEST_CMP_GE){
		if (diffuse.a < u_AlphaTestValue)
			discard;
	}
	else if (u_AlphaTestFunction == ATEST_CMP_LT){
		if (diffuse.a >= u_AlphaTestValue)
			discard;
	}	
	else if (u_AlphaTestFunction == ATEST_CMP_GT){
		if (diffuse.a <= u_AlphaTestValue)
			discard;
	}

	N = CalcNormal(var_Normal.xyz, texCoords, tangentToWorld);

  #if defined(USE_SHADOWMAP) 
	
  #endif

  #if defined(USE_LIGHTMAP) || defined(USE_LIGHT_VERTEX)
	ambientColor = lightColor;
	float surfNL = clamp(dot(var_Normal.xyz, L), 0.0, 1.0);

	// Scale the incoming light to compensate for the baked-in light angle
	// attenuation.
	lightColor /= max(surfNL, 0.25);
	// Recover any unused light as ambient, in case attenuation is over 4x or
	// light is below the surface
	ambientColor = clamp(ambientColor - lightColor * surfNL, 0.0, 1.0);
  #endif

	lightColor *= M_PI;
	ambientColor *= M_PI;

  #if defined(USE_SPECULARMAP)
	vec4 specular = texture(u_SpecularMap, texCoords);
  #else
	vec4 specular = vec4(0.04);
  #endif
	specular *= u_SpecularScale;

	// energy conservation
	diffuse.rgb *= vec3(1.0) - specular.rgb;
	float roughness = mix(1.0, 0.01, specular.a);

    vec3  H  = normalize(L + E);
	float NE = abs(dot(N, E)) + 1e-5;
	float NL = clamp(dot(N, L), 0.0, 1.0);
	
	diffuse.rgb = CalcDiffuse(diffuse.rgb);
	reflectance = diffuse.rgb;

  #if (defined(USE_LIGHTMAP) || defined(USE_LIGHT_VERTEX))
	float NH = clamp(dot(N, H), 0.0, 1.0);
	float VH = clamp(dot(E, H), 0.0, 1.0);
	reflectance += CalcSpecular(specular.rgb, NH, NL, NE, VH, roughness);
  #endif
  #if defined(USE_LIGHT_VECTOR)
	float NH = clamp(dot(N, H), 0.0, 1.0);
	float VH = clamp(dot(E, H), 0.0, 1.0);
	reflectance += CalcSpecular(specular.rgb, NH, NL, NE, VH, roughness);
  #endif

	out_Color.rgb  = lightColor * reflectance * (attenuation * NL);
	if (u_EnableTextures.x == 0.0)
		out_Color.rgb += ambientColor * diffuse.rgb;

	ivec2 windowCoordinate = ivec2(gl_FragCoord.xy);
	vec3 diffuseBufferColor = texelFetch(u_ScreenDiffuseMap, windowCoordinate, 0).rgb;
	out_Color.rgb += diffuse.rgb * diffuseBufferColor;

	vec4 specBufferColor = texelFetch(u_ScreenSpecularMap, windowCoordinate, 0);
	out_Color.rgb += specBufferColor.rgb;

  #if defined(USE_CUBEMAP)
	NE = abs(dot(N, E)) + 1e-5;
	vec3 EnvBRDF = texture(u_EnvBrdfMap, vec2(roughness, NE)).rgb;

	vec3 R = reflect(E, N);

	// parallax corrected cubemap (cheaper trick)
	// from http://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/
	vec3 parallax = u_CubeMapInfo.xyz + u_CubeMapInfo.w * viewDir;

	#if defined(EQUIRECTANGULAR_CUBEMAPS)
		vec3 rayDirection = normalize(R + parallax);
		vec2 cubeTC = vec2((atan(rayDirection.y, rayDirection.x) / 6.283185307179586476925286766559), acos(rayDirection.z) / 3.1415926535897932384626433832795);
		vec3 cubeLightColor = textureLod(u_CubeMap, cubeTC, ROUGHNESS_MIPS * roughness).rgb * u_EnableTextures.w;
	#else
		vec3 cubeLightColor = textureLod(u_CubeMap, R + parallax, ROUGHNESS_MIPS * roughness).rgb * u_EnableTextures.w;
	#endif
	vec3 shColor = CalcSHColor(-N) * u_EnableTextures.x;

	float horiz = 1.0;
	// from http://marmosetco.tumblr.com/post/81245981087
	#if defined(HORIZON_FADE)
		horiz = clamp( 1.0 + HORIZON_FADE * dot(-R,var_Normal.xyz), 0.0, 1.0 );
		horiz *= horiz;
	#endif

	if (u_EnableTextures.x == 1.0)
		out_Color.rgb += shColor * diffuse.rgb;

	out_Color.rgb += cubeLightColor * specBufferColor.a * (specular.rgb * EnvBRDF.x + EnvBRDF.y) * horiz;

  #endif

  #if defined(USE_PRIMARY_LIGHT) || defined(SHADOWMAP_MODULATE)
	vec3  L2   = normalize(var_PrimaryLightDir.xyz);
	vec3  H2   = normalize(L2 + E);
	float NL2  = clamp(dot(N,  L2), 0.0, 1.0);
	float NH2  = clamp(dot(N,  H2), 0.0, 1.0);
	float VH2  = clamp(dot(E, H), 0.0, 1.0);
	
	reflectance  = CalcSpecular(specular.rgb, NH2, NL2, NE, VH2, roughness);
	reflectance += diffuse.rgb;

	lightColor = u_PrimaryLightColor;

    #if defined(USE_SHADOWMAP)
	float shadowValue = texelFetch(u_ShadowMap, windowCoordinate, 0).r;
	lightColor *= shadowValue;
    #endif

	out_Color.rgb += lightColor * reflectance * NL2;
  #endif

  #if defined(USE_DEBUG)
	if (USE_DEBUG == 1)
		out_Color.rgb = diffuse.rgb;
	if (USE_DEBUG == 2)
		out_Color.rgb = specular.rgb;
	if (USE_DEBUG == 3)
		out_Color.rgb = N.rgb * 0.5 + 0.5;
	#if defined(USE_CUBEMAP)
	if (USE_DEBUG == 4)
		out_Color.rgb = cubeLightColor;
	#endif
  #endif

#else
	diffuse = texture(u_DiffuseMap, texCoords);

	if (u_AlphaTestFunction == ATEST_CMP_GE){
		if (diffuse.a < u_AlphaTestValue)
			discard;
	}
	else if (u_AlphaTestFunction == ATEST_CMP_LT){
		if (diffuse.a >= u_AlphaTestValue)
			discard;
	}	
	else if (u_AlphaTestFunction == ATEST_CMP_GT){
		if (diffuse.a <= u_AlphaTestValue)
			discard;
	}

	lightColor = var_Color.rgb;
	out_Color.rgb = diffuse.rgb * lightColor;
#endif

	out_Color.a = diffuse.a;

#if defined(USE_GLOW_BUFFER)
	out_Glow = out_Color;
#else
	out_Glow = vec4(vec3(0.0), out_Color.a);
#endif
}