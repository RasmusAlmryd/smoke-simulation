#version 420

layout(location = 0) in vec3 in_position;
//layout(location = 1) in vec3 in_color;
//layout(location = 2) in vec2 in_texCoord;

uniform mat4 modelViewProjectionMatrix;


//out vec3 color;
out vec3 position;
out float depthValue;

void main()
{
	gl_Position = modelViewProjectionMatrix * vec4(in_position, 1.0);
	//color = in_color;
	position = in_position;
	depthValue = gl_Position.w;
}