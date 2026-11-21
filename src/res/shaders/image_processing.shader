#shader vertex
#version 330 core
void main() {
    vec2 corner = vec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
#shader fragment
#version 330 core
out vec4 pixelColor;
uniform sampler2D inputImage;

uniform int mode;

void main() {
    vec2 pixel = floor(gl_FragCoord.xy);
    vec2 inputPixel = mode == 0 ? pixel : floor(pixel / 2.0);
    vec3 color = floor(texelFetch(inputImage, ivec2(inputPixel), 0).rgb * 255.0 + 0.5);
    float result;
    if (mode == 0) {
        result = floor((color.r + color.g + color.b) / 3.0) / 255.0;
    } else {
        float whiteCount = floor(color.r * 4.0 / 255.0 + 0.5);
        vec2 offset = mod(pixel, 2.0);
        float rank;
        if (offset.y == 0.0)
            rank = offset.x == 0.0 ? 0.0 : 2.0;
        else
            rank = offset.x == 0.0 ? 3.0 : 1.0;
        result = rank < whiteCount ? 1.0 : 0.0;
    }
    pixelColor = vec4(result, result, result, 1.0);
}

