#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vPosition;
layout(location = 0) out  vec2 texCoord;

const vec2 quadVertices[4] = vec2[4]( vec2(1, 1),  vec2(1, -1),  vec2(-1, 1),  vec2(-1, -1)  );
const vec2 texVertices[4] = vec2[4](  vec2(1, 1 ), vec2(1,  0 ), vec2( 0, 1 ), vec2(0,   0) );
const float z = 0.0f;

void main()
{
   texCoord = texVertices[gl_VertexIndex];
   gl_Position = vec4(quadVertices[gl_VertexIndex], z, 1.0);
}
