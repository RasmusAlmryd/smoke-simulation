#version 420

layout(location = 0) in vec3 in_position;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 normalMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelViewProjectionMatrix;

out vec3 position;
out vec4 transformedPos;
out vec3 viewSpacePosition;

void main(){
	gl_Position = modelViewProjectionMatrix * vec4(in_position, 1.0);
	position = in_position;
	transformedPos = gl_Position;
	viewSpacePosition = (modelViewMatrix * vec4(position, 1.0)).xyz;

}