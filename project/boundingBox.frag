#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;

//in vec3 color;
in vec2 texCoord;
in vec3 position;

vec3 boxCenter = vec3(0.0f, 0.0f, 0.0f);

const float threshold = 0.05f;



void main()
{
//	fragmentColor = vec4(texCoord, 1.0f, 1.0f); 
//	if(texCoord.x < threshold || texCoord.x > 1.0f - threshold)
//		fragmentColor = vec4(1.0);
//	else
//		fragmentColor = vec4(1,0,0, 0.0f);
	/*if((color.x < threshold || color.y < threshold || color.z < threshold) )
		fragmentColor = vec4(color, 1.0f);
	else
		fragmentColor = vec4(1,0,0, 0.0f);*/
	//fragmentColor = vec4(position, 1.0f);
	int count = 0;
	if(abs(position.x - boxCenter.x) > (1.0f-threshold)) count+=1;
	if(abs(position.y - boxCenter.y) > (1.0f-threshold)) count+=1;
	if(abs(position.z - boxCenter.z) > (1.0f-threshold)) count+=1;

	if(count >= 2) 
		//abs(position.y - boxCenter.y) < (1.0f-threshold) ||
		//abs(position.z - boxCenter.z) < (1.0f-threshold)
		fragmentColor = vec4(vec3(0.7f), 1.0);
	else
		fragmentColor = vec4(1,0,0, 0.0f);
}