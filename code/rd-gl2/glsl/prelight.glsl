/*[Vertex]*/
#if defined(POINT_LIGHT) || defined(CUBEMAP) 
#define USE_VOLUME_SPHERE
#endif

#if defined(USE_VOLUME_SPHERE)
in vec3 in_Position;
uniform mat4 u_ModelViewProjectionMatrix;
uniform vec3 u_ViewOrigin;
#endif

#if defined(POINT_LIGHT)
uniform vec4 u_LightTransforms[32]; // xyz = position, w = scale
uniform vec3 u_LightColors[32];
flat out vec4 var_Position;
flat out vec3 var_LightColor;
#endif

#if defined(CUBEMAP)
uniform vec4 u_CubemapTransforms[32]; // xyz = position, w = scale
flat out vec4 var_Position;
flat out int  var_Index;
#endif

uniform vec3 u_ViewForward;
uniform vec3 u_ViewLeft;
uniform vec3 u_ViewUp;
uniform int  u_VertOffset;

out vec3 var_ViewDir;
flat out int var_Instance;

void main()
{
	var_Instance			= gl_InstanceID;
#if defined(POINT_LIGHT)
	var_Position			= u_LightTransforms[gl_InstanceID + u_VertOffset];
	var_LightColor			= u_LightColors[gl_InstanceID + u_VertOffset];
	var_LightColor			*= var_LightColor;
	var_LightColor			*= var_Position.w;
#elif defined(CUBEMAP)
	var_Index				= gl_InstanceID + u_VertOffset;
	var_Position			= u_CubemapTransforms[gl_InstanceID + u_VertOffset];
#endif

#if defined(USE_VOLUME_SPHERE)
	vec3 worldSpacePosition = in_Position * var_Position.w * 1.1 + var_Position.xyz;
	gl_Position				= u_ModelViewProjectionMatrix * vec4(worldSpacePosition, 1.0);
	var_ViewDir				= normalize(worldSpacePosition - u_ViewOrigin);
#else
	vec2 position			= vec2(2.0 * float(gl_VertexID & 2) - 1.0, 4.0 * float(gl_VertexID & 1) - 1.0);
	gl_Position				= vec4(position, 0.0, 1.0);
	var_ViewDir				= (u_ViewForward + u_ViewLeft * -position.x) + u_ViewUp * position.y;
#endif
}

/*[Fragment]*/
#if defined(POINT_LIGHT) || defined(CUBEMAP) 
#define USE_VOLUME_SPHERE
#endif

#if defined(TWO_RAYS_PER_PIXEL)
#define brdfBias 0.6
#else
#define brdfBias 0.8
#endif

uniform vec3 u_ViewOrigin;
uniform vec4 u_ViewInfo;
uniform sampler2D u_ScreenImageMap;		// 0 
uniform sampler2D u_ScreenDepthMap;		// 1
uniform sampler2D u_NormalMap;			// 2
uniform sampler2D u_SpecularMap;		// 3
uniform sampler2D u_ScreenOffsetMap;	// 4
uniform sampler2D u_ScreenOffsetMap2;   // 5
uniform sampler2D u_EnvBrdfMap;			// 7
uniform sampler2D u_RandomMap;			// 11

#if defined(TEMPORAL_FILTER) || defined(SSR_RESOLVE) || defined(SSR)
uniform sampler2D u_ShadowMap;
#endif

uniform mat4 u_ModelMatrix;
uniform mat4 u_ModelViewProjectionMatrix;
uniform mat4 u_NormalMatrix;
uniform mat4 u_InvViewProjectionMatrix;

#if defined(POINT_LIGHT)
uniform sampler3D u_LightGridDirectionMap;
uniform sampler3D u_LightGridDirectionalLightMap;
uniform sampler3D u_LightGridAmbientLightMap;
uniform vec3 u_LightGridOrigin;
uniform vec3 u_LightGridCellInverseSize;
uniform vec3 u_StyleColor;
uniform vec2 u_LightGridLightScale;
uniform vec3 u_ViewForward;
uniform vec3 u_ViewLeft;
uniform vec3 u_ViewUp;
uniform int u_VertOffset;

