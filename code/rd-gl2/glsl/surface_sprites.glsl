/*[Vertex]*/
in vec3 attr_Position;
in vec3 attr_Normal;

uniform mat4 u_ModelViewProjectionMatrix;
uniform vec3 u_ViewOrigin;
uniform vec4 u_PrimaryLightOrigin;
uniform float u_PrimaryLightRadius;

layout(std140) uniform SurfaceSprite
{
	float u_Width;
	float u_Height;
	float u_FadeStartDistance;
	float u_FadeEndDistance;
	float u_FadeScale;
	float u_WidthVariance;
	float u_HeightVariance;
};

out vec2 var_TexCoords;
out vec4 var_PrimaryLightDir;
out float var_Alpha;

float random(vec2 n)
{
	return fract(sin(dot(n.xy, vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
	vec3 V = u_ViewOrigin - attr_Position;

	float width = u_Width * (1.0 + u_WidthVariance*random(attr_Position.xz));
	float height = u_Height * (1.0 + u_HeightVariance*random(attr_Position.xz));

	float distanceToCamera = length(V);
	float fadeScale = smoothstep(u_FadeStartDistance, u_FadeEndDistance,
						distanceToCamera);
	width += u_FadeScale * fadeScale * u_Width;

	float halfWidth = width * 0.5;
	vec3 offsets[] = vec3[](
#if defined(FACE_UP)
		vec3( halfWidth, -halfWidth, 0.0),
		vec3( halfWidth,  halfWidth, height),
		vec3(-halfWidth,  halfWidth, height),
		vec3(-halfWidth, -halfWidth, 0.0)
#else
		vec3( halfWidth, 0.0, 0.0),
		vec3( halfWidth, 0.0, height),
		vec3(-halfWidth, 0.0, height),
		vec3(-halfWidth, 0.0, 0.0)
#endif
	);

	const vec2 texcoords[] = vec2[](
		vec2(1.0, 1.0),
		vec2(1.0, 0.0),
		vec2(0.0, 0.0),
		vec2(0.0, 1.0)
	);

	vec3 offset = offsets[gl_VertexID];

#if defined(FACE_CAMERA)
	//TODO: Allow facing the camera on the z axis as well to match GL1.
	vec2 toCamera = normalize(V.xy);
	offset.xy = offset.x*vec2(toCamera.y, -toCamera.x);
#elif defined(FACE_UP)
	// Incorrect. Copied the FACE_CAMERA code (orients sprite only on X & Y axis) for FACE_UP instead. Now matches GL1.
	// Make this sprite face in some direction
	//offset.xy = offset.x*attr_Normal.xy;
	vec2 toCamera = normalize(V.xy);
	offset.xy = offset.x*vec2(toCamera.y, -toCamera.x);
#endif

	vec4 worldPos = vec4(attr_Position + offset, 1.0);
	gl_Position = u_ModelViewProjectionMatrix * worldPos;
	var_TexCoords = texcoords[gl_VertexID];
	var_Alpha = 1.0 - fadeScale;

	var_PrimaryLightDir.xyz = u_PrimaryLightOrigin.xyz - (worldPos.xyz * u_PrimaryLightOrigin.w);
	var_PrimaryLightDir.w = u_PrimaryLightRadius * u_PrimaryLightRadius;
}

/*[Fragment]*/
uniform sampler2D u_DiffuseMap;
uniform sampler2D u_ShadowMap;

in vec2		var_TexCoords;
in vec4     var_PrimaryLightDir;
in float	var_Alpha;

#if defined(ALPHA_TEST)
uniform float u_AlphaTestValue;
#endif

layout(std140) uniform SurfaceSprite
{
	float u_Width;
	float u_Height;
	float u_FadeStartDistance;
	float u_FadeEndDistance;
	float u_FadeScale;
	float u_WidthVariance;
	float u_HeightVariance;
};

out vec4 out_Color;
out vec4 out_Glow;

void main()
{
	const float alphaTestValue = 0.5;
	out_Color = texture(u_DiffuseMap, var_TexCoords);
	out_Color.a *= var_Alpha*(1.0 - alphaTestValue) + alphaTestValue;

#if defined(ALPHA_TEST)
	if ( out_Color.a < alphaTestValue )
		discard;
#endif

	vec2 shadowTex = gl_FragCoord.xy * r_FBufScale;
	float shadowValue = texture(u_ShadowMap, shadowTex).r;

	out_Color.rgb *= clamp(shadowValue + .3, 0.0, 1.0);

	out_Glow = vec4(0.0);
}
