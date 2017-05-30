//Simple Blinn-phong shader, with a single point-light source.

#version 430
in vec3 vNormalWC;
in vec3 vPositionWC;

uniform mat4 view;
uniform vec3 albedo;
uniform vec3 light_pos;
uniform float shininess;

out vec4 frag_color;

void main()
{
	vec3 camera_pos = -transpose(mat3(view)) * view[3].xyz;
	vec3 view_dir = normalize(camera_pos - vPositionWC);
	vec3 light_dir = normalize(light_pos - vPositionWC);
	vec3 halfway_dir = normalize(light_dir + view_dir);

	float specular_angle = max(dot(halfway_dir, vNormalWC), 0.0);
	float specular = pow(specular_angle, shininess);

	float lambertian = max(dot(light_dir, vNormalWC), 0.0);

	vec3 color = lambertian * albedo + specular;


	frag_color = vec4(pow(color,vec3(1.0/2.2)), 1.0);
	//frag_color = vec4(color, 1.0);
}