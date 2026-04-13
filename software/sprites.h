#include <stdint.h>

uint64_t SMILEY_FACE = 0x004242420081423C;

uint64_t make_sprite(const char *rows[8]) {
    uint64_t result = 0;
    for (int r = 0; r < 8; r++) {
        uint8_t byte = 0;
        for (int c = 0; c < 8; c++) {
            if (rows[r][c] == '#')
                byte |= (1 << c);
        }
        result |= ((uint64_t)byte) << (r * 8);
    }
    return result;
}

const char *smiley[8] = {
    "#.####..",
    ".#....#.",
    "#.#..#.#",
    "#......#",
    "#.#..#.#",
    "#..##..#",
    ".#....#.",
    "..####..",
};

uint64_t alphabet[26] = {
    SPRITE_A, SPRITE_B, SPRITE_C, SPRITE_D, SPRITE_E, SPRITE_F,
    SPRITE_G, SPRITE_H, SPRITE_I, SPRITE_J, SPRITE_K, SPRITE_L,
    SPRITE_M, SPRITE_N, SPRITE_O, SPRITE_P, SPRITE_Q, SPRITE_R,
    SPRITE_S, SPRITE_T, SPRITE_U, SPRITE_V, SPRITE_W, SPRITE_X,
    SPRITE_Y, SPRITE_Z
};

uint64_t get_letter_sprite(char c) {
    if (c >= 'A' && c <= 'Z')
        return alphabet[c - 'A'];
    return 0;
}

const uint64_t SPRITE_A = 0x8181FF818181423CULL;
const uint64_t SPRITE_B = 0x7C82827C8282827CULL;
const uint64_t SPRITE_C = 0x3C4280808080423CULL;
const uint64_t SPRITE_D = 0x7C4241818181427CULL;
const uint64_t SPRITE_E = 0xFF8080FC808080FFULL;
const uint64_t SPRITE_F = 0x808080FC808080FFULL;
const uint64_t SPRITE_G = 0x3C424F808080423CULL;
const uint64_t SPRITE_H = 0x818181FF81818181ULL;
const uint64_t SPRITE_I = 0x3C1818181818183CULL;
const uint64_t SPRITE_J = 0x1E0202020202423CULL;
const uint64_t SPRITE_K = 0x8182447844224181ULL;
const uint64_t SPRITE_L = 0xFF80808080808080ULL;
const uint64_t SPRITE_M = 0x818199A5C3818181ULL;
const uint64_t SPRITE_N = 0x8181A19189858381ULL;
const uint64_t SPRITE_O = 0x3C4281818181423CULL;
const uint64_t SPRITE_P = 0x808080FC828282FCULL;
const uint64_t SPRITE_Q = 0x5C6242818181423CULL;
const uint64_t SPRITE_R = 0x818284F8828282FCULL;
const uint64_t SPRITE_S = 0x3C4280060100423CULL;
const uint64_t SPRITE_T = 0x18181818181818FFULL;
const uint64_t SPRITE_U = 0x3C42818181818181ULL;
const uint64_t SPRITE_V = 0x1824428181818181ULL;
const uint64_t SPRITE_W = 0x8181C3A599818181ULL;
const uint64_t SPRITE_X = 0x8142241818244281ULL;
const uint64_t SPRITE_Y = 0x1818181818244281ULL;
const uint64_t SPRITE_Z = 0xFF804020100804FFULL;

