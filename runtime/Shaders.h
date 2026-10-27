// GLSL ported from Scratch's renderer (scratch-render's sprite and pen-line shaders).
#pragma once

namespace scratch::shaders {

// Anti-aliased line with round caps. texcoord = (distance along the line, distance across it)
// in pixels; normal = (thickness, length, alpha).
inline const char* LineFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
out vec4 finalColor;
void main() {
    float thickness = fragNormal.x;
    float len = fragNormal.y;
    float past = fragTexCoord.x - clamp(fragTexCoord.x, 0.0, len);
    float dist = length(vec2(past, fragTexCoord.y));
    float coverage = clamp(1.0 - (dist - (thickness - 1.0) * 0.5), 0.0, 1.0);
    float a = fragNormal.z * coverage;
    finalColor = vec4(fragColor.rgb * a, a);
})";

inline const char* SpriteFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform int u_effects;
uniform float u_color;
uniform float u_brightness;
uniform float u_ghost;
uniform float u_pixelate;
uniform vec2 u_skinSize;
const float epsilon = 1e-3;
const vec2 kCenter = vec2(0.5, 0.5);

vec3 convertRGB2HSV(vec3 rgb) {
    const vec4 hueOffsets = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 temp1 = rgb.b > rgb.g ? vec4(rgb.bg, hueOffsets.wz) : vec4(rgb.gb, hueOffsets.xy);
    vec4 temp2 = rgb.r > temp1.x ? vec4(rgb.r, temp1.yzx) : vec4(temp1.xyw, rgb.r);
    float m = min(temp2.y, temp2.w);
    float C = temp2.x - m;
    float V = temp2.x;
    return vec3(abs(temp2.z + (temp2.w - temp2.y) / (6.0 * C + epsilon)), C / (temp2.x + epsilon), V);
}
vec3 convertHue2RGB(float hue) {
    float r = abs(hue * 6.0 - 3.0) - 1.0;
    float g = 2.0 - abs(hue * 6.0 - 2.0);
    float b = 2.0 - abs(hue * 6.0 - 4.0);
    return clamp(vec3(r, g, b), 0.0, 1.0);
}
vec3 convertHSV2RGB(vec3 hsv) {
    vec3 rgb = convertHue2RGB(hsv.x);
    float c = hsv.z * hsv.y;
    return rgb * c + hsv.z - c;
}

void main() {
    vec2 texcoord0 = fragTexCoord;
    if ((u_effects & 8) != 0) {
        vec2 pixelTexelSize = u_skinSize / u_pixelate;
        texcoord0 = (floor(texcoord0 * pixelTexelSize) + kCenter) / pixelTexelSize;
    }
    vec4 c = texture(texture0, texcoord0);
    if ((u_effects & 33) != 0) {
        c.rgb = clamp(c.rgb / (c.a + epsilon), 0.0, 1.0);
        if ((u_effects & 1) != 0) {
            vec3 hsv = convertRGB2HSV(c.rgb);
            const float minLightness = 0.11 / 2.0;
            const float minSaturation = 0.09;
            if (hsv.z < minLightness) hsv = vec3(0.0, 1.0, minLightness);
            else if (hsv.y < minSaturation) hsv = vec3(0.0, minSaturation, hsv.z);
            hsv.x = mod(hsv.x + u_color, 1.0);
            if (hsv.x < 0.0) hsv.x += 1.0;
            c.rgb = convertHSV2RGB(hsv);
        }
        if ((u_effects & 32) != 0) {
            c.rgb = clamp(c.rgb + vec3(u_brightness), vec3(0), vec3(1));
        }
        c.rgb *= c.a + epsilon;
    }
    if ((u_effects & 64) != 0) c *= u_ghost;
    finalColor = c;
})";

} // namespace scratch::shaders
