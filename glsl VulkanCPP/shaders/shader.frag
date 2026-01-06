#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;
void main()
{
    // Force visible solid magenta for test
    outColor.rgb = fragColor;
}