/////// Character alphabet sprites
/*
const char **alphabet[26] = {
    sprite_A, sprite_B, sprite_C, sprite_D, sprite_E, sprite_F,
    sprite_G, sprite_H, sprite_I, sprite_J, sprite_K, sprite_L,
    sprite_M, sprite_N, sprite_O, sprite_P, sprite_Q, sprite_R,
    sprite_S, sprite_T, sprite_U, sprite_V, sprite_W, sprite_X,
    sprite_Y, sprite_Z
};
*/
const char *sprite_A[8] = {
    "..####..",
    ".#....#.",
    "#......#",
    "#......#",
    "########",
    "#......#",
    "#......#",
    "#......#",
};
const char *sprite_B[8] = {
    "#####...",
    "#....#..",
    "#....#..",
    "#####...",
    "#....#..",
    "#....#..",
    "#....#..",
    "#####...",
};
const char *sprite_C[8] = {
    "..####..",
    ".#....#.",
    "#.......",
    "#.......",
    "#.......",
    "#.......",
    ".#....#.",
    "..####..",
};
const char *sprite_D[8] = {
    "#####...",
    "#....#..",
    "#.....#.",
    "#.....#.",
    "#.....#.",
    "#.....#.",
    "#....#..",
    "#####...",
};
const char *sprite_E[8] = {
    "########",
    "#.......",
    "#.......",
    "######..",
    "#.......",
    "#.......",
    "#.......",
    "########",
};
const char *sprite_F[8] = {
    "########",
    "#.......",
    "#.......",
    "######..",
    "#.......",
    "#.......",
    "#.......",
    "#.......",
};
const char *sprite_G[8] = {
    "..####..",
    ".#....#.",
    "#.......",
    "#..####.",
    "#.....#.",
    "#.....#.",
    ".#....#.",
    "..####..",
};
const char *sprite_H[8] = {
    "#......#",
    "#......#",
    "#......#",
    "########",
    "#......#",
    "#......#",
    "#......#",
    "#......#",
};
const char *sprite_I[8] = {
    "..####..",
    "...##...",
    "...##...",
    "...##...",
    "...##...",
    "...##...",
    "...##...",
    "..####..",
};
const char *sprite_J[8] = {
    "...####.",
    ".....#..",
    ".....#..",
    ".....#..",
    ".....#..",
    "#....#..",
    "#....#..",
    ".####...",
};
const char *sprite_K[8] = {
    "#.....#.",
    "#....#..",
    "#...#...",
    "####....",
    "#...#...",
    "#....#..",
    "#.....#.",
    "#......#",
};
const char *sprite_L[8] = {
    "#.......",
    "#.......",
    "#.......",
    "#.......",
    "#.......",
    "#.......",
    "#.......",
    "########",
};
const char *sprite_M[8] = {
    "#......#",
    "##....##",
    "#.#..#.#",
    "#..##..#",
    "#......#",
    "#......#",
    "#......#",
    "#......#",
};
const char *sprite_N[8] = {
    "#......#",
    "##.....#",
    "#.#....#",
    "#..#...#",
    "#...#..#",
    "#....#.#",
    "#.....##",
    "#......#",
};
const char *sprite_O[8] = {
    "..####..",
    ".#....#.",
    "#......#",
    "#......#",
    "#......#",
    "#......#",
    ".#....#.",
    "..####..",
};
const char *sprite_P[8] = {
    "######..",
    "#.....#.",
    "#.....#.",
    "######..",
    "#.......",
    "#.......",
    "#.......",
    "#.......",
};
const char *sprite_Q[8] = {
    "..####..",
    ".#....#.",
    "#......#",
    "#......#",
    "#..#...#",
    "#...#..#",
    ".#....#.",
    "..###.#.",
};
const char *sprite_R[8] = {
    "######..",
    "#.....#.",
    "#.....#.",
    "######..",
    "#...#...",
    "#....#..",
    "#.....#.",
    "#......#",
};
const char *sprite_S[8] = {
    "..####..",
    ".#....#.",
    "#.......",
    "..####..",
    "......#.",
    "......#.",
    ".#....#.",
    "..####..",
};
const char *sprite_T[8] = {
    "########",
    "...##...",
    "...##...",
    "...##...",
    "...##...",
    "...##...",
    "...##...",
    "...##...",
};
const char *sprite_U[8] = {
    "#......#",
    "#......#",
    "#......#",
    "#......#",
    "#......#",
    "#......#",
    ".#....#.",
    "..####..",
};
const char *sprite_V[8] = {
    "#......#",
    "#......#",
    "#......#",
    "#......#",
    ".#....#.",
    ".#....#.",
    "..#..#..",
    "...##...",
};
const char *sprite_W[8] = {
    "#......#",
    "#......#",
    "#......#",
    "#..##..#",
    "#.#..#.#",
    "##....##",
    "#......#",
    "#......#",
};
const char *sprite_X[8] = {
    "#......#",
    ".#....#.",
    "..#..#..",
    "...##...",
    "...##...",
    "..#..#..",
    ".#....#.",
    "#......#",
};
const char *sprite_Y[8] = {
    "#......#",
    ".#....#.",
    "..#..#..",
    "...##...",
    "...##...",
    "...##...",
    "...##...",
    "...##...",
};
const char *sprite_Z[8] = {

    "########",
    ".....#..",
    "....#...",
    "...#....",
    "..#.....",
    ".#......",
    "#.......",
    "########",
};
