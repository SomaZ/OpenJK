/*[Vertex]*/
#if defined(LIGHT_POINT)

uniform vec4 u_DlightTransforms[16]; // xyz = position, w = scale
uniform vec3 u_DlightColors[16];
uniform mat4 u_ModelViewProjectionMatrix;
uniform vec3 u_ViewOrigin;

in vec3 in_Position;

flat out vec4 var_LightPosition;
flat out vec3 var_LightColor;

#elif defined(LIGHT_GRID)



in vec4 attr_Position;

#endif
uniform vec3 u_ViewForward;
uniform vec3 u_ViewLeft;
uniform vec3 u_ViewUp;


out vec3 var_ViewDir;

void main()
{
#if defined(LIGHT_POINT)

	var_LightPosition = u_DlightTransforms[gl_InstanceID];;
	var_LightColor = u_DlightColors[gl_InstanceID];

	const vec2 positions[] = vec2[4](
		vec2(-1.0, -1.0),
		vec2(-1.0, 1.0),
		vec2(1.0, 1.0),
		vec2(1.0, -1.0)
		);

	vec2 position = positions[gl_VertexID];

	var_ViewDir = (u_ViewForward + u_ViewLeft * -position.x) + u_ViewUp * position.y;
	gl_Position = vec4(position, 0.0, 1.0);

#elif defined(LIGHT_GRID)

	const vec2 positions[] = vec2[4](
		vec2(-1.0, -1.0),
		vec2(-1.0,  1.0),
		vec2( 1.0,  1.0),
		vec2( 1.0, -1.0)
	);
	
	vec2 position = positions[gl_VertexID];
	
	var_ViewDir = (u_ViewForward + u_ViewLeft * -position.x) + u_ViewUp * position.y;
	gl_Position = vec4(position, 0.0, 1.0);
#endif
}

/*[Fragment]*/

uniform vec3 u_ViewOrigin;
uniform vec4 u_ViewInfo; // zfar / znear, zfar
uniform sampler2D u_ScreenDepthMap;
uniform sampler2D u_ScreenNormalMap;
uniform sampler2D u_ScreenSpecularAndGlossMap;
uniform sampler2D u_ScreenDiffuseMap;

#if defined(LIGHT_GRID)
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

#define u_ZFarDivZNear u_ViewInfo.x
#define u_ZFar u_ViewInfo.y

in vec3 var_ViewDir;
#if defined(LIGHT_POINT)
flat in vec4 var_LightPosition;
flat in vec3 var_LightColor;
#endif

out vec4 out_Color;

float LinearDepth(float zBufferDepth, float zFarDivZNear)
{
	return 1.0 / mix(zFarDivZNear, 1.0, zBufferDepth);
}

vec3 DecodeNormal(in vec2 N)
{
	vec2 encoded = N*4.0 - 2.0;
	float f = dot(encoded, encoded);
	float g = sqrt(1.0 - f * 0.25);

	return vec3(encoded * g, 1.0 - f * 0.5);
}

#if defined(LIGHT_POINT) || defined(LIGHT_GRID)

float spec_D(
	float NH,
	float roughness)
{
	// normal distribution
	// from http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
	float alpha = roughness * roughness;
	float quotient = alpha / max(1e-8, (NH*NH*(alpha*alpha - 1.0) + 1.0));
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
	return NV / (NV*(1.0 - k) + k);
}

float spec_G(float NL, float NE, float roughness)
{
	// GXX Schlick
	// from http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
	float k = max(((roughness + 1.0) * (roughness + 1.0)) / 8.0, 1e-5);
	return G1(NL, k)*G1(NE, k);
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
	float distrib = spec_D(NH, roughness);
	vec3 fresnel = spec_F(EH, specular);
	float vis = spec_G(NL, NE, roughness);
	return (distrib * fresnel * vis);
}

float CalcLightAttenuation(float sqrDistance, float radius)
{
	float sqrRadius = radius * radius;
	float d = (sqrDistance*sqrDistance) / (sqrRadius*sqrRadius);
	float attenuation = clamp(1.0 - d, 0.0, 1.0);
	attenuation *= attenuation;
	attenuation /= max(sqrDistance, 0.0001);
	attenuation *= radius;
	return clamp(attenuation, 0.0, 1.0);
}

#endif

