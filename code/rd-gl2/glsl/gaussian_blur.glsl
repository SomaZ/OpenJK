/*[Vertex]*/
out vec2 var_TexCoords;

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
	var_TexCoords = texcoords[gl_VertexID];
}

/*[Fragment]*/
uniform sampler2D u_TextureMap;
uniform vec4  u_Color;
uniform vec2  u_InvTexRes;
uniform vec4  u_ViewInfo; // mip level to sample from, 0.0, 0.0, 0.0

in vec2 var_TexCoords;

out vec4 out_Color;

#define NUM_TAPS 7

void main()
{
	vec4 color = vec4 (0.0);

#if NUM_TAPS == 7
	const float weights[] = float[4](1.0 / 64.0, 6.0 / 64.0, 15.0 / 64.0, 20.0 / 64.0);

#if defined(BLUR_X)
	color += texelFetch (u_TextureMap, ivec2(2.0 * (vec2 (-3.0, 0.0) + gl_FragCoord.xy)), int(u_ViewInfo.x)) * weights[0];
	color += texelFetch (u_TextureMap, ivec2(2.0 * (vec2 (-2.0, 0.0) + gl_FragCoord.xy)), int(u_ViewInfo.x)) * weights[1];
	color += texelFetch (u_TextureMap, ivec2(2.0 * (vec2 (-1.0, 0.0) + gl_FragCoord.xy)), int(u_ViewInfo.x)) * weights[2];
	color += texelFetch (u_TextureMap, ivec2(2.0 * (vec2 ( 0.0, 0.0) + gl_FragCoord.xy)), int(u_ViewInfo.x)) * weights[3];
	color += texelFetch (u_TextureMap, ivec2(2.0 * (vec2 ( 1.0, 0.0) + gl_FragCoord.xy)), int(u_ViewInfo.x)) * weights[2];
	color += texelFetch (u_TextureMap, ivec2(2.0 * (vec2 ( 2.0, 0.0) + gl_FragCoord.xy)), int(u_ViewInfo.x)) * weights[1];
	color += texelFetch (u_TextureMap, ivec2(2.0 * (vec2 ( 3.0, 0.0) + gl_FragCoord.xy)), int(u_ViewInfo.x)) * weights[0];
#else
	color += texelFetch (u_TextureMap, ivec2(vec2 (0.0, -3.0) + gl_FragCoord.xy), int(u_ViewInfo.x)) * weights[0];
	color += texelFetch (u_TextureMap, ivec2(vec2 (0.0, -2.0) + gl_FragCoord.xy), int(u_ViewInfo.x)) * weights[1];
	color += texelFetch (u_TextureMap, ivec2(vec2 (0.0, -1.0) + gl_FragCoord.xy), int(u_ViewInfo.x)) * weights[2];
	color += texelFetch (u_TextureMap, ivec2(vec2 (0.0,  0.0) + gl_FragCoord.xy), int(u_ViewInfo.x)) * weights[3];
	color += texelFetch (u_TextureMap, ivec2(vec2 (0.0,  1.0) + gl_FragCoord.xy), int(u_ViewInfo.x)) * weights[2];
	color += texelFetch (u_TextureMap, ivec2(vec2 (0.0,  2.0) + gl_FragCoord.xy), int(u_ViewInfo.x)) * weights[1];
	color += texelFetch (u_TextureMap, ivec2(vec2 (0.0,  3.0) + gl_FragCoord.xy), int(u_ViewInfo.x)) * weights[0];
#endif
#elif NUM_TAPS == 3
	const float weights[] = float[2](0.25, 0.5);

#if defined(BLUR_X)
	color += texture (u_TextureMap, vec2 (-1.0, 0.0) * u_InvTexRes + var_TexCoords) * weights[0];
	color += texture (u_TextureMap, vec2 ( 0.0, 0.0) * u_InvTexRes + var_TexCoords) * weights[1];
	color += texture (u_TextureMap, vec2 ( 1.0, 0.0) * u_InvTexRes + var_TexCoords) * weights[0];
#else
	color += texture (u_TextureMap, vec2 (0.0, -1.0) * u_InvTexRes + var_TexCoords) * weights[0];
	color += texture (u_TextureMap, vec2 (0.0,  0.0) * u_InvTexRes + var_TexCoords) * weights[1];
	color += texture (u_TextureMap, vec2 (0.0,  1.0) * u_InvTexRes + var_TexCoords) * weights[0];
#endif
#endif

	out_Color = color * u_Color;
}