/*[Vertex]*/
#if defined(LIGHT_POINT)

uniform vec4 u_DlightTransforms[16]; // xyz = position, w = scale
uniform vec3 u_DlightColors[16];
uniform mat4 u_ModelViewProjectionMatrix;
uniform vec3 u_ViewOrigin;

in vec3 in_Position;

flat out vec3 var_LightPosition;
flat out vec3 var_LightColor;

#elif defined(LIGHT_GRID)

uniform vec3 u_ViewForward;
uniform vec3 u_ViewLeft;
uniform vec3 u_ViewUp;

in vec4 attr_Position;

#endif

out vec3 var_ViewDir;

void main()
{
#if defined(LIGHT_POINT)

	vec4 transform = u_DlightTransforms[gl_InstanceID];
	vec3 worldSpacePosition = in_Position * transform.w * 4.0 + transform.xyz;

	var_ViewDir = normalize(worldSpacePosition - u_ViewOrigin);
	var_LightPosition = transform.xyz;
	var_LightColor = u_DlightColors[gl_InstanceID] * transform.w * 4.0;
	gl_Position = u_ModelViewProjectionMatrix * vec4(worldSpacePosition, 1.0);

#elif defined(LIGHT_GRID)

	const vec2 positions[] = vec2[4](
		vec2(-1.0, -1.0),
		vec2(-1.0,  1.0),
		vec2( 1.0,  1.0),
		vec2( 1.0, -1.0)
	);
	
	vec2 position = positions[gl_VertexID];
	var_ViewDir = (u_ViewForward + u_ViewLeft * -position.x) + u_ViewUp * position.y;
	gl_Position = vec4(position, 0.0, 1.0);

#endif
}

/*[Fragment]*/

uniform vec3 u_ViewOrigin;
uniform vec4 u_ViewInfo; // zfar / znear, zfar
uniform sampler2D u_ScreenDepthMap;
uniform sampler2D u_ScreenNormalMap;
uniform sampler2D u_ScreenSpecularAndGlossMap;
uniform sampler2D u_ScreenDiffuseMap;

#if defined(LIGHT_GRID)
uniform sampler3D u_LightGridDirectionMap;
uniform sampler3D u_LightGridDirectionalLightMap;
uniform sampler3D u_LightGridAmbientLightMap;
uniform vec3 u_LightGridOrigin;
uniform vec3 u_LightGridCellInverseSize;
uniform vec3 u_StyleColor;
uniform vec2 u_LightGridLightScale;

#define u_LightGridAmbientScale u_LightGridLightScale.x
#define u_LightGridDirectionalScale u_LightGridLightScale.y
#endif

#define u_ZFarDivZNear u_ViewInfo.x
#define u_ZFar u_ViewInfo.y

in vec3 var_ViewDir;
#if defined(LIGHT_POINT)
flat in vec3 var_LightPosition;
flat in vec3 var_LightColor;
#endif

out vec4 out_Color;

float LinearDepth(float zBufferDepth, float zFarDivZNear)
{
	return 1.0 / mix(zFarDivZNear, 1.0, zBufferDepth);
}

vec3 DecodeNormal(in vec2 N)
{
	vec2 encoded = N*4.0 - 2.0;
	float f = dot(encoded, encoded);
	float g = sqrt(1.0 - f * 0.25);

	return vec3(encoded * g, 1.0 - f * 0.5);
}

void main()
{
	ivec2 windowCoord = ivec2(gl_FragCoord.xy);
	vec4 specularAndGloss = texelFetch(u_ScreenSpecularAndGlossMap, windowCoord, 0);
	vec3 albedo = texelFetch(u_ScreenDiffuseMap, windowCoord, 0).rgb;
	vec2 normal = texelFetch(u_ScreenNormalMap, windowCoord, 0).rg;
	float depth = texelFetch(u_ScreenDepthMap, windowCoord, 0).r;

	float linearDepth = LinearDepth(depth, u_ZFarDivZNear);
	vec3 N = normalize(DecodeNormal(normal));

	vec3 position, result;

#if defined(LIGHT_POINT)

	position = u_ViewOrigin + normalize(var_ViewDir) * (u_ZFar * linearDepth);
	vec3 vecToLight = var_LightPosition - position;
	float distanceToLight = length(vecToLight);
	vec3 L = normalize(vecToLight);

	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	float attenuation = 1.0 / (distanceToLight * distanceToLight);

	vec3 diffuse = albedo * var_LightColor;
	result = diffuse * NdotL * attenuation;

#elif defined(LIGHT_GRID)

	position = u_ViewOrigin + var_ViewDir * linearDepth;

  #if 1
	ivec3 gridSize = textureSize(u_LightGridDirectionalLightMap, 0);
	vec3 invGridSize = vec3(1.0) / vec3(gridSize);
	vec3 gridCell = (position - u_LightGridOrigin) * u_LightGridCellInverseSize * invGridSize;
	vec3 lightDirection = texture(u_LightGridDirectionMap, gridCell).rgb;
	vec3 directionalLight = texture(u_LightGridDirectionalLightMap, gridCell).rgb;
	vec3 ambientLight = texture(u_LightGridAmbientLightMap, gridCell).rgb;

	vec3 L = normalize(-lightDirection);
	float NdotL = clamp(dot(N, L), 0.0, 1.0);

	result = 2.0 * u_LightGridDirectionalScale * (NdotL * directionalLight) +
			(u_LightGridAmbientScale * ambientLight + vec3(0.125));

	result *= albedo;
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
	
	out_Color = vec4(result, 1.0);
}