uniform samplerCubeShadow u_ShadowMap;
uniform samplerCubeShadow u_ShadowMap2;
uniform samplerCubeShadow u_ShadowMap3;
uniform samplerCubeShadow u_ShadowMap4;

#define u_LightGridAmbientScale u_LightGridLightScale.x
#define u_LightGridDirectionalScale u_LightGridLightScale.y
#endif

#if defined(SUN_LIGHT)
uniform vec3 u_ViewForward;
uniform vec3 u_ViewLeft;
uniform vec3 u_ViewUp;
uniform vec4 u_PrimaryLightOrigin;
uniform vec3 u_PrimaryLightColor;
uniform vec3 u_PrimaryLightAmbient;
uniform float u_PrimaryLightRadius;
uniform sampler2D u_ShadowMap;
#endif

#if defined(CUBEMAP)
uniform samplerCube u_ShadowMap;
uniform samplerCube u_ShadowMap2;
uniform samplerCube u_ShadowMap3;
uniform samplerCube u_ShadowMap4;
uniform vec4		u_CubeMapInfo;
uniform vec4		u_CubemapTransforms[32]; // xyz = position, w = scale
uniform int			u_NumCubemaps;
flat in int			var_Index;
#endif

in vec3 var_ViewDir;
flat in int  var_Instance;

#if defined(POINT_LIGHT)
in vec2 var_screenCoords;
flat in vec4 var_Position;
flat in vec3 var_LightColor;
#endif

out vec4 out_Color;
out vec4 out_Glow;

float linearDepth(in float depthSample, in float zNear, in float zFar)
{
	depthSample = 2.0 * depthSample - 1.0;
    float zLinear = 2.0 * zNear * zFar / (zFar + zNear - depthSample * (zFar - zNear));
    return zLinear;
}

float depthSample(in float linearDepth, in float zNear, in float zFar)
{
    float nonLinearDepth = (zFar + zNear - 2.0 * zNear * zFar / linearDepth) / (zFar - zNear);
    nonLinearDepth = (nonLinearDepth + 1.0) * 0.5;
    return nonLinearDepth;
}

vec3 WorldPosFromDepth(float depth, vec2 TexCoord) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(TexCoord * 2.0 - 1.0, z, 1.0);
    vec4 worldPosition = u_InvViewProjectionMatrix * clipSpacePosition;
	worldPosition = vec4((worldPosition.xyz / worldPosition.w ), 1.0f);

    return worldPosition.xyz;
}

vec3 DecodeNormal(in vec2 N)
{
	vec2 encoded = N*4.0 - 2.0;
	float f = dot(encoded, encoded);
	float g = sqrt(1.0 - f * 0.25);

	return vec3(encoded * g, 1.0 - f * 0.5);
}

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
#if defined(SSR_RESOLVE) || defined(SSR)
	float k = max(roughness * roughness / 2.0, 1e-5);
#else
	float k = max(((roughness + 1.0) * (roughness + 1.0)) / 8.0, 1e-5);
#endif
	return G1(NL, k)*G1(NE, k);
}

#if defined(SSR_RESOLVE) || defined(SSR)
float CalcSpecular(
#else
vec3 CalcSpecular(
#endif
	in vec3 specular,
	in float NH,
	in float NL,
	in float NE,
	in float EH,
	in float roughness
)
{
	float denominator = max((4.0 * max(NE,0.0) * max(NL,0.0)),0.001);
	float distrib = spec_D(NH,roughness);
	float vis = spec_G(NL, NE, roughness);
	#if defined(SSR_RESOLVE) || defined(SSR)
		return (distrib * vis) / denominator;
	#else
		vec3 fresnel = spec_F(EH,specular);
		return (distrib * fresnel * vis) / denominator;
	#endif
}

#if defined(POINT_LIGHT)

float CalcLightAttenuation(float distance, float radius)
{
	float d = pow(distance / radius, 4.0);
	float attenuation = clamp(1.0 - d, 0.0, 1.0);
	attenuation *= attenuation;
	attenuation /= distance * distance + 1.0;

	return clamp(attenuation, 0.0, 1.0);
}

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

	return ((NormZComp + 1.0) * 0.5) + DEPTH_MAX_ERROR;
}

