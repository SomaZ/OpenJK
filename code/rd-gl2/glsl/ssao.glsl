/*[Vertex]*/
in vec4 attr_Position;
in vec4 attr_TexCoord0;

out vec2 var_ScreenTex;

void main()
{
	const vec2 positions[] = vec2[3](
		vec2(-1.0f, -1.0f),
		vec2(-1.0f,  3.0f),
		vec2( 3.0f, -1.0f)
	);

	const vec2 texcoords[] = vec2[3](
		vec2( 0.0f,  1.0f),
		vec2( 0.0f, -1.0f),
		vec2( 2.0f,  1.0f)
	);

	gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
	var_ScreenTex = texcoords[gl_VertexID];
}

/*[Fragment]*/
uniform sampler2D u_ScreenDepthMap;
uniform vec4 u_ViewInfo; // zfar / znear, zfar
uniform mat4 u_InvViewProjectionMatrix;

in vec2 var_ScreenTex;

out vec4 out_Color;

const vec2 poissonDisc[9] = vec2[9](
vec2(-0.7055767, 0.196515),    vec2(0.3524343, -0.7791386),
vec2(0.2391056, 0.9189604),    vec2(-0.07580382, -0.09224417),
vec2(0.5784913, -0.002528916), vec2(0.192888, 0.4064181),
vec2(-0.6335801, -0.5247476),  vec2(-0.5579782, 0.7491854),
vec2(0.7320465, 0.6317794)
);

//from https://github.com/OpenGLInsights/OpenGLInsightsCode/blob/master/Chapter%2015%20Depth%20of%20Field%20with%20Bokeh%20Rendering/src/glf/ssao.cpp
const vec2 halton[32] = vec2[32](
	vec2(-0.353553, 0.612372),
	vec2(-0.25, -0.433013),
	vec2(0.663414, 0.55667),
	vec2(-0.332232, 0.120922),
	vec2(0.137281, -0.778559),
	vec2(0.106337, 0.603069),
	vec2(-0.879002, -0.319931),
	vec2(0.191511, -0.160697),
	vec2(0.729784, 0.172962),
	vec2(-0.383621, 0.406614),
	vec2(-0.258521, -0.86352),
	vec2(0.258577, 0.34733),
	vec2(-0.82355, 0.0962588),
	vec2(0.261982, -0.607343),
	vec2(-0.0562987, 0.966608),
	vec2(-0.147695, -0.0971404),
	vec2(0.651341, -0.327115),
	vec2(0.47392, 0.238012),
	vec2(-0.738474, 0.485702),
	vec2(-0.0229837, -0.394616),
	vec2(0.320861, 0.74384),
	vec2(-0.633068, -0.0739953),
	vec2(0.568478, -0.763598),
	vec2(-0.0878153, 0.293323),
	vec2(-0.528785, -0.560479),
	vec2(0.570498, -0.13521),
	vec2(0.915797, 0.0711813),
	vec2(-0.264538, 0.385706),
	vec2(-0.365725, -0.76485),
	vec2(0.488794, 0.479406),
	vec2(-0.948199, 0.263949),
	vec2(0.0311802, -0.121049)
);

vec3 WorldPosFromDepth(float depth, vec2 TexCoord) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(TexCoord * 2.0 - 1.0, z, 1.0);
    vec4 worldPosition = u_InvViewProjectionMatrix * clipSpacePosition;
	worldPosition = vec4((worldPosition.xyz / worldPosition.w ), 1.0f);

    return worldPosition.xyz;
}

vec3 NormalFromWorldPos(vec3 pos) {
	return normalize(cross(dFdx(pos.xyz), dFdy(pos.xyz)));
}

// Input: It uses texture coords as the random number seed.
// Output: Random number: [0,1), that is between 0.0 and 0.999999... inclusive.
// Author: Michael Pohoreski
// Copyright: Copyleft 2012 :-)
// Source: http://stackoverflow.com/questions/5149544/can-i-generate-a-random-number-inside-a-pixel-shader

