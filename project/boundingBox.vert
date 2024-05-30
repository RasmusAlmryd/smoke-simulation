#version 420

layout(location = 0) in vec3 in_position;

uniform mat4 modelViewProjectionMatrix;


out vec3 position;
out float depthValue;

void main()
{
	gl_Position = modelViewProjectionMatrix * vec4(in_position, 1.0);
	position = in_position;
	depthValue = gl_Position.w;
}