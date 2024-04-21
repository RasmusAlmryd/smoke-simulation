#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;

layout(binding = 8) uniform usampler3D gridTex;

vec3 volumeSize;

in vec3 position;
in float depthValue;

vec3 boxCenter = vec3(0.0f, 0.0f, 0.0f);

uniform mat4 modelViewProjectionMatrix;

float threshold = 0.003f;



void main()
{


	volumeSize = textureSize(gridTex, 0);
	vec3 pos = position * volumeSize/2;

	threshold = threshold*depthValue;
	float r, g, b;

	int count = 0;
	if(abs(pos.x - boxCenter.x) > (volumeSize.x/2-threshold)) count+=1;
	if(abs(pos.y - boxCenter.y) > (volumeSize.y/2-threshold)) count+=1;
	if(abs(pos.z - boxCenter.z) > (volumeSize.z/2-threshold)) count+=1;

//	vec3 col = vec3(texture(gridTex, vec3(position.xy/2+1, 0.5f)).r);
//	fragmentColor = vec4(position.xy/2+1, 0.5f, 1.0f);
//	return;


	if(count >= 2) 
		//abs(position.y - boxCenter.y) < (1.0f-threshold) ||
		//abs(position.z - boxCenter.z) < (1.0f-threshold)
		//vec3 color = vec3(abs(position.x - boxCenter.x) 
		fragmentColor = vec4(vec3(0.7f), 1.0);
	else
		fragmentColor = vec4(1,0,0, 0.0f);
}