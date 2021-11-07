/*[Vertex]*/
in vec2 attr_TexCoord0;
in vec3 attr_Position;
in vec3 attr_Normal;
#if defined(USE_G_BUFFERS)
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

#if defined(USE_DEFORM_VERTEXES)
uniform int u_DeformType;
uniform int u_DeformFunc;
uniform float u_DeformParams[7];
uniform float u_Time;
#endif

uniform mat4 u_ModelViewProjectionMatrix;
uniform mat4 u_PrevViewProjectionMatrix;
uniform mat4 u_ModelMatrix;
uniform mat4 u_NormalMatrix;
uniform int u_ColorGen;
uniform vec4 u_Disintegration; // origin, threshhold

#if defined(USE_VERTEX_ANIMATION)
uniform float u_VertexLerp;
#elif defined(USE_SKELETAL_ANIMATION)
uniform mat4x3 u_BoneMatrices[20];
#endif

out vec4 var_TexCoords;
out vec3 var_Position;
out float var_Alpha;

#if defined(USE_G_BUFFERS)
out vec4 var_Normal;
out vec4 var_Tangent;
out vec4 var_Bitangent;
out vec4 var_CurrentPosition;
out vec4 var_OldPosition;
#endif

#if defined(USE_DEFORM_VERTEXES)
float GetNoiseValue( float x, float y, float z, float t )
{
	// Variation on the 'one-liner random function'.
	// Not sure if this is still 'correctly' random
	return fract( sin( dot(
		vec4( x, y, z, t ),
		vec4( 12.9898, 78.233, 12.9898, 78.233 )
	)) * 43758.5453 );
}

float CalculateDeformScale( in int func, in float time, in float phase, in float frequency )
{
	float value = phase + time * frequency;

	switch ( func )
	{
		case WF_SIN:
			return sin(value * 2.0 * M_PI);
		case WF_SQUARE:
			return sign(0.5 - fract(value));
		case WF_TRIANGLE:
			return abs(fract(value + 0.75) - 0.5) * 4.0 - 1.0;
		case WF_SAWTOOTH:
			return fract(value);
		case WF_INVERSE_SAWTOOTH:
			return 1.0 - fract(value);
		default:
			return 0.0;
	}
}

vec3 DeformPosition(const vec3 pos, const vec3 normal, const vec2 st)
{
	switch ( u_DeformType )
	{
		default:
		{
			return pos;
		}

		case DEFORM_BULGE:
		{
			float bulgeHeight = u_DeformParams[1]; // amplitude
			float bulgeWidth = u_DeformParams[2]; // phase
			float bulgeSpeed = u_DeformParams[3]; // frequency

			float scale = CalculateDeformScale( WF_SIN, u_Time, bulgeWidth * st.x, bulgeSpeed );

			return pos + normal * scale * bulgeHeight;
		}

		case DEFORM_WAVE:
		{
			float base = u_DeformParams[0];
			float amplitude = u_DeformParams[1];
			float phase = u_DeformParams[2];
			float frequency = u_DeformParams[3];
			float spread = u_DeformParams[4];

			float offset = dot( pos.xyz, vec3( spread ) );
			float scale = CalculateDeformScale( u_DeformFunc, u_Time, phase + offset, frequency );

			return pos + normal * (base + scale * amplitude);
		}

		case DEFORM_MOVE:
		{
			float base = u_DeformParams[0];
			float amplitude = u_DeformParams[1];
			float phase = u_DeformParams[2];
			float frequency = u_DeformParams[3];
			vec3 direction = vec3( u_DeformParams[4], u_DeformParams[5], u_DeformParams[6] );

			float scale = CalculateDeformScale( u_DeformFunc, u_Time, phase, frequency );

			return pos + direction * (base + scale * amplitude);
		}

		case DEFORM_PROJECTION_SHADOW:
		{
			vec3 ground = vec3(
				u_DeformParams[0],
				u_DeformParams[1],
				u_DeformParams[2]);
			float groundDist = u_DeformParams[3];
			vec3 lightDir = vec3(
				u_DeformParams[4],
				u_DeformParams[5],
				u_DeformParams[6]);

			float d = dot( lightDir, ground );

			if (d < 0.5)
				lightDir = lightDir + (max( 0.5 - d, 0.0 ) * ground);

			d = 1.0 / dot( lightDir, ground );

			vec3 lightPos = lightDir * d;

			return pos - (lightPos * (dot( pos, ground ) + groundDist));
		}
	}
}

