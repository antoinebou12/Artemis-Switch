#version 460

// Lightweight NIS-inspired 1-pass RGB upscaler with edge contrast boost.
// Not a full NVIDIA NIS coef-table port; tuned for Switch latency budget.

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

    vec3 c  = texture(inputTexture, vTextureCoord).rgb;
    vec3 nw = texture(inputTexture, vTextureCoord + vec2(-texel.x, -texel.y)).rgb;
    vec3 n  = texture(inputTexture, vTextureCoord + vec2(0.0, -texel.y)).rgb;
    vec3 ne = texture(inputTexture, vTextureCoord + vec2( texel.x, -texel.y)).rgb;
    vec3 w  = texture(inputTexture, vTextureCoord + vec2(-texel.x, 0.0)).rgb;
    vec3 e  = texture(inputTexture, vTextureCoord + vec2( texel.x, 0.0)).rgb;
    vec3 sw = texture(inputTexture, vTextureCoord + vec2(-texel.x,  texel.y)).rgb;
    vec3 s  = texture(inputTexture, vTextureCoord + vec2(0.0,  texel.y)).rgb;
    vec3 se = texture(inputTexture, vTextureCoord + vec2( texel.x,  texel.y)).rgb;

    vec3 blur = (nw + n + ne + w + c + e + sw + s + se) / 9.0;
    float contrast = clamp(length(c - blur) * 3.5, 0.0, 1.0);
    vec3 sharpened = c + (c - blur) * mix(0.55, 1.15, contrast);
    outColor = vec4(clamp(sharpened, 0.0, 1.0), 1.0);
}
