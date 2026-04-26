#pragma once
#include <vector>
#include <numeric>
#include <random>
#include <cmath>
#include <algorithm>

class PerlinNoise {
private:
    std::vector<int> p;

    float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    float lerp(float t, float a, float b) { return a + t * (b - a); }
    
    // Bulletproof 2D gradient mapping
    float grad(int hash, float x, float y) {
        switch(hash & 3) {
            case 0: return x + y;
            case 1: return -x + y;
            case 2: return x - y;
            case 3: return -x - y;
            default: return 0.0f;
        }
    }

public:
    PerlinNoise(unsigned int seed = 123) {
        p.resize(256);
        std::iota(p.begin(), p.end(), 0);
        std::default_random_engine engine(seed);
        std::shuffle(p.begin(), p.end(), engine);
        p.insert(p.end(), p.begin(), p.end());
    }

    float noise(float x, float y) {
        int X = (int)std::floor(x) & 255;
        int Y = (int)std::floor(y) & 255;
        x -= std::floor(x);
        y -= std::floor(y);
        float u = fade(x);
        float v = fade(y);
        
        int A = p[X] + Y;
        int B = p[X + 1] + Y;
        
        return lerp(v, lerp(u, grad(p[A], x, y), grad(p[B], x - 1.0f, y)),
                       lerp(u, grad(p[A + 1], x, y - 1.0f), grad(p[B + 1], x - 1.0f, y - 1.0f)));
    }
};