float getShadowValue(vec4 light)
{
	float distance = getLightDepth(light.xyz, light.w);

	if (var_Instance == 0)
		return pcfShadow(u_ShadowMap, light.xyz, distance);
	if (var_Instance == 1)
		return pcfShadow(u_ShadowMap2, light.xyz, distance);
	if (var_Instance == 2)
		return pcfShadow(u_ShadowMap3, light.xyz, distance);
	else
		return pcfShadow(u_ShadowMap4, light.xyz, distance);
}
#endif

#if defined(CUBEMAP)

float getCubemapWeight(in vec3 position, in vec3 normal)
{
	float length1, length2, length3 = 10000000.0;
	float NDF1,NDF2,NDF3			= 10000000.0;
	int closest, secondclosest, thirdclosest = -1;

	for (int i = 0; i < 32; i++)
	{
		vec3 dPosition = position - u_CubemapTransforms[i].xyz;
		float length = length(dPosition);
		float NDF = clamp (length / u_CubemapTransforms[i].w, 0.0, 1.0);

		if (length < length1)
		{
			length3 = length2;
			length2 = length1;
			length1 = length;
			NDF3 = NDF2;
			NDF2 = NDF1;
			NDF1 = NDF;

			thirdclosest = secondclosest;
			secondclosest = closest;
			closest = i;
		}
		else if (length < length2)
		{
			length3 = length2;
			length2 = length;

			NDF3 = NDF2;
			NDF2 = NDF;

			thirdclosest = secondclosest;
			secondclosest = i;
		}
		else if (length < length3)
		{
			length3 = length;

			NDF3 = NDF;

			thirdclosest = i;
		}
	}

	if (length1 > u_CubemapTransforms[closest].w && var_Index == closest)
		return 1.0;

	//cubemap is not under the closest ones, discard
	if (var_Index != closest && var_Index != secondclosest && var_Index != thirdclosest)
		return 0.0;

	float num = 0.0;

	float SumNDF	= 0.0;
	float InvSumNDF = 0.0;

	float blendFactor1, blendFactor2, blendFactor3 = 0.0;
	float sumBlendFactor;

	if (closest != -1){
		SumNDF		+= NDF1;
		InvSumNDF	+= 1.0 - NDF1;
		num += 1.0;
	}
	if (secondclosest != -1){
		SumNDF		+= NDF2;
		InvSumNDF	+= 1.0 - NDF2;
		num += 1.0;
	}
	if (thirdclosest != -1){
		SumNDF		+= NDF1;
		InvSumNDF	+= 1.0 - NDF2;
		num += 1.0;
	}

	if (num >= 2)
	{
		if (closest != -1){
			blendFactor1  = (1.0 - (NDF1 / SumNDF)) / (num - 1.0);
			blendFactor1 *= ((1.0 - NDF1) / InvSumNDF);
			sumBlendFactor += blendFactor1;
		}
		if (secondclosest != -1){
			blendFactor2  = (1.0 - (NDF2 / SumNDF)) / (num - 1.0);
			blendFactor2 *= ((1.0 - NDF2) / InvSumNDF);
			sumBlendFactor += blendFactor2;
		}
		if (thirdclosest != -1){
			blendFactor3  = (1.0 - (NDF3 / SumNDF)) / (num - 1.0);
			blendFactor3 *= ((1.0 - NDF3) / InvSumNDF);
			sumBlendFactor += blendFactor3;
		}

		if (var_Index == closest)
			return blendFactor1 / sumBlendFactor;
		if (var_Index == secondclosest)
			return blendFactor2 / sumBlendFactor;
		if (var_Index == thirdclosest)
			return blendFactor3 / sumBlendFactor;
		return 0.0;
	}
	else
		return -1.0;
}

