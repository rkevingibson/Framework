#version 430

#define SPHERE 0
#define DISC 1
#define CYLINDER 2
#define CONE 3

#define M_PI 3.1415926535897932384626433832795

layout(binding=0) uniform ModelBlock
{
	mat4 M;
	mat4 V;
	mat4 MV;
	mat4 MVP;
};

layout(std430, binding=1) buffer primitives
{
	vec4 data[];
};

out vec3 positionWC;
out vec3 normalWC;
out vec4 color;

vec3 Disc(int vertex_id, int primitive_offset, out vec3 normal)
{
	//First, extract packed data from buffer.
	vec3 center = data[primitive_offset].xyz; 
	float radius = data[primitive_offset].w;
	mat3 rot = mat3(data[primitive_offset+1].xyz, 
					data[primitive_offset+2].xyz, 
					data[primitive_offset+3].xyz);

	const float num_divisions = 16.0;
	const float dt = 2.0*M_PI/num_divisions;
	
	normal = normalize(rot*vec3(0,0,1.0));

	if(vertex_id % 3 == 0) {
		return center;
	} else {
		//TODO: Clamp vertex 3*num_divisions to the same spot as vertex 1. 

		float k = floor((float(vertex_id) + 1.0) / 3.0);
		vec3 offset = vec3(radius*cos(k*dt), radius*sin(k*dt), 0.0);
		//Rotate the offset such that positive z is aligned with the normal.
		return center + rot*offset;
	}
	
}

vec3 Cylinder(int vertex_id, in vec3 center, in mat3 rot, float radius)
{
	return vec3(0,0,0);

}

void main()
{
	//Unpack primitive info.
	int primitive_offset = gl_VertexID & 0x000FFFFF;
	int vertex_id = (gl_VertexID >> 20) & 0x1FF;
	uint primitive_type = uint(gl_VertexID) >> 29;

	vec3 position = vec3(gl_VertexID & 0x0000FFFF,(gl_VertexID >> 16) & 0x0000FFFF, vertex_id);
	vec3 normal = vec3(0.0,0.0,0.0);
	color = vec4(1.0,0.0,0.0,1.0);
	switch (primitive_type)
	{
		case SPHERE:
		
		break;
		case DISC:
			position = Disc(vertex_id, primitive_offset, normal);
		break;
		case CYLINDER:
		break;
		case CONE:
		break;

		default:
		break;
	}

	positionWC = position;
	normalWC = normal;
	gl_Position = MVP*vec4(position, 1.0);


}