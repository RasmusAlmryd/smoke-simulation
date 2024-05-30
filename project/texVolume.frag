#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

uniform vec3 material_color;
uniform vec3 volume_pos;
uniform bool backToFront;

uniform mat4 lightMatrix;

layout(location = 0) out vec4 fragmentColor;
layout(binding = 8) uniform sampler3D gridTex;
layout(binding = 9) uniform sampler2D ligthMap;

ivec3 volumeSize;
in vec3 worldFragPos;
in vec4 viewFragPos;
in vec3 viewSpacePosition;
in vec4 testLightPos;


vec4 lightCoord;
vec3 baseSmokeColor;


void main()
{
	baseSmokeColor = vec3(1.0f);

	lightCoord = lightMatrix * vec4(viewSpacePosition, 1.0);
	vec4 lightColor = texture(ligthMap, lightCoord.xy/lightCoord.w);

	volumeSize = textureSize(gridTex, 0);
	vec3 tex_volume_pos =  (((worldFragPos - volume_pos) / (volumeSize/2)) + 1) / 2; 
	float smoke_density = texture( gridTex,tex_volume_pos).r; 
	


	vec3 col = baseSmokeColor;
	float alpha = smoke_density;
	col *= lightColor.xyz;

	//pre-multiplying if rendering front-to-back
	if(!backToFront){
		col.rgb *= alpha;
	}


	fragmentColor = vec4(col, alpha);

}