#endif

// from https://www.shadertoy.com/view/llGSzw
float hash( uint n ) { 
	n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 789221U) + 1376312589U;
    return float( n & uvec3(0x7fffffffU))/float(0x7fffffff);
}

float Noise(vec2 U, float x) {
	U += x;
    return hash(uint(U.x+r_FBufScale.x*U.y));
}

#if defined(SSR)

const vec3 BinarySearch(in vec3 dir, in vec3 hitCoord)
{
	float dDepth = 0.0;
    for(int i = 0; i < 14; i++)
    {
		dDepth = textureLod(u_ShadowMap, hitCoord.xy, 0).r * hitCoord.z;
        
		dir *= 0.5;
		if(dDepth >= 1.0)
			hitCoord += dir;
		else
			hitCoord -= dir;
    }
	float hitScore = mix (1.0, 0.0, abs((1.0 / hitCoord.z) - textureLod(u_ShadowMap, hitCoord.xy, 0).r) * 18.0) ;

	return vec3(hitCoord.xy, hitScore);
}

const vec3 RayCast(in vec3 dir, in vec3 hitCoord)
{
	vec4 dDepth = vec4(0.0);
	vec3 samplingPoints[4];
	samplingPoints[0] = hitCoord + dir;
	samplingPoints[1] = samplingPoints[0] + dir;
	samplingPoints[2] = samplingPoints[1] + dir;
	samplingPoints[3] = samplingPoints[2] + dir;
    for(int i = 0; i < 14; ++i) {
		
		dDepth.x = textureLod(u_ShadowMap, samplingPoints[0].xy, 0).r * samplingPoints[0].z;
		dDepth.y = textureLod(u_ShadowMap, samplingPoints[1].xy, 0).r * samplingPoints[1].z;
		dDepth.z = textureLod(u_ShadowMap, samplingPoints[2].xy, 0).r * samplingPoints[2].z;
		dDepth.w = textureLod(u_ShadowMap, samplingPoints[3].xy, 0).r * samplingPoints[3].z;

		if (dDepth.x < 1.0)
			return BinarySearch(dir, samplingPoints[0]);

		if (dDepth.y < 1.0)
			return BinarySearch(dir, samplingPoints[1]);

		if (dDepth.z < 1.0)
			return BinarySearch(dir, samplingPoints[2]);

		if (dDepth.w < 1.0)
			return BinarySearch(dir, samplingPoints[3]);

		samplingPoints[0] = samplingPoints[3] + dir;
		samplingPoints[1] = samplingPoints[0] + dir;
		samplingPoints[2] = samplingPoints[1] + dir;
		samplingPoints[3] = samplingPoints[2] + dir;

		if (samplingPoints[0].x < 0.0 || 
			samplingPoints[0].x > 1.0 || 
			samplingPoints[0].y < 0.0 || 
			samplingPoints[0].y > 1.0)
			break;
    }
    return vec3(samplingPoints[3].xy, 0.0);
}

vec4 ImportanceSampleGGX(vec2 Xi, float Roughness, vec3 N)
{
	float a = Roughness * Roughness;
	float a2 = a * a;

	float Phi = 2.0 * M_PI * Xi.x;
	float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
	float SinTheta = sqrt( 1.0 - CosTheta * CosTheta );

	vec3 H;
	H.x = SinTheta * cos( Phi );
	H.y = SinTheta * sin( Phi );
	H.z = CosTheta;

	vec3 UpVector = abs(N.z) < 0.999 ? vec3(0.0,0.0,1.0) : vec3(1.0,0.0,0.0);
	vec3 TangentX = normalize(cross(UpVector , N));
	vec3 TangentY = cross(N , TangentX);

	float d = (CosTheta * a2 - CosTheta) * CosTheta + 1.0;
	float D = a2 / (M_PI * d * d);
	float pdf = D * CosTheta;

	return vec4(TangentX * H.x + TangentY * H.y + N * H.z, pdf);
}

