#include "vector_font.h"

// Very basic vector font data for a few characters to demonstrate scalability
static const stroke_t stroke_A[] = {{2, 10, 5, 0}, {5, 0, 8, 10}, {3, 6, 7, 6}};
static const stroke_t stroke_B[] = {{2, 0, 2, 10}, {2, 0, 7, 1}, {7, 1, 7, 4}, {7, 4, 2, 5}, {2, 5, 8, 6}, {8, 6, 8, 9}, {8, 9, 2, 10}};
static const stroke_t stroke_C[] = {{8, 2, 5, 0}, {5, 0, 2, 3}, {2, 3, 2, 7}, {2, 7, 5, 10}, {5, 10, 8, 8}};
static const stroke_t stroke_D[] = {{2, 0, 2, 10}, {2, 0, 6, 0}, {6, 0, 8, 3}, {8, 3, 8, 7}, {8, 7, 6, 10}, {6, 10, 2, 10}};
static const stroke_t stroke_E[] = {{8, 0, 2, 0}, {2, 0, 2, 10}, {2, 10, 8, 10}, {2, 5, 6, 5}};
static const stroke_t stroke_T[] = {{0, 0, 10, 0}, {5, 0, 5, 10}};
static const stroke_t stroke_I[] = {{3, 0, 7, 0}, {5, 0, 5, 10}, {3, 10, 7, 10}};
static const stroke_t stroke_N[] = {{2, 10, 2, 0}, {2, 0, 8, 10}, {8, 10, 8, 0}};
static const stroke_t stroke_Y[] = {{0, 0, 5, 5}, {10, 0, 5, 5}, {5, 5, 5, 10}};
static const stroke_t stroke_6[] = {{8, 1, 3, 3}, {3, 3, 2, 6}, {2, 6, 2, 9}, {2, 9, 5, 10}, {5, 10, 8, 9}, {8, 9, 8, 6}, {8, 6, 2, 6}};
static const stroke_t stroke_4[] = {{7, 10, 7, 0}, {7, 0, 1, 7}, {1, 7, 9, 7}};
static const stroke_t stroke_space[] = {};

const glyph_t vector_font[128] = {
    ['A'] = {stroke_A, 3},
    ['B'] = {stroke_B, 7},
    ['C'] = {stroke_C, 5},
    ['D'] = {stroke_D, 6},
    ['E'] = {stroke_E, 4},
    ['T'] = {stroke_T, 2},
    ['I'] = {stroke_I, 3},
    ['N'] = {stroke_N, 3},
    ['Y'] = {stroke_Y, 3},
    ['6'] = {stroke_6, 7},
    ['4'] = {stroke_4, 3},
    [' '] = {stroke_space, 0},
};
