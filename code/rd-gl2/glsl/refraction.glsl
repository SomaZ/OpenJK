/*[Vertex]*/
in vec2 attr_TexCoord0;
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

layout(std140) uniform Liquid
{
vec3		water_color;
float		time;
vec3		fog_color;
float		depth;
float		isLiquid;
float		height;
float		choppy;
float		speed;
float		freq;
};

uniform vec3	u_ViewOrigin;
uniform vec4	u_PrimaryLightOrigin;
uniform float	u_PrimaryLightRadius;

uniform int u_TCGen0;
uniform vec3 u_TCGen0Vector0;
uniform vec3 u_TCGen0Vector1;
uniform vec3 u_LocalViewOrigin;
uniform int u_TCGen1;

uniform vec4 u_DiffuseTexMatrix;
uniform vec4 u_DiffuseTexOffTurb;

uniform mat4 u_ModelViewProjectionMatrix;
uniform mat4 u_InvViewProjectionMatrix;
uniform mat4 u_ModelMatrix;
uniform mat4 u_NormalMatrix;

#if defined(USE_VERTEX_ANIMATION)
uniform float u_VertexLerp;
#elif defined(USE_SKELETAL_ANIMATION)
uniform mat4x3 u_BoneMatrices[20];
#endif

out vec4 var_TexCoords;
out vec3 var_Position;
out vec4 var_Normal;
out vec4 var_Tangent;
out vec4 var_Bitangent;
out vec4 var_PrimaryLightDir;

