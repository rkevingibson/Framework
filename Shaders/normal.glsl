#version 430
in vec3 vNormalWC;

out vec4 frag_color;
void main()
{
	frag_color = vec4(abs(vNormalWC), 1.0);
}
