#version 430

layout(location = 0)  in vec3 position;
layout(location = 1)  in vec3 normal;
//layout(location = 2)  in vec3 tangent;
//layout(location = 3)  in vec3 bitangent;
//layout(location = 4)  in vec4 color0;
//layout(location = 5)  in vec4 color1;
//layout(location = 6)  in vec4 index;
//layout(location = 7)  in vec4 weight;
//layout(location = 8)  in vec4 texcoord0;
//layout(location = 9)  in vec4 texcoord1;
//layout(location = 10) in vec4 texcoord2;

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

out vec3 positionWC;
out vec3 normalWC;

void main()
{
	gl_Position = MVP*vec4(position, 1.0);
	positionWC = (M*vec4(position, 1.0)).xyz;
	normalWC = (M*vec4(normal, 1.0)).xyz;
}

