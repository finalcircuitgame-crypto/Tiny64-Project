#include "math.h"

#define PI 3.14159265358979323846

double floor(double x) {
    if (x >= 0) return (double)((int)x);
    int i = (int)x;
    return (double)(x == (double)i ? i : i - 1);
}

double ceil(double x) {
    if (x <= 0) return (double)((int)x);
    int i = (int)x;
    return (double)(x == (double)i ? i : i + 1);
}

double fabs(double x) {
    return x < 0 ? -x : x;
}

int abs(int x) {
    return x < 0 ? -x : x;
}

double sqrt(double x) {
    if (x < 0) return 0;
    if (x == 0) return 0;
    
    double res = x;
    double last = 0;
    while (res != last) {
        last = res;
        res = (res + x / res) / 2;
    }
    return res;
}

double fmod(double x, double y) {
    if (y == 0.0) return 0.0;
    double quot = x / y;
    int iquot = (int)quot;
    return x - iquot * y;
}

double cos(double x) {
    // Normalize to -PI to PI
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    
    double x2 = x * x;
    return 1.0 - (x2 / 2.0) + (x2 * x2 / 24.0) - (x2 * x2 * x2 / 720.0) + (x2 * x2 * x2 * x2 / 40320.0);
}

double sin(double x) {
    // Normalize to -PI to PI
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    
    double x2 = x * x;
    double x3 = x2 * x;
    double x5 = x3 * x2;
    double x7 = x5 * x2;
    return x - (x3 / 6.0) + (x5 / 120.0) - (x7 / 5040.0);
}

double acos(double x) {
    if (x < -1.0) x = -1.0;
    if (x > 1.0) x = 1.0;
    
    // Approximation: acos(x) = PI/2 - asin(x)
    // asin(x) approx x + x^3/6 + 3x^5/40
    double x2 = x * x;
    double x3 = x2 * x;
    double x5 = x3 * x2;
    double asin_x = x + (x3 / 6.0) + (3.0 * x5 / 40.0);
    return (PI / 2.0) - asin_x;
}