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

		mat4 boneMatrix = mat4(
			vec4(u_BoneMatrices[boneIndex][0], 0.0),
			vec4(u_BoneMatrices[boneIndex][1], 0.0),
			vec4(u_BoneMatrices[boneIndex][2], 0.0),
			vec4(u_BoneMatrices[boneIndex][3], 1.0)
		);

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
	vec3 L = u_LightOrigin.xyz - (position * u_LightOrigin.w);
#elif defined(PER_PIXEL_LIGHTING)
	vec3 L = attr_LightDirection * 2.0 - vec3(1.0);
	L = (u_ModelMatrix * vec4(L, 0.0)).xyz;
#endif

#if defined(USE_LIGHTMAP)
	var_TexCoords.zw = GenTexCoords(u_TCGen1, vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0));
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

#if defined(USE_LIGHT_VECTOR) && defined(USE_FAST_LIGHT)
		float lightDist = length(L);
		float attenuation = CalcLightAttenuation(lightDist, u_LightRadius);
		float NL = clamp(dot(normalize(normal), L) / lightDist, 0.0, 1.0);

		var_Color.rgb *= u_DirectedLight * (u_LightRadius + float(u_LightRadius < 1.0)) * (attenuation * NL) + u_AmbientLight;
#endif
	}

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
uniform samplerCube u_CubeMap;
#endif

#if defined(USE_NORMALMAP) || defined(USE_DELUXEMAP) || defined(USE_SPECULARMAP) || defined(USE_CUBEMAP)
// x = sphericalHarmonics, y = deluxe, z = specular, w = cube
uniform vec4      u_EnableTextures; 
#endif

#if defined(USE_LIGHT_VECTOR) && !defined(USE_FAST_LIGHT)
uniform vec3 u_DirectedLight;
uniform vec3 u_AmbientLight;
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

#if defined(USE_ATEST)
uniform float u_AlphaTestValue;
#endif

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

vec3 EnvironmentBRDF(float roughness, float NE, vec3 specular)
{
	// from http://community.arm.com/servlet/JiveServlet/download/96891546-19496/siggraph2015-mmg-renaldas-slides.pdf
	float v = 1.0 - max(roughness, NE);
	v *= v * v;
	return vec3(v) + specular;
}

float spec_D(
  float NH,
  float roughness)
{
  // normal distribution
  // from http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
  float alpha = roughness * roughness;
  float quotient = alpha / max(1e-8,(NH*NH*(alpha*alpha-1.0)+1.0));
  return (quotient * quotient) / M_PI;
}

vec3 spec_F(
  float EH,
  vec3 F0)
{
  // Fresnel
  // from http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
  float pow2 = pow(2.0, (-5.55473*EH - 6.98316) * EH);
  return F0 + (vec3(1.0) - F0) * pow2;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
} 

float G1(
  float NV,
  float k)
{
  return NV / (NV*(1.0-k) +  k);
}

float spec_G(float NL, float NE, float roughness )
{
  // GXX Schlick
  // from http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
  float k = max(((roughness + 1.0) * (roughness + 1.0)) / 8.0, 1e-5);
  return G1(NL,k)*G1(NE,k);
}

vec3 CalcDiffuse(vec3 diffuseAlbedo, float NH, float EH, float roughness)
{
#if defined(USE_BURLEY)
	// modified from https://disney-animation.s3.amazonaws.com/library/s2012_pbs_disney_brdf_notes_v2.pdf
	float fd90 = -0.5 + EH * EH * roughness;
	float burley = 1.0 + fd90 * 0.04 / NH;
	burley *= burley;
	return diffuseAlbedo * burley;
#else
	return diffuseAlbedo;
#endif
}

