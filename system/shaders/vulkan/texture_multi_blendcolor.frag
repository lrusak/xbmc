#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler0;
layout(binding = 2) uniform sampler2D texSampler1;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord0;
layout(location = 2) in vec2 fragTexCoord1;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler0, fragTexCoord0).rgba * texture(texSampler1, fragTexCoord1).rgba * fragColor;
}
