#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 TintColor;

uniform sampler2D uTexture;
uniform vec3 uLightDir;
uniform float uAmbient;

void main() {
    vec4 texColor = texture(uTexture, TexCoord) * TintColor;
    if (texColor.a < 0.05) discard;

    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, -normalize(uLightDir)), 0.0);
    float light = uAmbient + diff * (1.0 - uAmbient);

    FragColor = vec4(texColor.rgb * light, texColor.a);
}
