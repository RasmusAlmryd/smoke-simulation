#version 420

layout(location = 0) in vec3 position;
uniform mat4 modelViewProjectionMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 lightViewProjectionMatrix;


out vec3 worldFragPos;
out vec4 viewFragPos;
out vec3 viewSpacePosition;
out vec4 testLightPos;

void main()
{
	gl_Position = modelViewProjectionMatrix * vec4(position, 1.0);
	testLightPos = lightViewProjectionMatrix * vec4(position, 1.0);
	worldFragPos = position;
	viewFragPos = gl_Position;
	viewSpacePosition = (modelViewMatrix * vec4(position, 1.0)).xyz;
}
