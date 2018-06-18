/*[Vertex]*/
in vec2 attr_TexCoord0;
in vec3 attr_Position;

#if defined(USE_G_BUFFERS)
in vec3 attr_Normal;
in vec4 attr_Tangent;
#endif

#if defined(USE_VERTEX_ANIMATION)
in vec3 attr_Position2;
in vec3 attr_Normal2;
in vec4 attr_Tangent2;
#elif defined(USE_SKELETAL_ANIMATION)
in uvec4 attr_BoneIndexes;
in vec4 attr_BoneWeights;
#endif

#if defined(USE_G_BUFFERS)
// x = normal_1, y = normal_2, z = specular_1, w = specular_2
uniform vec4	u_EnableTextures; 
uniform vec3	u_ViewOrigin;
#endif

uniform int u_TCGen0;
uniform vec3 u_TCGen0Vector0;
uniform vec3 u_TCGen0Vector1;
uniform vec3 u_LocalViewOrigin;
uniform int u_TCGen1;

uniform vec4 u_DiffuseTexMatrix;
uniform vec4 u_DiffuseTexOffTurb;

uniform mat4 u_ModelViewProjectionMatrix;
uniform mat4 u_ModelMatrix;

#if defined(USE_VERTEX_ANIMATION)
uniform float u_VertexLerp;
#elif defined(USE_SKELETAL_ANIMATION)
uniform mat4x3 u_BoneMatrices[20];
#endif

out vec4 var_TexCoords;
out vec3 var_Position;

#if defined(USE_G_BUFFERS)
out vec4 var_Normal;
out vec4 var_Tangent;
out vec4 var_Bitangent;
#endif

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
	#if defined(USE_G_BUFFERS)
		vec3 normal    = mix(attr_Normal,      attr_Normal2,      u_VertexLerp);
		vec3 tangent   = mix(attr_Tangent.xyz, attr_Tangent2.xyz, u_VertexLerp);
	#endif
#elif defined(USE_SKELETAL_ANIMATION)
	vec4 position4 = vec4(0.0);
	vec4 originalPosition = vec4(attr_Position, 1.0);
	#if defined(USE_G_BUFFERS)
		vec4 normal4 = vec4(0.0);
		vec4 originalNormal = vec4(attr_Normal - vec3 (0.5), 0.0);
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
		#if defined(USE_G_BUFFERS)
			normal4 += (boneMatrix * originalNormal) * attr_BoneWeights[i];
			tangent4 += (boneMatrix * originalTangent) * attr_BoneWeights[i];
		#endif
	}

	vec3 position = position4.xyz;
	#if defined(USE_G_BUFFERS)
		vec3 normal = normalize (normal4.xyz);
		vec3 tangent = normalize (tangent4.xyz);
	#endif
#else
	vec3 position  = attr_Position;
	#if defined(USE_G_BUFFERS)
		vec3 normal    = attr_Normal;
		vec3 tangent   = attr_Tangent.xyz;
	#endif
#endif

#if !defined(USE_SKELETAL_ANIMATION) && defined(USE_G_BUFFERS)
	normal  = normal  * 2.0 - vec3(1.0);
	tangent = tangent * 2.0 - vec3(1.0);
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

	#if defined(USE_G_BUFFERS)
		normal    = (u_ModelMatrix * vec4(normal,   0.0)).xyz;
		tangent   = (u_ModelMatrix * vec4(tangent,  0.0)).xyz;
		vec3 bitangent = cross(normal, tangent) * (attr_Tangent.w * 2.0 - 1.0);
	#endif

	var_Position = position;
	// store view direction in tangent space to save on outs
	#if defined(USE_G_BUFFERS)
		vec3 viewDir  = u_ViewOrigin - position;
		var_Normal    = vec4(normal,    viewDir.x);
		var_Tangent   = vec4(tangent,   viewDir.y);
		var_Bitangent = vec4(bitangent, viewDir.z);
	#endif

}

/*[Fragment]*/
uniform sampler2D	u_DiffuseMap;
uniform int			u_AlphaTestFunction;
uniform float		u_AlphaTestValue;

#if defined(USE_G_BUFFERS)
uniform sampler2D u_NormalMap;
uniform sampler2D u_SpecularMap;
uniform sampler2D u_ShadowMap;
uniform vec4      u_EnableTextures; // x = normal_1, y = normal_2, z = specular_1, w = specular_2
uniform vec4      u_NormalScale;
uniform vec4      u_SpecularScale;
#endif

in vec4   var_TexCoords;
in vec3	  var_Position;

#if defined(USE_G_BUFFERS)
in vec4   var_Normal;
in vec4   var_Tangent;
in vec4   var_Bitangent;

out vec4 out_Color;
out vec4 out_Glow;

#if defined(USE_PARALLAXMAP)
float SampleDepth(sampler2D normalMap, vec2 t)
{
	return 1.0 - texture(normalMap, t).r;
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

vec3 CalcNormal( in vec3 vertexNormal, in vec2 texCoords, in mat3 tangentToWorld )
{
	vec3 N = vertexNormal;

	if (u_EnableTextures.x > 0.0) {
		N.xy = texture(u_NormalMap, texCoords).ag - vec2(0.5);
		N.xy *= u_NormalScale.xy;
		N.z = sqrt(clamp((0.25 - N.x * N.x) - N.y * N.y, 0.0, 1.0));
		N = tangentToWorld * N;
	}

	return normalize(N);
}

vec2 EncodeNormal(in vec3 N)
{
	float f = sqrt(8.0 * N.z + 8.0);
	return N.xy / f + 0.5;
}
#endif
void main()
{

#if !defined(USE_G_BUFFERS)
	if (u_AlphaTestFunction == 0)
		return;
#endif

	vec2 texCoords = var_TexCoords.xy;
#if defined(USE_G_BUFFERS)
	vec3 offsetDir = vec3(0.0,0.0,0.0);
	vec3 vertexColor, position;
	vec3 N;

	mat3 tangentToWorld = mat3(var_Tangent.xyz, var_Bitangent.xyz, var_Normal.xyz);
	position = var_Position;

  #if defined(USE_PARALLAXMAP)
    vec3 viewDir = vec3(var_Normal.w, var_Tangent.w, var_Bitangent.w);
	offsetDir = viewDir * tangentToWorld;

	offsetDir.xy *= -u_NormalScale.a / offsetDir.z;
	offsetDir.xy *= RayIntersectDisplaceMap(texCoords, offsetDir.xy, u_NormalMap);

	texCoords += offsetDir.xy; 
  #endif
#endif
	vec4 diffuse = texture(u_DiffuseMap, texCoords);
	
	if (u_AlphaTestFunction == ATEST_CMP_GE){
		if (diffuse.a < u_AlphaTestValue)
			discard;
	}
	else if (u_AlphaTestFunction == ATEST_CMP_LT){
		if (diffuse.a >= u_AlphaTestValue)
			discard;
	}	
	else if (u_AlphaTestFunction == ATEST_CMP_GT){
		if (diffuse.a <= u_AlphaTestValue)
			discard;
	}
#if defined(USE_G_BUFFERS)
	N = CalcNormal(var_Normal.xyz, texCoords, tangentToWorld);

	vec4 specular = vec4 (1.0);
	if (u_EnableTextures.z > 0.0)
		specular = texture(u_SpecularMap, texCoords);
	specular *= u_SpecularScale;

	out_Glow	= specular;
	//out_Color	= vec4(EncodeNormal(N), offsetDir.xy * 0.5 + 0.5);
	out_Color	= vec4(N, 1.0);
#endif
}