vec4 traceSSRRay(in float roughness, in vec3 wsNormal, in vec3 E, in vec3 viewPos, in vec3 scspPos, in int sample)
{
	float fade = 0.0;
	vec4 H;
	vec3 reflection;
	bool NdotR, VdotR;

	for (int i = 0; i < 3; i++) 
	{
		float x_shifted = gl_FragCoord.x + (sample+i)*128;
		vec2 sampleCoordinates = vec2(x_shifted, gl_FragCoord.y) / vec2(1024.0, 128.0);

		vec2 Xi = texture(u_RandomMap, sampleCoordinates).rg;
		Xi.y = mix(Xi.y, 0.0, brdfBias);

		H = ImportanceSampleGGX(Xi, roughness, wsNormal);
		reflection = reflect(-E, H.xyz);
		
		NdotR = dot(wsNormal, reflection) > 0.0;
		fade = min(2.0 * dot(-E, reflection), 1.0);
		VdotR = fade > 0.0;

		if (NdotR && VdotR)
			break;
	}

	if (!NdotR || !VdotR)
		return vec4(0.0);
	
	reflection = normalize(mat3(u_ModelViewProjectionMatrix) * reflection);
	reflection *= max(0.0125, -viewPos.z * 0.025) * (roughness * 2.0 + 1.0);

	vec4 scspRefPos = u_ModelMatrix * vec4(viewPos + reflection, 1.0);
	scspRefPos.xyz /= scspRefPos.w;
	scspRefPos.xyz = scspRefPos.xyz * 0.5 + 0.5;
	scspRefPos.z = 1.0 / linearDepth(scspRefPos.z, u_ViewInfo.x, u_ViewInfo.y);

	vec3 scspReflection = vec3(scspRefPos.xyz - scspPos.xyz);
	
	vec3 screenCoord = RayCast(scspReflection, scspPos.xyz).xyz;

	vec2 dCoords = smoothstep(0.35, 0.5, abs(vec2(0.5, 0.5) - screenCoord.xy));
	float screenEdgefactor = clamp(1.0 - (dCoords.x + dCoords.y), 0.0, 1.0);
	screenCoord.z *= screenEdgefactor;
	screenCoord.z *= clamp(fade, 0.0, 1.0);

	float pdf = H.w / (4.0 * dot(E,H.xyz) / dot(wsNormal, H.xyz)) + 0.0001;
	// return intersection, pdf and hitScore
	return vec4(screenCoord.xy, 1.0 / pdf, clamp(screenCoord.z, 0.0, 1.0));
}

#endif

float luma(vec3 color)
{
	return dot(color, vec3(0.299, 0.587, 0.114));
}
#if defined(SSR_RESOLVE)
vec4 resolveSSRRay(	in sampler2D packedTexture, 
					in ivec2 coordinate,
					in sampler2D velocityTexture, 
					in vec3 viewPos, 
					in vec3 viewNormal, 
					in float roughness, 
					inout float weightSum)
{
	const vec2 bufferScale = 2.0 / r_FBufInvScale;
	vec4 diffuseSample	= vec4(0.0);
	vec4 packedHitPos = texelFetch(packedTexture, coordinate, 0);
	
	float depth = textureLod(u_ScreenDepthMap, packedHitPos.xy , 1.0).r;
	vec3 hitViewPos = WorldPosFromDepth(depth, packedHitPos.xy);

	vec3 L  = normalize(hitViewPos - viewPos); 
	vec3 E  = normalize(-viewPos);
	vec3 H  = normalize(L + E);

	float NH = max(1e-8, dot(viewNormal, H));
	float NE = max(1e-8, dot(viewNormal, E));
	float NL = max(1e-8, dot(viewNormal, L));

	float weight = CalcSpecular(vec3(1.0), NH, NL, NE, 0.0, roughness) * packedHitPos.z * packedHitPos.a;

	//from http://iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf
	float coneCos = exp2(-3.32193 * roughness * roughness * roughness * roughness);
	float coneTangent = sqrt(1.0 - coneCos * coneCos) / coneCos;
	coneTangent *= mix(clamp(NE * 2.0, 0.0, 1.0), 1.0, roughness);

	float intersectionCircleRadius = coneTangent * distance(hitViewPos, viewPos) * brdfBias;
	float mip = clamp(log2( intersectionCircleRadius ), 0.0, 4.0);
	
	vec2 velocity = texture(velocityTexture, packedHitPos.xy).rg;
	diffuseSample = textureLod(u_ScreenImageMap, packedHitPos.xy - velocity, mip);

	diffuseSample.rgb *= diffuseSample.rgb;
	diffuseSample.a = packedHitPos.a;

	diffuseSample.rgb /= 1.0 + luma(diffuseSample.rgb);
	diffuseSample.rgb *= weight;
	weightSum += weight;

	return diffuseSample;
}
#endif
#define FLT_EPS 0.00000001f;

