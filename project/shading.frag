#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform vec3 material_color;
uniform float material_reflectivity;
uniform float material_metalness;
uniform float material_fresnel;
uniform float material_shininess;
uniform float material_emission;

uniform int has_emission_texture;
uniform int has_color_texture;
layout(binding = 0) uniform sampler2D colorMap;
layout(binding = 5) uniform sampler2D emissiveMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 50.0;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;



vec3 calculateDirectIllumiunation(vec3 wo, vec3 n, vec3 base_color)
{
	vec3 direct_illum = base_color;

	float d = length(viewSpaceLightPosition - viewSpacePosition);

	vec3 Li = point_light_intensity_multiplier * point_light_color * 1/(d*d);

	vec3 wi = normalize(viewSpaceLightPosition - viewSpacePosition);
	
	if(dot(wi, n) <= 0.0)
		return vec3(0.0f);


	vec3 diffuse_term = base_color * 1.0/PI * length(dot(n, wi)) * Li;


	
	vec3 wh = normalize(wi + wo);
	float ndotwh = max(0.0001, dot(n,wh));
	float ndotwi = max(0.0001, dot(n, wi));
	float ndotwo = max(0.0001, dot(n,wo));
	float whdotwi = max(0.0001, dot(wh,wi));
	float wodotwh = max(0.0001, dot(wo,wh));



	float F = material_fresnel + (1 - material_fresnel)*pow(1-whdotwi, 5.0);
	float D = ((material_shininess +2)/ (PI * 2.0) ) * pow(ndotwh, material_shininess);
	float G = min(1, min(2*ndotwh*ndotwo/wodotwh, 2*ndotwh*ndotwi/wodotwh));
	

	float brdf =  F * D * G / (4 * ndotwo *  ndotwi);
	

	vec3 dielectric_term = brdf * ndotwi * Li + (1-F) * diffuse_term;

	vec3 metal_term = brdf * base_color * ndotwi * Li;

	direct_illum = material_metalness * metal_term + (1-material_metalness) * dielectric_term;


	return direct_illum;
	//return vec3(material_color);
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n)
{
	return vec3(0.0);
}

void main()
{
	float visibility = 1.0;
	float attenuation = 1.0;

	vec3 wo = -normalize(viewSpacePosition);
	vec3 n = normalize(viewSpaceNormal);

	vec3 base_color = material_color;

	// Direct illumination
	vec3 direct_illumination_term = visibility * calculateDirectIllumiunation(wo, n, base_color);

	// Indirect illumination
	vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n);

	///////////////////////////////////////////////////////////////////////////
	// Add emissive term. If emissive texture exists, sample this term.
	///////////////////////////////////////////////////////////////////////////
	vec3 emission_term = material_emission * material_color;
	if(has_emission_texture == 1)
	{
		emission_term = texture(emissiveMap, texCoord).xyz;
	}

	vec3 shading = direct_illumination_term + indirect_illumination_term + emission_term;

	fragmentColor = vec4(shading, 1.0);
	return;
}
