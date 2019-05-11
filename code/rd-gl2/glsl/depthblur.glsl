/*[Vertex]*/
in vec4 attr_Position;
in vec4 attr_TexCoord0;

out vec2 var_ScreenTex;

void main()
{
	const vec2 positions[] = vec2[3](
		vec2(-1.0f,  1.0f),
		vec2(-1.0f, -3.0f),
		vec2( 3.0f,  1.0f)
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
uniform sampler2D u_ScreenImageMap;
uniform sampler2D u_ScreenDepthMap;
uniform vec4 u_ViewInfo; // zfar / znear, zfar

in vec2 var_ScreenTex;

out vec4 out_Color;

const float gauss[4] = float[4](0.1964825501511404, 0.2969069646728344, 0.09447039785044732, 0.010381362401148057);
const float offsets[4] = float[4](0.0, 1.411764705882353, 3.2941176470588234, 5.176470588235294);
#define GAUSS_SIZE 4

float getLinearDepth(sampler2D depthMap, vec2 tex, const float zFarDivZNear)
{
		float sampleZDivW = texture(depthMap, vec2(tex.x, 1.0-tex.y)).r;
		return 1.0 / mix(zFarDivZNear, 1.0, sampleZDivW);
}

vec4 depthGaussian1D(sampler2D imageMap, sampler2D depthMap, vec2 tex, float zFarDivZNear, float zFar, vec2 direction)
{
	const vec2 scale = r_FBufInvScale * 2.0;
	const float depthBias = 200.0;
	float depthCenter = zFar * getLinearDepth(depthMap, tex, zFarDivZNear);
	vec2 centerSlope = vec2(dFdx(depthCenter), dFdy(depthCenter)) / vec2(dFdx(tex.x), dFdy(tex.y));
		
	vec4 result = texture(imageMap, tex) * gauss[0];
	float total = gauss[0];

	int i, j;

	for (j = 1; j < GAUSS_SIZE; j++)
	{
		vec2 offset = direction * offsets[j] * scale;
		float depthSample = zFar * getLinearDepth(depthMap, tex + offset, zFarDivZNear);
		float depthExpected = depthCenter + dot(centerSlope, offset);
		if(abs(depthSample - depthExpected) < depthBias)
		{
			result += texture(imageMap, tex + offset) * gauss[j];
			total += gauss[j];
		}
	}

	for (j = 1; j < GAUSS_SIZE; j++)
	{
		vec2 offset = -direction * offsets[j] * scale;
		float depthSample = zFar * getLinearDepth(depthMap, tex + offset, zFarDivZNear);
		float depthExpected = depthCenter + dot(centerSlope, offset);
		if(abs(depthSample - depthExpected) < depthBias)
		{
			result += texture(imageMap, tex + offset) * gauss[j];
			total += gauss[j];
		}
	}
		
	return result / total;
}

const int kernelSize = 9;
const float kernel0[9] = float[9]( 0.008397,	0.009655,	0.010667,	0.011324,	0.011552,	0.011324,	0.010667,	0.009655,	0.008397 );
const float kernel1[9] = float[9]( 0.009655,	0.0111, 	0.012264,	0.013019,	0.013282,	0.013019,	0.012264,	0.0111, 	0.009655 );
const float kernel2[9] = float[9]( 0.010667,	0.012264,	0.013549,	0.014384,	0.014674,	0.014384,	0.013549,	0.012264,	0.010667 );
const float kernel3[9] = float[9]( 0.011324,	0.013019,	0.014384,	0.01527,	0.015578,	0.01527,	0.014384,	0.013019,	0.011324 );
const float kernel4[9] = float[9]( 0.011552,	0.013282,	0.014674,	0.015578,	0.015891,	0.015578,	0.014674,	0.013282,	0.011552 );
const float kernel5[9] = float[9]( 0.011324,	0.013019,	0.014384,	0.01527,	0.015578,	0.01527,	0.014384,	0.013019,	0.011324 );
const float kernel6[9] = float[9]( 0.010667,	0.012264,	0.013549,	0.014384,	0.014674,	0.014384,	0.013549,	0.012264,	0.010667 );
const float kernel7[9] = float[9]( 0.009655,	0.0111, 	0.012264,	0.013019,	0.013282,	0.013019,	0.012264,	0.0111, 	0.009655 );
const float kernel8[9] = float[9]( 0.008397,	0.009655,	0.010667,	0.011324,	0.011552,	0.011324,	0.010667,	0.009655,	0.008397 );

float getGaussWeight(int x, int y) {
	if (x == 0)
		return kernel0[y];
	if (x == 1)
		return kernel1[y];
	if (x == 2)
		return kernel2[y];
	if (x == 3)
		return kernel3[y];
	if (x == 4)
		return kernel4[y];
	if (x == 5)
		return kernel5[y];
	if (x == 6)
		return kernel6[y];
	if (x == 7)
		return kernel7[y];
	if (x == 8)
		return kernel8[y];
}

vec4 bilateralFilter(sampler2D imageMap, sampler2D depthMap, vec2 tex, float zFarDivZNear, float zFar, vec2 direction) {
	vec4 color = vec4(0.0);
	vec2 offset;
	float sum = 0.0;
	float coefG,coefZ,finalCoef;
	float Zp = zFar * getLinearDepth(depthMap, tex, zFarDivZNear);

	const float epsilon = 15.0;

	for(int i = -(kernelSize-1)/2; i <= (kernelSize-1)/2; i++) {
		for(int j = -(kernelSize-1)/2; j <= (kernelSize-1)/2; j++) {
			offset = vec2(i,j) * r_FBufInvScale * 4.0;
			coefG = getGaussWeight(i + (kernelSize-1)/2, j + (kernelSize-1)/2);
			float zTmp = zFar * getLinearDepth(depthMap, tex + offset, zFarDivZNear);
			coefZ = 1.0 / (epsilon + abs(Zp - zTmp));
			finalCoef = coefG * coefZ;
			sum += finalCoef;
			color += finalCoef * texture(imageMap, tex + offset);
		}
	}

	return color / sum;
} 

void main()
{		
	out_Color = depthGaussian1D(u_ScreenImageMap, u_ScreenDepthMap, var_ScreenTex, u_ViewInfo.x, u_ViewInfo.y, u_ViewInfo.zw);
}
