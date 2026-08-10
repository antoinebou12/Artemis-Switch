#version 460

// Compact SGSR1-style 1-pass spatial upscaler (edge-directed RGB).
// Inspired by Qualcomm SGSR1 mobile edge-direction filtering.

layout (location = 0) in vec2 vTextureCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D inputTexture;

layout (std140, binding = 0) uniform EasuConstants
{
    uvec4 con0;
    uvec4 con1;
    uvec4 con2;
    uvec4 con3;
} u;

void main()
{
    ivec2 size = textureSize(inputTexture, 0);
    vec2 texel = 1.0 / vec2(size);

    vec3 c = texture(inputTexture, vTextureCoord).rgb;
    vec3 l = texture(inputTexture, vTextureCoord + vec2(-texel.x, 0.0)).rgb;
    vec3 r = texture(inputTexture, vTextureCoord + vec2( texel.x, 0.0)).rgb;
    vec3 t = texture(inputTexture, vTextureCoord + vec2(0.0, -texel.y)).rgb;
    vec3 b = texture(inputTexture, vTextureCoord + vec2(0.0,  texel.y)).rgb;

    float gx = length(r - l);
    float gy = length(b - t);
    float edge = clamp(max(gx, gy) * 4.0, 0.0, 1.0);

    // Prefer samples along the weaker gradient (edge-aligned).
    vec3 alongEdge = (gx > gy) ? mix(t, b, 0.5) : mix(l, r, 0.5);
    vec3 acrossEdge = (gx > gy) ? mix(l, r, 0.5) : mix(t, b, 0.5);

    vec3 sharpened = c + (c - acrossEdge) * 0.35;
    vec3 filtered = mix(sharpened, alongEdge, edge * 0.45);
    outColor = vec4(clamp(filtered, 0.0, 1.0), 1.0);
}
