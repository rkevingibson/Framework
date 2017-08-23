#version 430

layout(binding=0) uniform ModelBlock
{
	mat4 M;
	mat4 V;
	mat4 MV;
	mat4 MVP;
};

in vec3 positionWC;
in vec3 normalWC;
in vec4 color;

const float invPI = 0.31830988618379067;

out vec4 frag_color;

void main()
{
	//Just assume one light, at the camera location.
	vec3 camera_pos = -transpose(mat3(V))*V[3].xyz;
	vec3 view_dir = normalize(camera_pos - positionWC);
	vec3 light_dir = view_dir;
	
	vec3 diffuse = color.xyz*invPI;
	vec3 lighting = diffuse*abs(dot(light_dir, normalize(normalWC)));
	frag_color = vec4(pow(lighting, vec3(0.45, 0.45,0.45)),1.0);
	//frag_color = vec4(normalWC, 1.0);
}