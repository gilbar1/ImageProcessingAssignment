#version 430
layout(local_size_x = 1) in;
layout(std430, binding = 0) readonly buffer Input { uint gray[]; };
layout(std430, binding = 1) buffer Scratch { vec4 work[]; };
layout(std430, binding = 2) buffer Output { uint result[]; };
layout(std430, binding = 3) buffer Queue { uint pending[]; };
uniform int width;
uniform int height;
uniform int mode;
int pos(int x, int y) { return y * width + x; }
bool inside(int x, int y) { return x >= 0 && x < width && y >= 0 && y < height; }
float blur(int x, int y) { return work[pos(x,y)].x; }

void floyd() {
    for (int i = 0; i < width * height; ++i) work[i].x = float(gray[i]);
    const ivec2 offsets[4] = ivec2[4](ivec2(1,0), ivec2(-1,1), ivec2(0,1), ivec2(1,1));
    const float weights[4] = float[4](7,3,5,1);
    const uint maxLevel = 15u;
    const uint levelStep = 255u / maxLevel;
    for (int y = 0; y < height; ++y) for (int column = 0; column < width; ++column) {
        // Mirror both the scan and the diffusion kernel on odd rows.
        int direction = (y % 2 == 0) ? 1 : -1;
        int x = direction == 1 ? column : width - 1 - column;
        int p = pos(x,y);
        float value = work[p].x;
        uint level = uint(clamp(floor(value / float(levelStep) + 0.5), 0.0, float(maxLevel)));
        result[p] = level;
        float error = value - float(level * levelStep);
        int count = 0;
        for (int k = 0; k < 4; ++k)
            if (inside(x+direction*offsets[k].x, y+offsets[k].y)) ++count;
        for (int k = 0; k < 4; ++k) {
            ivec2 q = ivec2(x + direction * offsets[k].x, y + offsets[k].y);
            if (inside(q.x,q.y)) {
                precise float share = error * (count == 4 ? weights[k]/16.0 : 1.0/float(count));
                precise float updated = work[pos(q.x,q.y)].x + share;
                work[pos(q.x,q.y)].x = updated;
            }
        }
    }
}

void canny() {
    int size = width * height;
    // Separable Gaussian: [1,2,1] horizontally, then vertically.
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        int sum = 2 * int(gray[pos(x,y)]);
        if (x > 0) sum += int(gray[pos(x-1,y)]);
        if (x+1 < width) sum += int(gray[pos(x+1,y)]);
        work[pos(x,y)] = vec4(0, 0, 0, float(sum));
    }
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        float sum = 2.0 * work[pos(x,y)].w;
        if (y > 0) sum += work[pos(x,y-1)].w;
        if (y+1 < height) sum += work[pos(x,y+1)].w;
        int horizontalWeight = 2 + int(x > 0) + int(x+1 < width);
        int verticalWeight = 2 + int(y > 0) + int(y+1 < height);
        work[pos(x,y)].x = sum / float(horizontalWeight * verticalWeight);
    }
    for (int i = 0; i < size; ++i) work[i].w = 0.0;
    for (int y = 1; y < height-1; ++y) for (int x = 1; x < width-1; ++x) {
        float gx = -blur(x-1,y-1)+blur(x+1,y-1)-2*blur(x-1,y)+2*blur(x+1,y)-blur(x-1,y+1)+blur(x+1,y+1);
        float gy = -blur(x-1,y-1)-2*blur(x,y-1)-blur(x+1,y-1)+blur(x-1,y+1)+2*blur(x,y+1)+blur(x+1,y+1);
        work[pos(x,y)].yz = vec2(length(vec2(gx,gy)), gx == 0.0 ? 90.0 : mod(degrees(atan(gy,gx))+180.0,180.0));
    }
    float peak = 0;
    for (int y = 2; y < height-2; ++y) for (int x = 2; x < width-2; ++x) {
        int p = pos(x,y);
        float a = work[p].z, current = work[p].y;
        ivec2 d = a < 22.5 || a >= 157.5 ? ivec2(1,0) :
                  a < 67.5 ? ivec2(1,1) : a < 112.5 ? ivec2(0,1) : ivec2(1,-1);
        if (current >= work[pos(x+d.x,y+d.y)].y && current >= work[pos(x-d.x,y-d.y)].y) {
            work[p].w = current;
            peak = max(peak,current);
        }
    }
    uint tail = 0;
    for (int i = 0; i < size; ++i) {
        float strength = work[i].w;
        result[i] = 0u;
        if (strength > 0.0 && strength >= peak * 0.18) {
            result[i] = 1u;
            pending[tail++] = uint(i);
        } else if (strength > 0.0 && strength >= peak * 0.08) {
            result[i] = 2u;
        }
    }
    for (uint head = 0; head < tail; ++head) {
        int p = int(pending[head]);
        int x = p % width, y = p / width;
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            if (inside(x+dx,y+dy)) {
                int q = pos(x+dx,y+dy);
                if (result[q] == 2u) {
                    result[q] = 1u;
                    pending[tail++] = uint(q);
                }
            }
        }
    }
    for (int i = 0; i < size; ++i) if (result[i] == 2u) result[i] = 0u;
}

void main() {
    if (mode == 0) floyd();
    else canny();
}
