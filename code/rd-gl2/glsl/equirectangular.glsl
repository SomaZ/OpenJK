/*[Vertex]*/
in vec3 attr_Position;
in vec4 attr_TexCoord0;

uniform mat4 u_ModelViewProjectionMatrix;

out vec2 var_Tex1;

#if defined(CREATE_EQUIRECTANGULAR)

#endif

#if defined(CREATE_CUBEMAP) || defined(CREATE_CUBEFACE)

#endif

void main()
{
	const vec2 positions[] = vec2[3](
		vec2(-1.0f, -1.0f),
		vec2(-1.0f,  3.0f),
		vec2( 3.0f, -1.0f)
	);

	const vec2 texcoords[] = vec2[3](
		vec2( 0.0f, -1.0f),
		vec2( 0.0f,  1.0f),
		vec2( 2.0f, -1.0f)
	);

	gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
	var_Tex1 = texcoords[gl_VertexID];
}

/*[Fragment]*/

uniform vec4 u_Color;

#if defined(CREATE_EQUIRECTANGULAR)
uniform samplerCube u_CubeMap;
#elif defined(CREATE_CUBEMAP) || defined(CREATE_CUBEFACE)
uniform sampler2D u_DiffuseMap;
#endif

in vec2 var_Tex1;

out vec4 out_Color;


void main()
{
#if defined(CREATE_EQUIRECTANGULAR)
    vec2 thetaphi = ((var_Tex1 * 2.0) - vec2(1.0)) * vec2(M_PI, M_PI / 2.0); 
    vec3 rayDirection = vec3(cos(thetaphi.y) * cos(thetaphi.x), cos(thetaphi.y) * sin(thetaphi.x), sin(thetaphi.y));
	out_Color = textureLod(u_CubeMap, rayDirection, 0);
#elif defined(CREATE_CUBEMAP) || defined(CREATE_CUBEFACE)
	out_Color = texture(u_DiffuseMap, var_Tex1) * u_Color;
#else
	out_Color = vec4(1.0, 0.0, 0.0, 0.0);
#endif
}
