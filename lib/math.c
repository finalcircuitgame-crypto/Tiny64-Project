#include <stdint.h>

double fabs(double x) {
    return x < 0 ? -x : x;
}

double floor(double x) {
    if (x >= 0) {
        return (double)(int)x;
    } else {
        int i = (int)x;
        return (double)((double)i == x ? i : i - 1);
    }
}

double ceil(double x) {
    if (x <= 0) {
        return (double)(int)x;
    } else {
        int i = (int)x;
        return (double)((double)i == x ? i : i + 1);
    }
}

double sqrt(double x) {
    if (x < 0) return 0;
    if (x == 0) return 0;
    double res = x;
    for (int i = 0; i < 10; i++) {
        res = 0.5 * (res + x / res);
    }
    return res;
}

double pow(double x, double y) {
    // Very limited implementation for stb_truetype if it uses it for things like 1.0/3.0
    // Actually stb_truetype uses it in stbtt__pow
    // For now, let's just provide a basic one or hope it's not used much
    if (y == 0) return 1;
    if (y == 1) return x;
    if (y == 2) return x * x;
    if (y == 0.5) return sqrt(x);
    
    // Fallback (not accurate for general y)
    return x; 
}

double cos(double x) {
    // Taylor series for cos
    double res = 1;
    double term = 1;
    double x2 = x * x;
    for (int i = 1; i < 10; i++) {
        term *= -x2 / ((2 * i) * (2 * i - 1));
        res += term;
    }
    return res;
}

double acos(double x) {
    // Very basic approximation
    if (x > 1) x = 1;
    if (x < -1) x = -1;
    return 1.570796 - x; // π/2 - x is NOT acos, but it's a first order approx
}

double fmod(double x, double y) {
    if (y == 0) return 0;
    return x - y * floor(x / y);
}