vec3 CalcSpecular(
	in vec3 specular,
	in float NH,
	in float NL,
	in float NE,
	in float EH,
	in float roughness
)
{
	float distrib = spec_D(NH,roughness);
	vec3 fresnel = spec_F(EH,specular);
	float vis = spec_G(NL, NE, roughness);
	float denominator = max((4.0 * max(NE,0.0) * max(NL,0.0)),0.001);
	return (distrib * fresnel * vis) / denominator;
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
	vec3 viewDir, lightColor, ambientColor, reflectance, vertexColor, position;
	vec3 L, N, E, H;
	float NL, NH, NE, EH, attenuation;

#if defined(PER_PIXEL_LIGHTING)
	mat3 tangentToWorld = mat3(var_Tangent.xyz, var_Bitangent.xyz, var_Normal.xyz);
	viewDir = vec3(var_Normal.w, var_Tangent.w, var_Bitangent.w);
	E = normalize(viewDir);

	position = var_Position;
	ivec3 gridSize = textureSize(u_LightGridDirectionalLightMap, 0);
	vec3 invGridSize = vec3(1.0) / vec3(gridSize);
	vec3 gridCell = (position - u_LightGridOrigin) * u_LightGridCellInverseSize * invGridSize;
#endif

#if defined(USE_LIGHTMAP)
	vec4 lightmapColor = texture(u_LightMap, var_TexCoords.zw);
  #if defined(RGBM_LIGHTMAP)
	lightmapColor.rgb *= lightmapColor.a;
  #endif
  #if defined(USE_PBR) && !defined(USE_FAST_LIGHT)
	lightmapColor.rgb *= lightmapColor.rgb;
  #endif
#endif

	vec2 texCoords = var_TexCoords.xy;

#if defined(USE_PARALLAXMAP)
	vec3 offsetDir = viewDir * tangentToWorld;

	offsetDir.xy *= -u_NormalScale.a / offsetDir.z;

	texCoords += offsetDir.xy * RayIntersectDisplaceMap(texCoords, offsetDir.xy, u_NormalMap);
#endif

	vec4 diffuse = texture(u_DiffuseMap, texCoords);
#if defined(USE_ATEST)
#  if USE_ATEST == ATEST_CMP_LT
	if (diffuse.a >= u_AlphaTestValue)
#  elif USE_ATEST == ATEST_CMP_GT
	if (diffuse.a <= u_AlphaTestValue)
#  elif USE_ATEST == ATEST_CMP_GE
	if (diffuse.a < u_AlphaTestValue)
#  endif
		discard;
#endif

#if defined(PER_PIXEL_LIGHTING)
	float isLightgrid = float(var_LightDir.w < 1.0);
	L = var_LightDir.xyz;
  #if defined(USE_LIGHT_VERTEX)
	L = -normalize(texture(u_LightGridDirectionMap, gridCell).rgb * 2.0 - vec3(1.0)) * (1.0 - u_EnableTextures.y);
  #endif
  #if defined(USE_DELUXEMAP)
	L = -normalize(texture(u_LightGridDirectionMap, gridCell).rgb  * 2.0 - vec3(1.0)) * (1.0 - u_EnableTextures.y);
	L += (texture(u_DeluxeMap, var_TexCoords.zw).xyz - vec3(0.5)) * u_EnableTextures.y;
  #endif
	float lightDist = length(L);
	L /= lightDist;

	vec3 ambientLight = texture(u_LightGridAmbientLightMap, gridCell).rgb * isLightgrid;
  #if defined(USE_PBR)
	vertexColor = var_Color.rgb * var_Color.rgb;
	ambientLight *= ambientLight;
	#if defined(USE_LIGHT_VECTOR)
	  L -= normalize(texture(u_LightGridDirectionMap, gridCell).rgb * 2.0 - vec3(1.0)) * isLightgrid;
	  vec3 directedLight = texture(u_LightGridDirectionalLightMap, gridCell).rgb * isLightgrid;
	  directedLight *= directedLight;
	  directedLight += u_DirectedLight * u_DirectedLight;
	#endif
  #else
	vertexColor = var_Color.rgb;
	#if defined(USE_LIGHT_VECTOR)
	  L -= normalize(texture(u_LightGridDirectionMap, gridCell).rgb * 2.0 - vec3(1.0)) * isLightgrid;
	  vec3 directedLight = texture(u_LightGridDirectionalLightMap, gridCell).rgb * isLightgrid;
	  directedLight += u_DirectedLight;
	#endif
  #endif
	ambientColor = ambientLight * vertexColor;
  #if defined(USE_LIGHTMAP)
	lightColor	 = lightmapColor.rgb * vertexColor;
	attenuation  = 1.0;
  #elif defined(USE_LIGHT_VECTOR)
	lightColor = directedLight * vertexColor * (var_LightDir.w + float(var_LightDir.w < 1.0));
	attenuation  = CalcLightAttenuation(lightDist, var_LightDir.w);
  #elif defined(USE_LIGHT_VERTEX)
	lightColor	 = vertexColor;
	attenuation  = 1.0;
  #endif

	N = CalcNormal(var_Normal.xyz, texCoords, tangentToWorld);

  #if defined(USE_SHADOWMAP) 
	vec2 shadowTex = gl_FragCoord.xy * r_FBufScale;
	float shadowValue = texture(u_ShadowMap, shadowTex).r;

	// surfaces not facing the light are always shadowed
	shadowValue *= clamp(dot(N, var_PrimaryLightDir.xyz), 0.0, 1.0);

    #if defined(SHADOWMAP_MODULATE)
	lightColor *= shadowValue * (1.0 - u_PrimaryLightAmbient.r) + u_PrimaryLightAmbient.r;
    #endif
  #endif

  #if defined(USE_LIGHTMAP) || defined(USE_LIGHT_VERTEX)
	float surfNL = clamp(dot(N, L), 0.0, 1.0) * 0.25;
	ambientColor = max(lightColor - lightColor * surfNL, vec3(0.0));
  #endif

  #if defined(USE_SPECULARMAP)
	vec4 specular = texture(u_SpecularMap, texCoords);
  #else
	vec4 specular = vec4(1.0);
  #endif
	specular *= u_SpecularScale;

#if defined(USE_PBR)
	diffuse.rgb *= diffuse.rgb;
	specular.rgb *= specular.rgb;
	// energy conservation, requires specular workflow to use an albedo texture too
	diffuse.rgb *= vec3(1.0) - specular.rgb;
#endif

	// diffuse rgb is diffuse
	// specular rgb is specular reflectance at normal incidence
	// specular alpha is gloss
	float roughness = 1.0 - specular.a;

    H  = normalize(L + E);
    EH = max(1e-8, dot(E, H));
	NH = max(1e-8, dot(N, H));
	NL = clamp(dot(N, L), 1e-8, 1.0);
	
	reflectance = CalcDiffuse(diffuse.rgb, NH, EH, roughness);

  #if defined(USE_LIGHTMAP) || defined(USE_LIGHT_VERTEX)
	NE = abs(dot(N, E)) + 1e-5;
	reflectance += CalcSpecular(specular.rgb, NH, NL, NE, EH, roughness) * 1.0;
  #endif
  #if defined(USE_LIGHT_VECTOR)
	NE = abs(dot(N, E)) + 1e-5;
	reflectance += CalcSpecular(specular.rgb, NH, NL, NE, EH, roughness);
  #endif

	out_Color.rgb  = lightColor   * reflectance * (attenuation * NL);
	if (u_EnableTextures.x == 0.0)
		out_Color.rgb += ambientColor * diffuse.rgb;

  #if defined(USE_CUBEMAP)
	NE = clamp(dot(N, E), 0.0, 1.0);
	vec3 EnvBRDF = texture(u_EnvBrdfMap, vec2(roughness, NE)).rgb;

	vec3 R = reflect(E, N);

	// parallax corrected cubemap (cheaper trick)
	// from http://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/
	vec3 parallax = u_CubeMapInfo.xyz + u_CubeMapInfo.w * viewDir;

	vec3 cubeLightColor = textureLod(u_CubeMap, R + parallax, ROUGHNESS_MIPS * roughness).rgb * u_EnableTextures.w;
	vec3 shColor = CalcSHColor(-N) * u_EnableTextures.x;

	float horiz = 1.0;
	// from http://marmosetco.tumblr.com/post/81245981087
	#if defined(HORIZON_FADE)
		const float horizonFade = HORIZON_FADE;
		horiz = clamp( 1.0 + horizonFade * dot(-R,var_Normal.xyz), 0.0, 1.0 );
		horiz *= horiz;
	#endif

    #if defined(USE_PBR)
		cubeLightColor *= cubeLightColor;
    #endif
	if (u_EnableTextures.x == 1.0)
		out_Color.rgb += shColor * diffuse.rgb;
	out_Color.rgb += cubeLightColor * (specular.rgb * EnvBRDF.x + EnvBRDF.y) * horiz;
  #endif

  #if defined(USE_PRIMARY_LIGHT) || defined(SHADOWMAP_MODULATE)
	vec3 L2, H2;
	float NL2, EH2, NH2, L2H2;

	L2 = var_PrimaryLightDir.xyz;

	H2  = normalize(L2 + E);
    NL2 = clamp(dot(N, L2), 0.0, 1.0);
    NL2 = max(1e-8, abs(NL2) );
    EH2 = max(1e-8, dot(E, H2));
    NH2 = max(1e-8, dot(N, H2));
	
	reflectance  = CalcSpecular(specular.rgb, NH2, NL2, NE, EH2, roughness);

	// bit of a hack, with modulated shadowmaps, ignore diffuse
    #if !defined(SHADOWMAP_MODULATE)
	reflectance += CalcDiffuse(diffuse.rgb, NH2, EH2, roughness);
    #endif

	lightColor = u_PrimaryLightColor * var_Color.rgb;

    #if defined(USE_SHADOWMAP)
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
  #endif

  #if defined(USE_PBR)
	out_Color.rgb = sqrt(out_Color.rgb);
  #endif

#else
	lightColor = var_Color.rgb;
	out_Color.rgb = diffuse.rgb * lightColor;
#endif

	out_Color.a = diffuse.a * var_Color.a;

#if defined(USE_GLOW_BUFFER)
	out_Glow = out_Color;
#else
	out_Glow = vec4(0.0);
#endif
}