float random( const vec2 p )
{
  // We need irrationals for pseudo randomness.
  // Most (all?) known transcendental numbers will (generally) work.
  const vec2 r = vec2(
    23.1406926327792690,  // e^pi (Gelfond's constant)
     2.6651441426902251); // 2^sqrt(2) (Gelfond-Schneider constant)
  //return fract( cos( mod( 123456789., 1e-7 + 256. * dot(p,r) ) ) );
  return mod( 123456789., 1e-7 + 256. * dot(p,r) );  
}

mat2 randomRotation( const vec2 p )
{
	float r = random(p);
	float sinr = sin(r);
	float cosr = cos(r);
	return mat2(cosr, sinr, -sinr, cosr);
}

float getLinearDepth(sampler2D depthMap, const vec2 tex, const float zFarDivZNear)
{
		float sampleZDivW = texture(depthMap, tex).r;
		return 1.0 / mix(zFarDivZNear, 1.0, sampleZDivW);
}

float randAngle()
{
  uint x = uint(gl_FragCoord.x);
  uint y = uint(gl_FragCoord.y);
  return (30u * x ^ y + 10u * x * y);
}

// from http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float radicalInverse_VdC(uint bits) {
     bits = (bits << 16u) | (bits >> 16u);
     bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
     bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
     bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
     bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
     return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 hammersley2D(uint i, uint N) {
     return vec2(float(i)/float(N), radicalInverse_VdC(i));
}

float ambientOcclusion(sampler2D depthMap, const vec2 tex, const float zFarDivZNear, const float zFar)
{
	const float SampleRadius = 30.0;
	const float ShadowScalar = 40.0;
	const float DepthThreshold = 0.006;
	const float ShadowContrast = 1.0;
	const int NumSamples = 16;

	float visibility = 0.0;
	vec3 P = WorldPosFromDepth(texture(depthMap, tex).r, tex);
	vec3 N = -NormalFromWorldPos(P);
	float PerspectiveRadius = (SampleRadius / P.z);

	for (int i = 0; i < NumSamples; ++i)
	{
		// Generate Sample Position
		//vec2 E = halton[i] * vec2(M_PI, 2.0*M_PI);
		vec2 E = hammersley2D(uint(i), uint(NumSamples)) * vec2(M_PI, 2.0*M_PI);

		E.y += randAngle(); // Apply random angle rotation
		vec2 sE= vec2(cos(E.y), sin(E.y)) * PerspectiveRadius * cos(E.x);
		vec2 Sample = var_ScreenTex + sE;
 
		// Create Alchemy helper variables
		vec3 Pi         = WorldPosFromDepth(texture(depthMap, Sample).r, Sample);
		vec3 V          = Pi - P;
		float sqrLen    = dot(V, V);
		float Heaveside = step(sqrt(sqrLen), SampleRadius);
		float dD        = DepthThreshold * P.z;
 
		// For arithmetically removing edge-bleeding error
		// introduced by clamping the ambient occlusion map.
		float EdgeError =	step(0.0, Sample.x) * step(0.0, 1.0 - Sample.x) *
							step(0.0, Sample.y) * step(0.0, 1.0 - Sample.y);
 
		// Summation of Obscurance Factor
		visibility += (max(0.0, dot(N, V) + dD) * Heaveside * EdgeError) / (sqrLen + 0.0001);
	}
 
	// Final scalar multiplications for averaging and intensifying shadows
	visibility *= (2 * ShadowScalar) / NumSamples;
	return max(0.0, 1.0 - pow(visibility, ShadowContrast));
}

void main()
{
	float result = ambientOcclusion(u_ScreenDepthMap, var_ScreenTex, u_ViewInfo.x, u_ViewInfo.y);

	out_Color = vec4(vec3(result), 1.0);
}
