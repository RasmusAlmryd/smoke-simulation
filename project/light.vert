#version 420

layout(location = 0) in vec3 position;

uniform mat4 lightViewProjectionMatrix;

out vec3 worldFragPos;

void main()
{
	gl_Position = lightViewProjectionMatrix * vec4(position, 1.0);
	worldFragPos = position;
}