#version 460

layout (location = 0) in vec2 vTextureCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D plane0;
layout (binding = 1) uniform sampler2D plane1;

layout (std140, binding = 0) uniform Transformation
{
    mat3 yuvmat;
    vec3 offset;
    vec4 uv_data;
    vec4 orientation;
} u;

void main()
{
    // uv_data = (cropOriginXY / frameSize, frameSize / cropSize).
    // Map the destination quad across the cropped rect in logical UV space,
    // then rotate into the real NV12 texture when orientation != 0.
    // (vTC - origin) * scale is wrong for Fill/zoom crops and blows up under
    // 90/270 rotation when logical aspect no longer matches the screen.
    vec2 uv = u.uv_data.xy + vTextureCoord / u.uv_data.zw;
    if (u.orientation.x == 90.0)
        uv = vec2(uv.y, 1.0 - uv.x);
    else if (u.orientation.x == 180.0)
        uv = vec2(1.0 - uv.x, 1.0 - uv.y);
    else if (u.orientation.x == 270.0)
        uv = vec2(1.0 - uv.y, uv.x);

    vec3 yuv = vec3(texture(plane0, uv).r, texture(plane1, uv).r,
                    texture(plane1, uv).g) - u.offset;
    vec3 rgb = u.yuvmat * yuv;

    outColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
