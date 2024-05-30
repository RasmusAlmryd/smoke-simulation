#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;
layout(binding = 10) uniform sampler2D viewBuffer;
uniform bool backToFront;

in vec2 texCoord;

void main()
{
	vec4 color = texture(viewBuffer, texCoord);

	fragmentColor = vec4(color);
}