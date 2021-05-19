#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    outColor = texture(texSampler, inTexCoord);

//    vec2 v1 = texture(texSampler, vec2(0.25, 0.25)).xy;
//    vec2 v2 = texture(texSampler, vec2(0.5, 0.5)).xy;
//    vec2 v3 = texture(texSampler, vec2(0.75, 0.75)).xy;
//    outColor = vec4(rand(v1), rand(v2), rand(v3), 1.0);
////    outColor = vec4(1.0, 0.5, 0, 1.0);
}


