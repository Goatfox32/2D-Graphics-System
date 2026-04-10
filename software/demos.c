
#include "comm.h"
#include "demos.h"

#define W 320
#define H 240
#define CELL 4
#define GW (W / CELL)   /* 80 */
#define GH (H / CELL)   /* 60 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned char grid[GH][GW];
static unsigned char next[GH][GW];


static float verts[8][3] = {
    {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
    {-1,-1, 1},{1,-1, 1},{1,1, 1},{-1, 1, 1}
};

/* 6 faces as quad indices, each split into 2 tris */
static int faces[6][4] = {
    {0,1,2,3},{4,5,6,7},{0,1,5,4},
    {2,3,7,6},{0,3,7,4},{1,2,6,5}
};

/* Per-face colors (r,g,b) in RGB565 range */
static int facecolor[6][3] = {
    {31,0,0},{0,63,0},{0,0,31},
    {31,63,0},{31,0,31},{0,63,31}
};

static void project(float x, float y, float z, int *sx, int *sy) {
    float d = 4.0f + z; /* simple perspective */
    *sx = (int)(W/2 + x * 200.0f / d);
    *sy = (int)(H/2 + y * 200.0f / d);
}

static void rotate(float *x, float *y, float *z, float ax, float ay) {
    float t;
    /* rotate Y */
    t  = *x * cosf(ay) - *z * sinf(ay);
    *z = *x * sinf(ay) + *z * cosf(ay);
    *x = t;
    /* rotate X */
    t  = *y * cosf(ax) - *z * sinf(ax);
    *z = *y * sinf(ax) + *z * cosf(ax);
    *y = t;
}

static void seed_random(void) {
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            grid[y][x] = (rand() & 7) == 0;  /* ~12% alive */
}

/* Optional: drop a glider gun in the top-left so there's perpetual action */
static void add_gosper_gun(int ox, int oy) {
    static const char *pat[] = {
        "........................O...........",
        "......................O.O...........",
        "............OO......OO............OO",
        "...........O...O....OO............OO",
        "OO........O.....O...OO..............",
        "OO........O...O.OO....O.O...........",
        "..........O.....O.......O...........",
        "...........O...O....................",
        "............OO......................",
    };
    int rows = sizeof(pat) / sizeof(pat[0]);
    for (int y = 0; y < rows; y++)
        for (int x = 0; pat[y][x]; x++)
            if (pat[y][x] == 'O' && oy + y < GH && ox + x < GW)
                grid[oy + y][ox + x] = 1;
}

static void step(void) {
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            int n = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = (x + dx + GW) % GW;  /* toroidal */
                    int ny = (y + dy + GH) % GH;
                    n += grid[ny][nx];
                }
            }
            next[y][x] = grid[y][x] ? (n == 2 || n == 3) : (n == 3);
        }
    }
    memcpy(grid, next, sizeof(grid));
}

/* Draw a filled cell as two triangles. Color shifts with generation. */
static void draw_cell(int gx, int gy, int r, int g, int b) {
    int x0 = gx * CELL;
    int y0 = gy * CELL;
    int x1 = x0 + CELL - 1;
    int y1 = y0 + CELL - 1;
    /* Upper-left triangle */
    draw_triangle(x0, y0, r, g, b,
                  x1, y0, r, g, b,
                  x0, y1, r, g, b);
    /* Lower-right triangle */
    draw_triangle(x1, y0, r, g, b,
                  x1, y1, r, g, b,
                  x0, y1, r, g, b);
}

