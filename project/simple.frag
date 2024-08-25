#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

uniform vec3 material_color;
uniform vec3 point_light_color;
uniform bool add_shadow;
uniform float shadowAlpha;

uniform mat4 lightMatrix;

layout(location = 0) out vec4 fragmentColor;

layout(binding = 0) uniform sampler2D colorMap;
layout(binding = 9) uniform sampler2D ligthMap;

in vec3 worldPos;
in vec3 viewSpacePosition;

vec4 lightCoord;

void main()
{
	lightCoord = lightMatrix * vec4(viewSpacePosition, 1.0);
	vec4 lightColor = texture(ligthMap, lightCoord.xy/lightCoord.w);
	vec3 shadowColor = lightColor.xyz/point_light_color;
	shadowColor = 1 - ((1 - shadowColor) * shadowAlpha);

	lightCoord /= lightCoord.w;
	if(lightCoord.x < 0.0f || lightCoord.x > 1.0f || lightCoord.y < 0.0f || lightCoord.y > 1.0f) shadowColor = vec3(1.0f);


	vec4 color = texture(colorMap, vec2(worldPos.x, worldPos.z*-1.0)/10);
	if(add_shadow) color *= shadowColor.x;

	fragmentColor = vec4(color.xyz, 1.0);
}
