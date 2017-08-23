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

const float num_divisions = 32.0;
const float EPSILON = 1e-7;

void ConstructBasis(in vec3 n, out vec3 tangent, out vec3 bitangent)
{
	//From Duff et. al. 2017, Building an Orthonormal Basis, Revisited
	//Doesn't handle 0 the way I want - I want 0 to return + or -1. If this is 0, then the other axes are just z and an orthogonal one, which is an easy test.
	//So add to it some value mixed on abs(s)?
	float s = sign(n.z);
	s += mix(1, 0, abs(s));
	const float a = -1.0 / (s + n.z);
	const float b = n.x * n.y * a;
	tangent = vec3(1.0 + s*n.x*n.x*a, s*b, -s*n.x);
	bitangent = vec3(b, s + n.y*n.y*a, -n.y);
}

//Construct a rotation matrix st the positive z axis becomes aligned with n.
mat3 ConstructRotation(in vec3 n)
{
	vec3 t, bt;
	ConstructBasis(n, t, bt);
	return mat3(t, bt, n);
}



vec3 Disc(int vertex_id, int primitive_offset, out vec3 normal)
{
	//First, extract packed data from buffer.
	vec3 center = data[primitive_offset].xyz; 
	float radius = data[primitive_offset].w;
	normal = normalize(vec3(data[primitive_offset+1]).xyz);
	mat3 rot = ConstructRotation(normal);
	const float dt = 2.0*M_PI/num_divisions;

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

vec3 Cylinder(int vertex_id, int primitive_offset, out vec3 normal)
{
	vec3 bottom = data[primitive_offset].xyz;
	float bottom_rad = data[primitive_offset].w;
	vec3 top = data[primitive_offset + 1].xyz;
	float top_rad = data[primitive_offset + 1].w;
	
	vec3 axis = normalize(top - bottom);
	vec3 t, bt;
	ConstructBasis(axis, t, bt);

	const float dt = 2.0*M_PI/num_divisions;

	//normal = axis;
	//return bottom;
	

	if(vertex_id < 3*num_divisions) {
		//Drawing triangles from the top half of the cylinder.
		float k = floor((float(vertex_id) + 1.0) / 3);
		float s = round(0.5*mod(float(vertex_id + 1.0), 3)); //If 1, we're at the bottom, otherwise we're at the top of the cylinder.
		normal = bottom_rad*(cos(k*dt)*t + sin(k*dt)*bt);
		return mix(top, bottom, s) + normal;
	} else {
		//Drawing tris from the bottom half of the cylinder.
		float k = floor((float(vertex_id) + 2.0) / 3);
		float s = round(0.5*mod(float(vertex_id), 3)); //If 1, we're at the bottom, otherwise we're at the top of the cylinder.
		normal = bottom_rad*(cos(k*dt)*t + sin(k*dt)*bt);
		return mix(bottom, top, s) + normal;
	}
}

vec3 Cone(int vertex_id, int primitive_offset, out vec3 normal)
{
	//This is very similar to disc, with a bit of extra logic for the normals.

	vec3 bottom = data[primitive_offset].xyz;
	float radius = data[primitive_offset].w;
	vec3 top = data[primitive_offset + 1].xyz;
	
	vec3 axis = top - bottom;

	

	vec3 t, bt;
	const float dt = 2.0*M_PI/num_divisions;
	ConstructBasis(normalize(axis), t, bt);
	//TODO: Compute per face normal, to use for the point.

	if(mod(float(vertex_id), 3.0) == 0.0) {
		float k = floor((float(vertex_id)) / 3.0) + 0.5; //We're in between the two bottom points.
		vec3 offset = radius*(cos(k*dt)*t + sin(k*dt)*bt);
		float h = 1.0/length(axis - offset);

		normal = length(offset)*h*axis + length(axis)*h*offset;

		return top;
	} 
	else {
		//We're around the bottom. 
		float k = floor((float(vertex_id) + 1.0) / 3.0);
		vec3 offset = radius*(cos(k*dt)*t + sin(k*dt)*bt);
		float h = length(axis - offset);
		normal = length(offset)*h*axis + length(axis)*h*offset;
		return bottom + offset;
	}

}

void main()
{
	//Unpack primitive info.
	int primitive_offset = gl_VertexID & 0x000FFFFF;
	int vertex_id = (gl_VertexID >> 20) & 0x1FF;
	uint primitive_type = uint(gl_VertexID) >> 29;

	vec3 position = vec3(1,2,3);
	vec3 normal = vec3(4, 5, 6);
	color = vec4(1.0,1.0,0.0,1.0);

	switch (primitive_type)
	{
		case SPHERE:
		
		break;
		case DISC:
			position = Disc(vertex_id, primitive_offset, normal);
			color = data[primitive_offset+2];
		break;
		case CYLINDER:
			position = Cylinder(vertex_id, primitive_offset, normal);
			color = data[primitive_offset+2];
		break;
		case CONE:
			position = Cone(vertex_id, primitive_offset, normal);
			color = data[primitive_offset+2];
		break;

		default:
		break;
	}

	positionWC = position;
	normalWC = normalize(normal);
	gl_Position = MVP*vec4(position, 1.0);


}