void main()
{
	vec3 E, H;
	float NL, NH, NE, EH;

	ivec2 windowCoord = ivec2(gl_FragCoord.xy);
	vec4 specularAndGloss = texelFetch(u_ScreenSpecularAndGlossMap, windowCoord, 0);
	float roughness = 1.0 - specularAndGloss.a;
	specularAndGloss *= specularAndGloss;
	vec3 albedo = texelFetch(u_ScreenDiffuseMap, windowCoord, 0).rgb;
	vec2 normal = texelFetch(u_ScreenNormalMap, windowCoord, 0).rg;
	float depth = texelFetch(u_ScreenDepthMap, windowCoord, 0).r;

	float linearDepth = LinearDepth(depth, u_ZFarDivZNear);
	vec3 N = normalize(DecodeNormal(normal));

	vec3 position, result;

#if defined(LIGHT_POINT)

	position			= u_ViewOrigin + var_ViewDir * linearDepth;
	vec3 L				= var_LightPosition.xyz - position;
	float sqrLightDist	= dot(L,L);
	L				   /= sqrt(sqrLightDist);

	float attenuation;

	attenuation = CalcLightAttenuation(sqrLightDist, var_LightPosition.w);

	E = normalize(-var_ViewDir);
	H = normalize(L + E);
	EH = max(1e-8, dot(E, H));
	NH = max(1e-8, dot(N, H));
	NL = clamp(dot(N, L), 1e-8, 1.0);
	NE = abs(dot(N, E)) + 1e-5;

	vec3 reflectance = CalcDiffuse(albedo.rgb, NH, EH, roughness);
	reflectance += CalcSpecular(specularAndGloss.rgb, NH, NL, NE, EH, roughness);

	result = sqrt(var_LightColor * var_LightColor * reflectance * (attenuation * NL));

#elif defined(LIGHT_GRID)

	position = u_ViewOrigin + var_ViewDir * linearDepth;

  #if 1
	ivec3 gridSize = textureSize(u_LightGridDirectionalLightMap, 0);
	vec3 invGridSize = vec3(1.0) / vec3(gridSize);
	vec3 gridCell = (position - u_LightGridOrigin) * u_LightGridCellInverseSize * invGridSize;
	vec3 lightDirection = texture(u_LightGridDirectionMap, gridCell).rgb * 2.0 - vec3(1.0);
	vec3 directionalLight = texture(u_LightGridDirectionalLightMap, gridCell).rgb;
	vec3 ambientLight = texture(u_LightGridAmbientLightMap, gridCell).rgb;

	directionalLight *= directionalLight;
	ambientLight *= ambientLight;

	vec3 L = normalize(-lightDirection);
	float NdotL = clamp(dot(N, L), 0.0, 1.0);

	vec3 reflectance = 2.0 * u_LightGridDirectionalScale * (NdotL * directionalLight) +
		(u_LightGridAmbientScale * ambientLight);
	reflectance *= albedo;

	E = normalize(-var_ViewDir);
	H = normalize(L + E);
	EH = max(1e-8, dot(E, H));
	NH = max(1e-8, dot(N, H));
	NL = clamp(dot(N, L), 1e-8, 1.0);
	NE = abs(dot(N, E)) + 1e-5;

	reflectance += CalcSpecular(specularAndGloss.rgb, NH, NL, NE, EH, roughness);

	result = sqrt(reflectance);
  #else
	// Ray marching debug visualisation
	ivec3 gridSize = textureSize(u_LightGridDirectionalLightMap, 0);
	vec3 invGridSize = vec3(1.0) / vec3(gridSize);
	vec3 samplePosition = invGridSize * (u_ViewOrigin - u_LightGridOrigin) * u_LightGridCellInverseSize;
	vec3 stepSize = 0.5 * normalize(var_ViewDir) * invGridSize;
	float stepDistance = length(0.5 * u_LightGridCellInverseSize);
	float maxDistance = linearDepth;
	vec4 accum = vec4(0.0);
	float d = 0.0;

	for ( int i = 0; d < maxDistance && i < 50; i++ )
	{
		vec3 ambientLight = texture(u_LightGridAmbientLightMap, samplePosition).rgb;
		ambientLight *= 0.05;

		accum = (1.0 - accum.a) * vec4(ambientLight, 0.05) + accum;

		if ( accum.a > 0.98 )
		{
			break;
		}

		samplePosition += stepSize;
		d += stepDistance;

		if ( samplePosition.x < 0.0 || samplePosition.y < 0.0 || samplePosition.z < 0.0 ||
			samplePosition.x > 1.0 || samplePosition.y > 1.0 || samplePosition.z > 1.0 )
		{
			break;
		}
	}

	result = accum.rgb * 0.8;
  #endif
#endif
	
	out_Color = vec4(result, 1.0);
}