vec4 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec4 p, vec4 q)
{
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min) + FLT_EPS;

    vec4 v_clip = q - vec4(p_clip, p.w);
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0)
        return vec4(p_clip, p.w) + v_clip / ma_unit;
    else
        return q; // point inside aabb
}

void main()
{
	vec3 H;
	float NL, NH, NE, EH;
	float attenuation;
	vec4 diffuseOut = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 specularOut = vec4(0.0, 0.0, 0.0, 0.0);
	ivec2 windowCoord = ivec2(gl_FragCoord.xy);

#if defined(SSR)
	vec2 coord = windowCoord + 0.5;
	coord *= 2.0 * r_FBufInvScale;
	float depth = texture(u_ShadowMap, coord).r;

	if (depth < (u_ViewInfo.y - 0.1))
	{
	vec3 vsPosition = WorldPosFromDepth(depthSample(depth, u_ViewInfo.x, u_ViewInfo.y), coord);
#else
	vec2 coord = gl_FragCoord.xy * r_FBufInvScale;
	float depth = texture(u_ScreenDepthMap, coord).r;
	vec3 position = WorldPosFromDepth(depth, coord);
#endif

#if !defined(SSR) && !defined(SSR_RESOLVE)
	vec4 specularAndGloss = texture(u_SpecularMap, coord);
	specularAndGloss.rgb *= specularAndGloss.rgb;
#endif

	vec4 normal = texture(u_NormalMap, coord);
	float roughness = max(1.0 - normal.a, 0.02);

	//vec3 N = normalize(DecodeNormal(normal.rg));
	vec3 N = normalize(normal.rgb);
	vec3 E = normalize(-var_ViewDir);

#if defined(SSR)

	vec4 scspPos = u_ModelMatrix * vec4(vsPosition, 1.0);
	scspPos.xyz /= scspPos.w;
	scspPos.xyz = scspPos.xyz * 0.5 + 0.5;
	scspPos.z = 1.0 / linearDepth(scspPos.z, u_ViewInfo.x, u_ViewInfo.y);

	diffuseOut = traceSSRRay( roughness, N, E, vsPosition, scspPos.xyz, int(u_ViewInfo.z));

	#if defined(TWO_RAYS_PER_PIXEL)
		specularOut = traceSSRRay( roughness, N, E, vsPosition, scspPos.xyz, int(u_ViewInfo.z + 4));
	#endif
	}
#elif defined(SSR_RESOLVE)
	windowCoord = ivec2((gl_FragCoord.xy + 0.5) * 0.5); 
	const int samples = 4;
	float weightSum = 0.0;
	vec3 viewNormal = normalize(mat3(u_NormalMatrix) * N);
	vec3 viewPos = position;
	diffuseOut.a = 0.0;

	const ivec2 offset[12] = ivec2[12](
		ivec2(0.0, 0.0),
		ivec2(-1.0, 1.0),
		ivec2(0.0, 2.0),
		ivec2(2.0, -1.0),
		ivec2(0.0, 0.0),
		ivec2(0.0, 1.0),
		ivec2(-1.0, 0.0),
		ivec2(-2.0, -2.0),
		ivec2(0.0, 0.0),
		ivec2(2.0, 0.0),
		ivec2(1.0, 1.0),
		ivec2(0.0, -2.0)
	);

	for( int i = 0; i < samples; i++)
	{
		int index = int(mod(i + u_ViewInfo.z, 12.0));
		diffuseOut += resolveSSRRay(u_ScreenOffsetMap, windowCoord + offset[index], u_ShadowMap, viewPos, viewNormal, roughness, weightSum);

		#if defined(TWO_RAYS_PER_PIXEL)
			index = int(mod(i + 6 + u_ViewInfo.z, 12.0));
			diffuseOut += resolveSSRRay(u_ScreenOffsetMap2, windowCoord + offset[index], u_ShadowMap, viewPos, viewNormal, roughness, weightSum);
		#endif
	}

	diffuseOut.rgb /= weightSum;
	diffuseOut.rgb /= 1.0 - luma(diffuseOut.rgb);

	#if defined(TWO_RAYS_PER_PIXEL)
	diffuseOut.a /= 8.0;
	#else
	diffuseOut.a /= 4.0;
	#endif
	diffuseOut *= diffuseOut.a;

#elif defined(TEMPORAL_FILTER)
/*
Based on Playdead's TAA implementation
https://github.com/playdeadgames/temporal

The MIT License (MIT)

Copyright (c) [2015] [Playdead]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
	NE = abs(dot(N, E)) + 1e-5;
	vec3 EnvBRDF = texture(u_EnvBrdfMap, vec2(roughness, NE)).rgb;

	vec2 tc = gl_FragCoord.xy / r_FBufScale;

	const ivec2 du = ivec2(1.0, 0.0);
	const ivec2 dv = ivec2(0.0,	1.0);

	vec4 ctl = textureOffset(u_ScreenImageMap, tc.xy, - dv - du);
	vec4 ctc = textureOffset(u_ScreenImageMap, tc.xy, - dv);
	vec4 ctr = textureOffset(u_ScreenImageMap, tc.xy, - dv + du);
	vec4 cml = textureOffset(u_ScreenImageMap, tc.xy, - du);
	vec4 cmc = texture		(u_ScreenImageMap, tc.xy);
	vec4 cmr = textureOffset(u_ScreenImageMap, tc.xy, + du);
	vec4 cbl = textureOffset(u_ScreenImageMap, tc.xy, + dv - du);
	vec4 cbc = textureOffset(u_ScreenImageMap, tc.xy, + dv);
	vec4 cbr = textureOffset(u_ScreenImageMap, tc.xy, + dv + du);

	vec4 currentMin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
	vec4 currentMax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));

	vec4 center = (currentMin + currentMax) * 0.5;
	currentMin = (currentMin - center) * 6.0 + center;
	currentMax = (currentMax + center) * 6.0 - center;

	vec2 uvTraced = texture(u_ScreenOffsetMap, tc).xy;
	vec2 minVelocity = texture(u_ShadowMap, uvTraced).xy;

	#if defined(TWO_RAYS_PER_PIXEL)
	uvTraced = texture(u_ScreenOffsetMap2, tc).xy;
	minVelocity = (minVelocity + texture(u_ShadowMap, uvTraced).xy) * 0.5;
	#endif

	tc -= minVelocity.xy;

	vec4 previous = texture(u_ScreenDepthMap, tc);

	previous = clip_aabb(currentMin.xyz, currentMax.xyz, clamp(cmc, currentMin, currentMax), previous);
	float temp = clamp(1.0 - (length(minVelocity * r_FBufScale) * 0.1), min(0.2 + (roughness * 1.7), 0.875), 0.875);

	specularOut		= mix(cmc, previous, temp);
	diffuseOut.rgb	= sqrt(specularOut.rgb * (specularAndGloss.rgb * EnvBRDF.x + EnvBRDF.y));
	diffuseOut.a	= 1.0 - specularOut.a;

#elif defined(POINT_LIGHT)
	vec4 lightVec		= vec4(var_Position.xyz - position + (N*0.01), var_Position.w);
	vec3 L				= lightVec.xyz;
	float lightDist		= length(L);
	L				   /= lightDist;

	NL = clamp(dot(N, L), 1e-8, 1.0);

	attenuation  = CalcLightAttenuation(lightDist, var_Position.w);
	attenuation *= NL;

	#if defined(USE_DSHADOWS)
		attenuation *= getShadowValue(lightVec);
	#endif

	H = normalize(L + E);
	EH = max(1e-8, dot(E, H));
	NH = max(1e-8, dot(N, H));
	NE = abs(dot(N, E)) + 1e-5;

	vec3 reflectance = vec3(1.0, 1.0, 1.0);
	diffuseOut.rgb = sqrt(var_LightColor * reflectance * attenuation);

	reflectance = CalcSpecular(specularAndGloss.rgb, NH, NL, NE, EH, roughness);
	specularOut.rgb = sqrt(var_LightColor * reflectance * attenuation);
#elif defined(CUBEMAP)
	NE = clamp(dot(N, E), 0.0, 1.0);
	vec3 EnvBRDF = texture(u_EnvBrdfMap, vec2(roughness, NE)).rgb;

	vec3 R = reflect(E, N);

	float weight = clamp(-getCubemapWeight(position, R), 0.0, 1.0);

	if (weight == 0.0)
		discard;

	// parallax corrected cubemap (cheaper trick)
	// from http://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/
	vec3 parallax = u_CubeMapInfo.xyz + u_CubeMapInfo.w * -var_ViewDir;

	vec3 cubeLightColor = vec3(0.0);
	if (var_Instance == 0)
		cubeLightColor = textureLod(u_ShadowMap, R + parallax, ROUGHNESS_MIPS * roughness).rgb;
	if (var_Instance == 1)
		cubeLightColor = textureLod(u_ShadowMap2, R + parallax, ROUGHNESS_MIPS * roughness).rgb;
	if (var_Instance == 2)
		cubeLightColor = textureLod(u_ShadowMap3, R + parallax, ROUGHNESS_MIPS * roughness).rgb;
	else
		cubeLightColor = textureLod(u_ShadowMap4, R + parallax, ROUGHNESS_MIPS * roughness).rgb;

    cubeLightColor *= cubeLightColor;
	diffuseOut.rgb	= sqrt(cubeLightColor * (specularAndGloss.rgb * EnvBRDF.x + EnvBRDF.y) * weight);

#elif defined(SUN_LIGHT)
	vec3 L2, H2;
	float NL2, EH2, NH2, L2H2;

	L2	= (u_PrimaryLightOrigin.xyz - position * u_PrimaryLightOrigin.w);
	H2  = normalize(L2 + E);
    NL2 = clamp(dot(N, L2), 0.0, 1.0);
    NL2 = max(1e-8, abs(NL2) );
    EH2 = max(1e-8, dot(E, H2));
    NH2 = max(1e-8, dot(N, H2));

	float shadowValue = texelFetch(u_ShadowMap, windowCoord, 0).r;

	attenuation  = NL2;
	attenuation *= shadowValue;

	vec3 reflectance = vec3(1.0);
	diffuseOut.rgb  = sqrt(u_PrimaryLightColor * reflectance * attenuation);
	
	reflectance			= CalcSpecular(specularAndGloss.rgb, NH2, NL2, NE, EH2, roughness);
	specularOut.rgb		= sqrt(u_PrimaryLightColor * reflectance * attenuation);

#elif defined(LIGHT_GRID)
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
	
	out_Color = max(diffuseOut, vec4(0.0));
	out_Glow  = max(specularOut, vec4(0.0));
}