vec2 GenTexCoords(int TCGen, vec3 position, vec3 normal, vec3 TCGenVector0, vec3 TCGenVector1)
{
	vec2 tex = attr_TexCoord0;

	switch (TCGen)
	{
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

void main()
{
#if defined(USE_VERTEX_ANIMATION)
	vec3 position  = mix(attr_Position,    attr_Position2,    u_VertexLerp);
	vec3 normal    = mix(attr_Normal,      attr_Normal2,      u_VertexLerp);
	vec3 tangent   = mix(attr_Tangent.xyz, attr_Tangent2.xyz, u_VertexLerp);
#elif defined(USE_SKELETAL_ANIMATION)
	vec4 position4 = vec4(0.0);
	vec4 originalPosition = vec4(attr_Position, 1.0);
	vec4 normal4 = vec4(0.0);
	vec4 originalNormal = vec4(attr_Normal - vec3 (0.5), 0.0);
	vec4 tangent4 = vec4(0.0);
	vec4 originalTangent = vec4(attr_Tangent.xyz - vec3(0.5), 0.0);
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
		tangent4 += (boneMatrix * originalTangent) * attr_BoneWeights[i];
	}

	vec3 position = position4.xyz;
	vec3 normal = normalize (normal4.xyz);
	vec3 tangent = normalize (tangent4.xyz);
#else
	vec3 position  = attr_Position;
	vec3 normal    = attr_Normal;
	vec3 tangent   = attr_Tangent.xyz;
#endif

#if !defined(USE_SKELETAL_ANIMATION)
	normal  = normal  * 2.0 - vec3(1.0);
	tangent = tangent * 2.0 - vec3(1.0);
#endif

	vec2 texCoords = GenTexCoords(u_TCGen0, position, normal, u_TCGen0Vector0, u_TCGen0Vector1);
	var_TexCoords.xy = ModTexCoords(texCoords, position, u_DiffuseTexMatrix, u_DiffuseTexOffTurb);

	gl_Position = u_ModelViewProjectionMatrix * vec4(position, 1.0);

	position  = (u_ModelMatrix * vec4(position, 1.0)).xyz;
	normal    = mat3(u_NormalMatrix) * normal;
	tangent   = mat3(u_NormalMatrix) * tangent;
	vec3 bitangent = cross(normal, tangent) * (attr_Tangent.w * 2.0 - 1.0);

	var_Position = position;
	// store view direction in tangent space to save on outs
	vec3 viewDir  = u_ViewOrigin - position;
	var_Normal    = vec4(normal,    viewDir.x);
	var_Tangent   = vec4(tangent,   viewDir.y);
	var_Bitangent = vec4(bitangent, viewDir.z);

	var_PrimaryLightDir.xyz = u_PrimaryLightOrigin.xyz - (position * u_PrimaryLightOrigin.w);
	var_PrimaryLightDir.w = u_PrimaryLightRadius * u_PrimaryLightRadius;
}

/*[Fragment]*/
/*
* Liquid shader based on:
* "Seascape" by Alexander Alekseev aka TDM - 2014
* Contact: tdmaav@gmail.com
*/

const float etaR = 0.65;
const float etaG = 0.67; // Ratio of indices of refraction
const float etaB = 0.69;
const float fresnelPower = 2.0;
const float F = ((1.0 - etaG) * (1.0 - etaG)) / ((1.0 + etaG) * (1.0 + etaG));

uniform sampler2D	u_ScreenImageMap;
uniform sampler2D   u_ScreenDepthMap;
uniform samplerCube u_CubeMap;
uniform vec4		u_CubeMapInfo;
uniform vec4		u_Color;
uniform vec4		u_ViewInfo;

uniform sampler2D u_ShadowMap;
uniform vec3  u_PrimaryLightColor;
uniform vec3  u_PrimaryLightAmbient;

layout(std140) uniform Liquid
{
vec3		water_color;
float		time;
vec3		fog_color;
float		depth;
float		isLiquid;
float		height;
float		choppy;
float		speed;
float		freq;
};

in vec3 var_Position;
in vec4 var_Normal;
in vec4 var_Tangent;
in vec4 var_Bitangent;
in vec4 var_PrimaryLightDir;

out vec4 out_Color;
out vec4 out_Glow;

#define EPSILON_NRM (.2 / 1920)
const int NUM_STEPS = 8;
const float EPSILON = 1e-3;
const int ITER_GEOMETRY = 3;
const int ITER_FRAGMENT = 5;
const mat2 octave_m = mat2(1.6, 1.2, -1.2, 1.6);
float SEA_TIME = 1.0 + time * speed;

float hash(vec2 p) {
	float h = dot(p, vec2(127.1, 311.7));
	return fract(sin(h)*43758.5453123);
}
float noise(in vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f*f*(3.0 - 2.0*f);
	return -1.0 + 2.0*mix(mix(hash(i + vec2(0.0, 0.0)),
		hash(i + vec2(1.0, 0.0)), u.x),
		mix(hash(i + vec2(0.0, 1.0)),
			hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float diffuse(vec3 n, vec3 l, float p) {
	return pow(dot(n, l) * 0.4 + 0.6, p);
}

vec3 specular(vec3 n, vec3 l, vec3 e, float s) {

	vec2 shadowTex = gl_FragCoord.xy * r_FBufScale;
	float shadowValue = texture(u_ShadowMap, shadowTex).r;

	// surfaces not facing the light are always shadowed
	shadowValue *= clamp(dot(n, -l), 0.0, 1.0);

	float nrm = (s + 8.0) / (M_PI * 8.0);
	vec3 lightColor = u_PrimaryLightColor * (pow(max(dot(reflect(e, n), -l), 0.0), s) * nrm);

	return (lightColor * shadowValue);
}

float sea_octave(vec2 uv, float choppy) {
	uv += noise(uv);
	vec2 wv = 1.0 - abs(sin(uv));
	vec2 swv = abs(cos(uv));
	wv = mix(wv, swv, wv);
	return pow(1.0 - pow(wv.x * wv.y, 0.65), choppy);
}


float map(vec3 p) {
	float freq = freq;
	float amp = height;
	float choppy = choppy;
	vec2 uv = p.xy; uv.x *= 0.75;

	float d, h = 0.0;
	for (int i = 0; i < ITER_GEOMETRY; i++) {
		d = sea_octave((uv + SEA_TIME)*freq, choppy);
		d += sea_octave((uv - SEA_TIME)*freq, choppy);
		h += d * amp;
		uv *= octave_m; freq *= 1.9; amp *= 0.22;
		choppy = mix(choppy, 1.0, 0.2);
	}
	return p.z - h;
}

float map_detailed(vec3 p) {
	float freq = freq;
	float amp = height;
	float choppy = choppy;
	vec2 uv = p.xy; uv.x *= 0.75;

	float d, h = 0.0;
	for (int i = 0; i < ITER_FRAGMENT; i++) {
		d = sea_octave((uv + SEA_TIME)*freq, choppy);
		d += sea_octave((uv - SEA_TIME)*freq, choppy);
		h += d * amp;
		uv *= octave_m; freq *= 1.9; amp *= 0.22;
		choppy = mix(choppy, 1.0, 0.2);
	}
	return p.z - h;
}


vec3 getNormal(vec3 p, float eps) {
	vec3 n;
	n.z = map_detailed(p);
	n.x = map_detailed(vec3(p.x + eps, p.y, p.z)) - n.z;
	n.y = map_detailed(vec3(p.x, p.y + eps, p.z)) - n.z;
	n.z = eps;
	return normalize(n);
}

float heightMapTracing(vec3 ori, vec3 dir, out vec3 p) {
	float tm = 0.0;
	float tx = 1000.0;
	float hx = map(ori + dir * tx);
	if (hx > 0.0) return tx;
	float hm = map(ori + dir * tm);
	float tmid = 0.0;
	for (int i = 0; i < NUM_STEPS; i++) {
		tmid = mix(tm, tx, hm / (hm - hx));
		p = ori + dir * tmid;
		float hmid = map(p);
		if (hmid < 0.0) {
			tx = tmid;
			hx = hmid;
		}
		else {
			tm = tmid;
			hm = hmid;
		}
	}
	return tmid;
}

vec3 getSkyColor(vec3 n) {
	vec3 i = normalize(vec3(var_Normal.w,var_Tangent.w,var_Bitangent.w));

	vec3 parallax = u_CubeMapInfo.xyz + u_CubeMapInfo.w * vec3(var_Normal.w,var_Tangent.w,var_Bitangent.w);
	vec3 cubeLightColor = texture(u_CubeMap, reflect(i, n) + parallax).rgb;
	return cubeLightColor;
}

vec3 getGroundColor(vec3 n) {
	vec3 i = normalize(vec3(var_Normal.w,var_Tangent.w,var_Bitangent.w));

	vec3 cubeLightColor = vec3(texture(u_CubeMap, refract(i, n, 0.948)).r, texture(u_CubeMap, refract(i, n, 0.95)).g, texture(u_CubeMap, refract(i, n, 0.952)).b);
	return cubeLightColor;
}

vec3 getSeaColor(vec3 p, vec3 n, vec3 l, vec3 eye, vec3 dist) {
	vec3 i = normalize(vec3(var_Normal.w,var_Tangent.w,var_Bitangent.w));

	float fresnel = clamp(1.0 - dot(n, -i), 0.0, 1.0);
	fresnel = pow(fresnel, 3.0) * 0.65;

	vec3 groundColor = vec3(.4, .3, .2);

	vec3 reflected = getSkyColor(n);
	vec3 refracted = fog_color + diffuse(n,l,80) * water_color * 0.12;
	//vec3 refracted = mix(groundColor, fog_color /*+ diffuse(n,l,80)*/ * water_color * 0.12, depth);
	//refracted.r = mix(texture(u_CubeMap, refract(eye, n, 0.948)).rbg, (fog_color + diffuse(n, l, 80.0) * water_color * 0.12), .1).r;
	//refracted.g = mix(texture(u_CubeMap, refract(eye, n, 0.95)).rbg, (fog_color + diffuse(n, l, 80.0) * water_color * 0.12), .1).g;
	//refracted.b = mix(texture(u_CubeMap, refract(eye, n, 0.952)).rbg, (fog_color + diffuse(n, l, 80.0) * water_color * 0.12), .1).b;

	vec3 color = mix(refracted, reflected, fresnel);

	float atten = max(1.0 - dot(dist, dist) * 0.001, 0.0);
	color += water_color * (p.z - height) * 0.18 * atten;

	color += specular(n, l, i, 60.0);

	return color;
}

void main()
{	
	vec3 n = normalize(var_Normal.xyz);
	vec3 refractColor;
	
	float alpha;
	vec3 viewDir = vec3(var_Normal.w,var_Tangent.w,var_Bitangent.w);
	vec3 eye = normalize(viewDir);

	if (isLiquid == 1) 
	{
		// based on https://www.shadertoy.com/view/Ms2SD1
		eye.z -= 2.0;
		vec3 ori = var_Position.xyz;
		
		vec3 p;
		heightMapTracing(ori, eye, p);
		vec3 dist = p - ori;

		n = getNormal(p, dot(viewDir, viewDir) * EPSILON_NRM);
		vec3 color = getSeaColor(p, -n, var_PrimaryLightDir.xyz, eye, dist);
		
		refractColor = vec3(pow(color, vec3(0.75)));

		alpha = .80;
		alpha += depth * 0.2;
	}
	else
	{
		vec2 windowCoord = gl_FragCoord.xy * r_FBufScale;

		refractColor.r = texture(u_ScreenImageMap, windowCoord + n.xy * etaR).r;
		refractColor.g = texture(u_ScreenImageMap, windowCoord + n.xy * etaG).g;
		refractColor.b = texture(u_ScreenImageMap, windowCoord + n.xy * etaB).b;

		alpha = u_ViewInfo.w;
	}

	out_Color = vec4((refractColor), alpha);
	out_Glow = vec4(0.0);
}
