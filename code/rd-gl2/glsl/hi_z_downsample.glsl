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
uniform vec4 u_ViewInfo; // miplevel, mip, zNear, zFar
uniform vec2 u_InvTexRes;
uniform mat4 u_InvViewProjectionMatrix; //FIXME: actually inverse projection matrix!

in vec2 var_TexCoords;

out vec4 out_Color;

vec3 ViewPosFromDepth(mat4 inverseProjectionMatrix, vec2 TexCoord, float depth ) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(TexCoord * 2.0 - 1.0, z, 1.0);
    vec4 viewPosition = inverseProjectionMatrix * clipSpacePosition;
	viewPosition = vec4((viewPosition.xyz / viewPosition.w ), 1.0f);

    return viewPosition.xyz;
}

float linearDepth(in float depthSample, in float zNear, in float zFar)
{
	depthSample = 2.0 * depthSample - 1.0;
    float zLinear = 2.0 * zNear * zFar / (zFar + zNear - depthSample * (zFar - zNear));
    return zLinear;
}

void main()
{
	//based on http://rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
	int mip = int(u_ViewInfo.x);
	ivec2 bufferSize = textureSize(u_TextureMap, mip);

	ivec2 tc = ivec2((gl_FragCoord.xy) * 2.0);
	
	vec4 color = vec4(0.0);
	color.x = texelFetch(u_TextureMap, tc, mip).r;
	color.y = texelFetch(u_TextureMap, tc + ivec2(-1, 0), mip).r;
	color.z = texelFetch(u_TextureMap, tc + ivec2( 0,-1), mip).r;
	color.w = texelFetch(u_TextureMap, tc + ivec2(-1,-1), mip).r;

	float minZ = min(min(color.x,color.y),min(color.z,color.w));

	vec3 extra;
	// if we are reducing an odd-width texture then fetch the edge texels
	if ( ( (bufferSize.x & 1) != 0 ) && ( int(gl_FragCoord.x) == bufferSize.x-3 ) ) {
		// if both edges are odd, fetch the top-left corner texel
		if ( ( (bufferSize.y & 1) != 0 ) && ( int(gl_FragCoord.y) == bufferSize.y-3 ) ) {
			extra.z = texelFetch( u_TextureMap, tc + ivec2( 1, 1), mip ).r;
			minZ = min( minZ, extra.z );
		}
		extra.x = texelFetch( u_TextureMap, tc + ivec2( 1, 0), mip ).r;
		extra.y = texelFetch( u_TextureMap, tc + ivec2( 1,-1), mip ).r;
		minZ = min( minZ, min( extra.x, extra.y ) );
	} 
	else
		// if we are reducing an odd-height texture then fetch the edge texels
		if ( ( (bufferSize.y & 1) != 0 ) && ( int(gl_FragCoord.y) == bufferSize.y-3 ) ) {
			extra.x = texelFetch( u_TextureMap, tc + ivec2( 0, 1), mip ).r;
			extra.y = texelFetch( u_TextureMap, tc + ivec2(-1, 1), mip ).r;
			minZ = min( minZ, min( extra.x, extra.y ) );
		}

	if ( true )
	{
		if (u_ViewInfo.y < 1.5)
			minZ = linearDepth(minZ, u_ViewInfo.z, u_ViewInfo.w);
	}
	else
	{
		if (u_ViewInfo.y < 1.5)
			minZ = ViewPosFromDepth(u_InvViewProjectionMatrix, tc / vec2(bufferSize), minZ).z;
	}

	out_Color = vec4(minZ);
}