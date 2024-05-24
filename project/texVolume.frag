#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

uniform vec3 material_color;
uniform vec3 volume_pos;

layout(location = 0) out vec4 fragmentColor;
layout(binding = 8) uniform sampler3D gridTex;

ivec3 volumeSize;
in vec3 fragPos;


void main()
{
	volumeSize = textureSize(gridTex, 0);
	vec3 tex_volume_pos =  (((fragPos - volume_pos) / (volumeSize/2)) + 1) / 2; 
	float smoke_density = texture( gridTex,tex_volume_pos).r; 
	//fragmentColor = vec4(vec3(tex_volume_pos), 1.0);
	fragmentColor = vec4(vec3(1.0), smoke_density);
	//fragmentColor = vec4(tex_volume_pos, 1.0);
}
