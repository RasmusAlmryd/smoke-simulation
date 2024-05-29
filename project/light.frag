#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;

uniform vec3 volume_pos;
uniform mat4 lightViewMatrix;

layout(binding = 8) uniform sampler3D gridTex;

ivec3 volumeSize;
in vec3 worldFragPos;

void main()
{
	volumeSize = textureSize(gridTex, 0);
	vec3 tex_volume_pos =  (((worldFragPos - volume_pos) / (volumeSize/2)) + 1) / 2; 
	float smoke_density = texture( gridTex,tex_volume_pos).r; 
	
	fragmentColor = vec4(0,0,0,smoke_density);
}