vec3 DeformNormal( const in vec3 position, const in vec3 normal )
{
	if ( u_DeformType != DEFORM_NORMALS )
	{
		return normal;
	}

	float amplitude = u_DeformParams[1];
	float frequency = u_DeformParams[3];

	vec3 outNormal = normal;
	const float scale = 0.98;
	
	outNormal.x += amplitude * GetNoiseValue(
		position.x * scale,
		position.y * scale,
		position.z * scale,
		u_Time * frequency );

	outNormal.y += amplitude * GetNoiseValue(
		100.0 * position.x * scale,
		position.y * scale,
		position.z * scale,
		u_Time * frequency );

	outNormal.z += amplitude * GetNoiseValue(
		200.0 * position.x * scale,
		position.y * scale,
		position.z * scale,
		u_Time * frequency );

	return outNormal;
}
#endif

vec2 GenTexCoords(int TCGen, vec3 position, vec3 normal, vec3 TCGenVector0, vec3 TCGenVector1)
{
       vec2 tex = attr_TexCoord0;
 
       switch (TCGen)
       {
             case TCGEN_ENVIRONMENT_MAPPED:
                    vec3 viewer = normalize(u_LocalViewOrigin - position);
                    vec2 ref = reflect(viewer, normal).yz;
                    tex.s = ref.x * -0.5 + 0.5;
                    tex.t = ref.y *  0.5 + 0.5;
                    break;
             case TCGEN_VECTOR:
                    tex = vec2(dot(position, TCGenVector0), dot(position, TCGenVector1));
                    break;
             default:
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

vec4 CalcColor(vec3 position)
{
	vec4 color = vec4(1.0);

	if (u_ColorGen == CGEN_DISINTEGRATION_1)
	{
		vec3 delta = u_Disintegration.xyz - position;
		float distance = dot(delta, delta);
		if (distance < u_Disintegration.w)
		{
			color *= 0.0;
		}
		else if (distance < u_Disintegration.w + 60.0)
		{
			color *= vec4(0.0, 0.0, 0.0, 1.0);
		}
		else if (distance < u_Disintegration.w + 150.0)
		{
			color *= vec4(0.435295, 0.435295, 0.435295, 1.0);
		}
		else if (distance < u_Disintegration.w + 180.0)
		{
			color *= vec4(0.6862745, 0.6862745, 0.6862745, 1.0);
		}

	}
	else if (u_ColorGen == CGEN_DISINTEGRATION_2)
	{
		vec3 delta = u_Disintegration.xyz - position;
		float distance = dot(delta, delta);
		if (distance < u_Disintegration.w)
		{
			color *= 0.0;
		}
	}
	return color;
}

void main()
{
#if defined(USE_VERTEX_ANIMATION)
	vec3 position  = mix(attr_Position,    attr_Position2,    u_VertexLerp);
	vec3 normal    = mix(attr_Normal,      attr_Normal2,      u_VertexLerp);
	#if defined(USE_G_BUFFERS)
		vec3 tangent   = mix(attr_Tangent.xyz, attr_Tangent2.xyz, u_VertexLerp);
	#endif
#elif defined(USE_SKELETAL_ANIMATION)
	mat4x3 influence =
		u_BoneMatrices[attr_BoneIndexes[0]] * attr_BoneWeights[0] +
        u_BoneMatrices[attr_BoneIndexes[1]] * attr_BoneWeights[1] +
        u_BoneMatrices[attr_BoneIndexes[2]] * attr_BoneWeights[2] +
        u_BoneMatrices[attr_BoneIndexes[3]] * attr_BoneWeights[3];

    vec3 position = influence * vec4(attr_Position, 1.0);
    vec3 normal = normalize(influence * vec4(attr_Normal - vec3(0.5), 0.0));
	#if defined(USE_G_BUFFERS)
		vec3 tangent = normalize(influence * vec4(attr_Tangent.xyz - vec3(0.5), 0.0));
	#endif
#else
	vec3 position  = attr_Position;
	vec3 normal    = attr_Normal;
	#if defined(USE_G_BUFFERS)
		vec3 tangent   = attr_Tangent.xyz;
	#endif
#endif
	
#if !defined(USE_SKELETAL_ANIMATION) && defined(USE_G_BUFFERS)
	normal  = normal  * 2.0 - vec3(1.0);
	#if defined(USE_G_BUFFERS)
		tangent = tangent * 2.0 - vec3(1.0);
	#endif
#endif

	vec2 texCoords = GenTexCoords(u_TCGen0, position, normal, u_TCGen0Vector0, u_TCGen0Vector1);
	var_TexCoords.xy = ModTexCoords(texCoords, position, u_DiffuseTexMatrix, u_DiffuseTexOffTurb);

#if defined(USE_DEFORM_VERTEXES)
	position = DeformPosition(position, normal, attr_TexCoord0.st);
	normal = DeformNormal( position, normal );
#endif

	gl_Position = u_ModelViewProjectionMatrix * vec4(position, 1.0);
	var_Alpha = CalcColor(position).a;
	#if !defined(USE_CUBEMAP_TRANSFORMS)
	#if defined(USE_G_BUFFERS)
	var_CurrentPosition = gl_Position;
	var_OldPosition = (u_PrevViewProjectionMatrix * u_ModelMatrix) * vec4(position, 1.0);
	#endif
	position  = mat3(u_ModelMatrix) * position;
	#endif

	#if defined(USE_G_BUFFERS)
		normal    = mat3(u_NormalMatrix) * normal;
		tangent   = mat3(u_NormalMatrix) * tangent;
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

/*[Geometry]*/
layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

layout(std140) uniform CubemapMatrices
{
mat4		cubeMatrices[6];
};
uniform mat4 u_ModelMatrix;
uniform mat4 u_ModelViewProjectionMatrix;
uniform vec3 u_ViewOrigin;
in vec4   var_TexCoords[];
in vec3	  var_Position[];

#if defined(USE_G_BUFFERS)
in vec4   var_Normal[];
in vec4   var_Tangent[];
in vec4   var_Bitangent[];

out vec4   fs_Normal;
out vec4   fs_Tangent;
out vec4   fs_Bitangent;
#endif

out vec4   fs_TexCoords;
out vec3   fs_Position;

void main()
{
	for (int face = 0; face < 6; ++face)
	{
		for (int i = 0; i < 3; ++i)
		{
			gl_Layer = face;
			gl_Position = cubeMatrices[face] * u_ModelMatrix * vec4(var_Position[i], 1.0);
			fs_TexCoords = var_TexCoords[i];
			fs_Position = var_Position[i];

			#if defined(USE_G_BUFFERS)
			fs_Normal = var_Normal[i];
			fs_Tangent = var_Tangent[i];
			fs_Bitangent = var_Bitangent[i];
			#endif

			EmitVertex();
		}

		EndPrimitive();
	}
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

#if defined(USE_CUBEMAP_TRANSFORMS)
in vec4   fs_TexCoords;
in vec3	  fs_Position;
#else
in vec4   var_TexCoords;
in vec3	  var_Position;
#endif
in float  var_Alpha;

#if defined(USE_G_BUFFERS)

#if defined(USE_CUBEMAP_TRANSFORMS)
in vec4   fs_Normal;
in vec4   fs_Tangent;
in vec4   fs_Bitangent;
#else
in vec4   var_Normal;
in vec4   var_Tangent;
in vec4   var_Bitangent;
in vec4   var_OldPosition;
in vec4   var_CurrentPosition;
#endif

out vec4 out_Color;
out vec4 out_Glow;
out vec2 out_Velocity;

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

	if (u_EnableTextures.x > 0.5) {
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
#if !defined(USE_CUBEMAP_TRANSFORMS)
	vec3 fs_Position = var_Position;
	vec4 fs_TexCoords = var_TexCoords;
#endif

#if !defined(USE_G_BUFFERS)
	if (u_AlphaTestFunction == 0)
		return;
#endif

	vec2 texCoords = fs_TexCoords.xy;
#if defined(USE_G_BUFFERS)

	#if !defined(USE_CUBEMAP_TRANSFORMS)
	vec4   fs_Normal = var_Normal;
	vec4   fs_Tangent = var_Tangent;
	vec4   fs_Bitangent = var_Bitangent;
	#endif

	vec3 offsetDir = vec3(0.0,0.0,0.0);
	vec3 vertexColor, position;
	vec3 N;

	mat3 tangentToWorld = mat3(fs_Tangent.xyz, fs_Bitangent.xyz, fs_Normal.xyz);
	position = fs_Position;

	vec3 viewDir = vec3(fs_Normal.w, fs_Tangent.w, fs_Bitangent.w);

  #if defined(USE_PARALLAXMAP)
	offsetDir = viewDir * tangentToWorld;

	offsetDir.xy *= -u_NormalScale.a / offsetDir.z;
	offsetDir.xy *= RayIntersectDisplaceMap(texCoords, offsetDir.xy, u_NormalMap);

	texCoords += offsetDir.xy; 
  #endif
#endif
	vec4 diffuse = texture(u_DiffuseMap, texCoords);
	diffuse.a *= var_Alpha;
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
	N = CalcNormal(fs_Normal.xyz, texCoords, tangentToWorld);

	vec4 specular = vec4 (1.0);
	if (u_EnableTextures.z > 0.0)
		specular = texture(u_SpecularMap, texCoords);
	specular *= u_SpecularScale;

	out_Glow	= specular;
	//out_Color	= vec4(EncodeNormal(N), offsetDir.xy * 0.5 + 0.5);
	out_Color	= vec4(N, specular.a);

	#if !defined(USE_CUBEMAP_TRANSFORMS)
		vec2 a = var_CurrentPosition.xy / var_CurrentPosition.w;
		vec2 b = var_OldPosition.xy / var_OldPosition.w;
		out_Velocity = (a - b) * 0.5;
	#endif
#endif
}