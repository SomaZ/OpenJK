/*[Vertex]*/
in vec3 attr_Position;
in vec4 attr_TexCoord0;

uniform mat4 u_ModelViewProjectionMatrix;

out vec2 var_TexCoords;


void main()
{
	gl_Position = u_ModelViewProjectionMatrix * vec4(attr_Position, 1.0);
	var_TexCoords = attr_TexCoord0.st;
}

/*[Fragment]*/
uniform sampler2D u_TextureMap;
uniform sampler2D u_LevelsMap;
uniform vec4 u_Color;
uniform vec2  u_AutoExposureMinMax;
uniform vec3   u_ToneMinAvgMaxLinear;

in vec2 var_TexCoords;

out vec4 out_Color;

const vec3  LUMINANCE_VECTOR =   vec3(0.2125, 0.7154, 0.0721); //vec3(0.299, 0.587, 0.114);

vec3 LinearTosRGB( in vec3 color )
{
	vec3 lo = 12.92 * color;
	vec3 hi = 1.055 * pow(color, vec3(0.41666)) - 0.055;
	return mix(lo, hi, greaterThanEqual(color, vec3(0.0031308)));
}

#if defined(USE_FILMIC_TONEMAPPING)
vec3 Tonemap( in vec3 x )
{
	const float SS  = 0.22; // Shoulder Strength
	const float LS  = 0.30; // Linear Strength
	const float LA  = 0.10; // Linear Angle
	const float TS  = 0.20; // Toe Strength
	const float TAN = 0.01; // Toe Angle Numerator
	const float TAD = 0.30; // Toe Angle Denominator
	
	vec3 SSxx = SS * x * x;
	vec3 LSx = LS * x;
	vec3 LALSx = LSx * LA;
	
	return ((SSxx + LALSx + TS * TAN) / (SSxx + LSx + TS * TAD)) - TAN / TAD;

	//return ((x*(SS*x+LA*LS)+TS*TAN)/(x*(SS*x+LS)+TS*TAD)) - TAN/TAD;

}
#elif defined(USE_ACES_TONEMAPPING)
vec3 Tonemap( in vec3 x )
{
	vec3 a = x * (x + 0.0245786) - 0.000090537;
	vec3 b = x * (0.983729 * x + 0.4329510) + 0.238081;
	return a / b;
}
#elif defined(USE_ACES_V_TONEMAPPING)
vec3 Tonemap( in vec3 x )
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0, 1.0);
}
#else
vec3 Tonemap( in vec3 x)
{
	return x;
}
#endif

void main()
{
	vec4 color = texture(u_TextureMap, var_TexCoords) * u_Color;
	vec3 minAvgMax = texture(u_LevelsMap, var_TexCoords).rgb;
	vec3 logMinAvgMaxLum = clamp(minAvgMax * 20.0 - 10.0, -u_AutoExposureMinMax.y, -u_AutoExposureMinMax.x);
		
	float avgLum = exp2(logMinAvgMaxLum.y);
	//float maxLum = exp2(logMinAvgMaxLum.z);

	color.rgb *= u_ToneMinAvgMaxLinear.y / avgLum;
	color.rgb = max(vec3(0.0), color.rgb - vec3(u_ToneMinAvgMaxLinear.x));

	vec3 fWhite = 1.0 / Tonemap(vec3(u_ToneMinAvgMaxLinear.z - u_ToneMinAvgMaxLinear.x));
	color.rgb = Tonemap(color.rgb) * fWhite;

	#if defined(USE_LINEAR_LIGHT)
	color.rgb = LinearTosRGB(color.rgb);
	#endif
	
	out_Color = clamp(color, 0.0, 1.0);
}