void spinning_cube_demo(int *exit) {
    int frame = 0;
    while (!*exit) {
        clear();
        float t = frame * 0.03f;
        float ax = t * 0.7f, ay = t;

        /* Transform vertices */
        float rv[8][3];
        int sv[8][2];
        for (int i = 0; i < 8; i++) {
            rv[i][0] = verts[i][0];
            rv[i][1] = verts[i][1];
            rv[i][2] = verts[i][2];
            rotate(&rv[i][0], &rv[i][1], &rv[i][2], ax, ay);
            project(rv[i][0], rv[i][1], rv[i][2], &sv[i][0], &sv[i][1]);
        }

        /* Draw faces — simple back-face cull via cross product */
        for (int f = 0; f < 6; f++) {
            int *q = faces[f];
            /* Cross product of two edges in screen space */
            int ax2 = sv[q[1]][0] - sv[q[0]][0];
            int ay2 = sv[q[1]][1] - sv[q[0]][1];
            int bx  = sv[q[2]][0] - sv[q[0]][0];
            int by  = sv[q[2]][1] - sv[q[0]][1];
            if (ax2 * by - ay2 * bx <= 0) continue; /* back-facing */

            int *c = facecolor[f];
            draw_triangle(sv[q[0]][0], sv[q[0]][1], c[0], c[1], c[2],
                          sv[q[1]][0], sv[q[1]][1], c[0], c[1], c[2],
                          sv[q[2]][0], sv[q[2]][1], c[0], c[1], c[2]);
            draw_triangle(sv[q[2]][0], sv[q[2]][1], c[0], c[1], c[2],
                          sv[q[3]][0], sv[q[3]][1], c[0], c[1], c[2],
                          sv[q[0]][0], sv[q[0]][1], c[0], c[1], c[2]);
        }

        frame++;
        usleep(33000);
    }
}

void game_of_life_demo(int *exit) {
    srand(1);
    memset(grid, 0, sizeof(grid));
    seed_random();
    add_gosper_gun(2, 2);

    int gen = 0;
    while (!*exit) {
        clear();

        /* Cycle hue over generations so the field shimmers as it evolves.
           RGB565-ish ranges: r,b in [0,31], g in [0,63]. */
        int r = 16 + (gen * 1) % 16;
        int g = 32 + (gen * 2) % 32;
        int b = 16 + (gen * 3) % 16;

        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++)
                if (grid[y][x])
                    draw_cell(x, y, r, g, b);

        step();
        gen++;
        usleep(3300);  /* ~12 fps */
    }
}

void spinning_triangles_demo(int *exit) {
    int frame = 0;
    while (!*exit) {
        clear();

        float t = frame * 0.01f;

        /* Three triangles orbiting a common center, each pulsing in size
        and cycling through colors out of phase. */
        for (int i = 0; i < 3; i++) {
            float phase = t + i * (2.0f * M_PI / 3.0f);
            float cx = W / 2 + cosf(phase) * 70.0f;
            float cy = H / 2 + sinf(phase * 1.3f) * 50.0f;
            float size = 30.0f + 20.0f * sinf(t * 2.0f + i);
            float rot  = phase * 2.0f;

            /* Equilateral triangle vertices around (cx,cy) */
            int x0 = (int)(cx + size * cosf(rot));
            int y0 = (int)(cy + size * sinf(rot));
            int x1 = (int)(cx + size * cosf(rot + 2.0944f)); /* +120° */
            int y1 = (int)(cy + size * sinf(rot + 2.0944f));
            int x2 = (int)(cx + size * cosf(rot + 4.1888f)); /* +240° */
            int y2 = (int)(cy + size * sinf(rot + 4.1888f));

            /* Color cycling: each vertex leads a different channel */
            int r0 = (int)(15.5f + 15.5f * sinf(t + i));
            int g0 = (int)(31.5f + 31.5f * sinf(t * 1.1f + i + 2.0f));
            int b0 = (int)(15.5f + 15.5f * sinf(t * 0.9f + i + 4.0f));

            int r1 = (int)(15.5f + 15.5f * sinf(t + i + 1.0f));
            int g1 = (int)(31.5f + 31.5f * sinf(t * 1.1f + i + 3.0f));
            int b1 = (int)(15.5f + 15.5f * sinf(t * 0.9f + i + 5.0f));

            int r2 = (int)(15.5f + 15.5f * sinf(t + i + 2.0f));
            int g2 = (int)(31.5f + 31.5f * sinf(t * 1.1f + i + 4.0f));
            int b2 = (int)(15.5f + 15.5f * sinf(t * 0.9f + i + 6.0f));

            draw_triangle(x0, y0, r0, g0, b0,
                        x1, y1, r1, g1, b1,
                        x2, y2, r2, g2, b2);
        }

        frame++;
        usleep(33000); /* ~30 fps */
    }
}