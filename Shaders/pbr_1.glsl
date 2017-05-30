//Simple diffuse shader exploring the Disney/Unreal brdf model, with a single white point light.

#version 430
in vec3 vNormalWC;
in vec3 vPositionWC;

uniform mat4 view;
uniform vec3 albedo;
uniform vec3 light_pos;
uniform vec3 light_color;
uniform float roughness;
uniform float specular;
out vec4 frag_color;

const float invPI = 0.31830988618379067;

float unreal_specular_D(in vec3 h)
{
	float alpha2 = pow(roughness, 4.0);
	return alpha2*invPI / pow(pow(dot(vNormalWC, h), 2.0) * (alpha2 - 1.0) + 1.0, 2.0);
}

float unreal_specular_G(in vec3 l, in vec3 v, in vec3 h)
{
	float k = pow(roughness + 1.0, 2.0) * 0.125;
	float g1_l = dot(vNormalWC, l) / (dot(vNormalWC, l) * (1 - k) + k);
	float g1_v = dot(vNormalWC, v) / (dot(vNormalWC, v) * (1 - k) + k);
	return step(0.0, dot(vNormalWC, l)) * step(0.0, dot(vNormalWC, v)) * g1_l * g1_v;
}

float unreal_specular_F(in vec3 v, in vec3 h)
{
	float f0 = 0.04;//Not sure how to calculate this.
	return f0 + (1.0 - f0) * exp2((-5.55473*dot(v, h) - 6.98316)*dot(v, h));
}

void main()
{
	vec3 camera_pos = -transpose(mat3(view)) * view[3].xyz;
	vec3 view_dir = normalize(camera_pos - vPositionWC);
	vec3 light_dir = normalize(light_pos - vPositionWC);
	vec3 halfway_dir = normalize(light_dir + view_dir);


	vec3 diffuse = albedo*invPI;

	float F = unreal_specular_F(view_dir, halfway_dir);
	float G = unreal_specular_G(light_dir, view_dir, halfway_dir);
	float D = unreal_specular_D(halfway_dir);
	float specular = F * G * D /(4.0*dot(light_dir, vNormalWC)*dot(view_dir, vNormalWC));

	vec3 brdf = diffuse + vec3(specular);
	vec3 color = brdf*light_color*max(dot(light_dir, vNormalWC), 0.0);

	frag_color = vec4(color, 1.0);
	//frag_color = vec4(color, 1.0);
}