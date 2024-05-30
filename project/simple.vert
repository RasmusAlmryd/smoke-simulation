#version 420

layout(location = 0) in vec3 position;
uniform mat4 modelViewProjectionMatrix;
uniform mat4 modelViewMatrix;

out vec3 worldPos;
out vec3 viewSpacePosition;

void main()
{
	gl_Position = modelViewProjectionMatrix * vec4(position, 1.0);
	worldPos = position;
	viewSpacePosition = (modelViewMatrix * vec4(position, 1.0)).xyz;
}
