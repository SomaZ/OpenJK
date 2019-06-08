/*[Vertex]*/
out vec2 var_TexCoords;

void main()
{
	const vec2 positions[] = vec2[3](
		vec2(-1.0f,  1.0f),
		vec2(-1.0f, -3.0f),
		vec2( 3.0f,  1.0f)
	);

	const vec2 texcoords[] = vec2[3](
		vec2( 0.0f,  2.0f),
		vec2( 0.0f, -2.0f),
		vec2( 4.0f,  2.0f)
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

void main()
{
	// Based on "Next Generation Post Processing in Call of Duty: Advanced Warfare":
	// http://advances.realtimerendering.com/s2014/index.html
	vec4 color = vec4(0.0);
	color += 0.25  * 0.125 *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2(-2.0, -2.0)), u_ViewInfo.x);
	color += 0.5   * 0.25  *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2( 0.0, -2.0)), u_ViewInfo.x);
	color += 0.25  * 0.125 *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2( 2.0, -2.0)), u_ViewInfo.x);
	color += 0.25  * 0.5   *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2(-1.0, -1.0)), u_ViewInfo.x);
	color += 0.25  * 0.5   *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2( 1.0, -1.0)), u_ViewInfo.x);
	color += 0.25  * 0.125 *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2(-2.0,  0.0)), u_ViewInfo.x);
	color += 0.125 *			textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2( 0.0,  0.0)), u_ViewInfo.x);
	color += 0.25  * 0.125 *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2( 2.0, -2.0)), u_ViewInfo.x);
	color += 0.25  * 0.5   *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2(-1.0,  1.0)), u_ViewInfo.x);
	color += 0.25  * 0.5   *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2( 1.0,  1.0)), u_ViewInfo.x);
	color += 0.25  * 0.125 *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2(-2.0,  2.0)), u_ViewInfo.x);
	color += 0.5   * 0.25  *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2( 0.0,  2.0)), u_ViewInfo.x);
	color += 0.25  * 0.125 *	textureLod(u_TextureMap, var_TexCoords + (u_InvTexRes * vec2( 2.0,  2.0)), u_ViewInfo.x);

	out_Color = color;
}