#version 430

layout(binding=0) uniform ModelBlock
{
	mat4 M;
	mat4 V;
	mat4 MV;
	mat4 MVP;
};

layout(binding=1) uniform MaterialBlock
{
	vec4 albedo; //Alpha channel holds roughness.
};

in vec3 positionWC;
in vec3 normalWC;

const float invPI = 0.31830988618379067;

out vec4 frag_color;

void main()
{
	//Just assume one light, at the camera location.
	vec3 camera_pos = -transpose(mat3(V))*V[3].xyz;
	vec3 view_dir = normalize(camera_pos - positionWC);
	vec3 light_dir = view_dir;
	
	vec3 diffuse = albedo.xyz*invPI;
	vec3 color = diffuse*max(dot(light_dir, normalize(normalWC)), 0.0);

	frag_color = vec4(color,1.0);
}