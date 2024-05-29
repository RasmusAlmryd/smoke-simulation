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
	//fragmentColor = vec4(vec3(tex_volume_pos), 1.0);
	//fragmentColor.rgb *= fragmentColor.a;
	//vec3 col = vec3(lightCoord.xy/lightCoord.w,0);
	vec3 col = baseSmokeColor;
	float alpha = smoke_density;
	col *= lightColor.xyz;

	if(!backToFront){
		col.rgb *= alpha;
	}
	//col.a *= alpha;
	fragmentColor = vec4(col, alpha);
	//fragmentColor = vec4(1.0, 0.0, 0.0, 1.0); // Red color

	
	//fragmentColor = vec4(vec3(1.0), 0.5);
	//fragmentColor = vec4(viewFragPos.xy/viewFragPos.w, 0, 0.5);
	//fragmentColor = vec4(vec3(1.0), smoke_density);
	//fragmentColor = vec4(tex_volume_pos, 1.0);
}
