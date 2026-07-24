

#include "th_liquidgen.h"
#include "th_system.h"
#include <string.h>

//the whole sim is AI generated (w/ minor tweaks)
//meshing is by me

#define GRID_W        32
#define GRID_H        16
#define CELL_SIZE     1.0f
#define GRAVITY       9.81f
#define DAMPING       0.9995f
// #define MIN_HEIGHT    0.0001f
#define MIN_HEIGHT 0.00
#define MAX_HEIGHT 20.0
#define DT            0.02f


#define PIPE_AREA     (CELL_SIZE * CELL_SIZE)


#define SMOOTH_PASSES 20
#define SMOOTH_ALPHA  0.15f

#define NOISE_INTERVAL 30
#define NOISE_STRENGTH 0.8f


#define DIR_N 0
#define DIR_E 1
#define DIR_S 2
#define DIR_W 3


#define INDEX_GRID(y,x) ((y)*GRID_W + (x))

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}


void th_initLiquidSim(th_Allocator *alloc, th_LiquidHeights *heights)
{
    heights->grid = th_alloc(alloc,sizeof(th_SimCell)*GRID_W*GRID_H);

    heights->smoothed = th_alloc(alloc,sizeof(float)*GRID_W*GRID_H);

    heights->smoothed_temp = th_alloc(alloc,sizeof(float)*GRID_W*GRID_H);

    heights->dim_w = GRID_W;
    heights->dim_h = GRID_H;

    memset(heights->grid, 0, sizeof(th_SimCell)*GRID_W*GRID_H);

    memset(heights->smoothed, 0, sizeof(float)*GRID_W*GRID_H);

    memset(heights->smoothed_temp, 0, sizeof(float)*GRID_W*GRID_H);

    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            heights->grid[INDEX_GRID(y,x)].height = 1.0f;
}



void th_addNoiseLiquidSim(th_LiquidHeights *s,float strength)
{
    strength = strength == 0.0 ? NOISE_STRENGTH : strength;
    int count = 4 + th_random() % 5;     /* 4–8 random splashes                */
    for (int i = 0; i < count; i++) {
        int x = rand() % GRID_W;
        int y = rand() % GRID_H;
        float delta = th_randomFloat(-strength,strength);
        s->grid[INDEX_GRID(y,x)].height += delta;
        if (s->grid[INDEX_GRID(y,x)].height < 0.0f)
            s->grid[INDEX_GRID(y,x)].height = 0.0f;
    }
}

// void th_addCentalLiftLiquidSim(th_LiquidHeights *s,float strength,int rad)
// {
//     strength = strength == 0.0 ? NOISE_STRENGTH : strength;
//     rad = rad == 0 ? 4 : rad ;
//     for (int y = 0; y < GRID_H; y++) {
//         for (int x = 0; x < GRID_W; x++) {
//         float delta = 0;
//         if (x > GRID_W/2 - rad && x < GRID_W/2 + rad && y > GRID_H/2 - rad && y < GRID_H/2 + rad )
//         {
//             delta = strength;
//         }
//
//         s->grid[INDEX_GRID(y,x)].height += delta;
//         if (s->grid[INDEX_GRID(y,x)].height < 0.0f)
//             s->grid[INDEX_GRID(y,x)].height = 0.0f;
//         }
//     }
// }

void th_addCentalLiftLiquidSim(th_LiquidHeights *s, float strength, int rad)
{
    strength = strength == 0.0f ? NOISE_STRENGTH : strength;
    rad      = rad == 0 ? 4 : rad;

    int cx = GRID_W / 2;
    int cy = GRID_H / 2;


    int lifted_count = 0;
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (x > cx - rad && x < cx + rad && y > cy - rad && y < cy + rad) {
                s->grid[INDEX_GRID(y,x)].height += strength;
                lifted_count++;
            }
        }
    }


    float total_added   = strength * lifted_count;
    int   outer_count   = GRID_W * GRID_H - lifted_count;
    float per_outer     = total_added / (float)outer_count;

    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (!(x > cx - rad && x < cx + rad && y > cy - rad && y < cy + rad)) {
                s->grid[INDEX_GRID(y,x)].height -= per_outer;
            }
        }
    }
}



void sim_update_flow(th_LiquidHeights *s,float dt)
{
    th_SimCell* g = s->grid;

    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {

            float h_self = g[INDEX_GRID(y,x)].height;

            /* Neighbour heights (boundary = same height → no flow out) */
            float h_n = (y > 0)          ? g[INDEX_GRID(y-1,x)].height : h_self;
            float h_e = (x < GRID_W-1)   ? g[INDEX_GRID(y,x+1)].height : h_self;
            float h_s = (y < GRID_H-1)   ? g[INDEX_GRID(y+1,x)].height : h_self;
            float h_w = (x > 0)          ? g[INDEX_GRID(y,x-1)].height : h_self;

            /* Acceleration from height gradient */
            float accel = dt * PIPE_AREA * GRAVITY / CELL_SIZE;

            g[INDEX_GRID(y,x)].flow[DIR_N] += accel * (h_self - h_n);
            g[INDEX_GRID(y,x)].flow[DIR_E] += accel * (h_self - h_e);
            g[INDEX_GRID(y,x)].flow[DIR_S] += accel * (h_self - h_s);
            g[INDEX_GRID(y,x)].flow[DIR_W] += accel * (h_self - h_w);

            /* Damp */
            for (int d = 0; d < 4; d++)
                g[INDEX_GRID(y,x)].flow[d] *= DAMPING;




            if (h_self > MAX_HEIGHT) {
                float overshoot = h_self - MAX_HEIGHT;
                // Push flow outward proportional to overshoot — acts like a ceiling pushing down
                float reflect_accel = overshoot * GRAVITY * dt;
                g[INDEX_GRID(y,x)].flow[DIR_N] += reflect_accel;
                g[INDEX_GRID(y,x)].flow[DIR_E] += reflect_accel;
                g[INDEX_GRID(y,x)].flow[DIR_S] += reflect_accel;
                g[INDEX_GRID(y,x)].flow[DIR_W] += reflect_accel;
            }

            if (h_self < MIN_HEIGHT) {
                float undershoot = MIN_HEIGHT - h_self;
                // Pull flow inward — acts like a floor pushing up
                float reflect_accel = undershoot * GRAVITY * dt;
                g[INDEX_GRID(y,x)].flow[DIR_N] -= reflect_accel;
                g[INDEX_GRID(y,x)].flow[DIR_E] -= reflect_accel;
                g[INDEX_GRID(y,x)].flow[DIR_S] -= reflect_accel;
                g[INDEX_GRID(y,x)].flow[DIR_W] -= reflect_accel;
            }


        }
    }
}



void sim_update_heights(th_LiquidHeights *s,float dt)
{
    th_SimCell* g = s->grid;

    /* Opposite direction map */
    static const int opp[4] = { DIR_S, DIR_W, DIR_N, DIR_E };
    /* dx, dy for each direction */
    static const int dx[4] = {  0, 1, 0, -1 };
    static const int dy[4] = { -1, 0, 1,  0 };

    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {

            float out = g[INDEX_GRID(y,x)].flow[DIR_N]
            + g[INDEX_GRID(y,x)].flow[DIR_E]
            + g[INDEX_GRID(y,x)].flow[DIR_S]
            + g[INDEX_GRID(y,x)].flow[DIR_W];

            float in = 0.0f;
            for (int d = 0; d < 4; d++) {
                int nx = x + dx[d];
                int ny = y + dy[d];
                if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H)
                    in += g[INDEX_GRID(ny,nx)].flow[opp[d]];
            }

            float delta_vol = dt * (in - out);
            g[INDEX_GRID(y,x)].height += delta_vol / (CELL_SIZE * CELL_SIZE);

            // /* Guard against floating-point negatives */
            // if (g[INDEX_GRID(y,x)].height < 0.0f)
            //     g[INDEX_GRID(y,x)].height = 0.0f;
        }
    }
}



void sim_smooth_heights(th_LiquidHeights *s)
{
    /* Copy raw heights into smooth buffer */
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            s->smoothed[INDEX_GRID(y,x)] = s->grid[INDEX_GRID(y,x)].height;


    for (int pass = 0; pass < SMOOTH_PASSES; pass++) {
        for (int y = 0; y < GRID_H; y++) {
            for (int x = 0; x < GRID_W; x++) {
                float sum = 0.0f;
                int   cnt = 0;

                if (y > 0)          { sum += s->smoothed[INDEX_GRID(y-1,x)]; cnt++; }
                if (y < GRID_H-1)   { sum += s->smoothed[INDEX_GRID(y+1,x)]; cnt++; }
                if (x > 0)          { sum += s->smoothed[INDEX_GRID(y,x-1)]; cnt++; }
                if (x < GRID_W-1)   { sum += s->smoothed[INDEX_GRID(y,x+1)]; cnt++; }

                float avg = (cnt > 0) ? (sum / cnt) : s->smoothed[INDEX_GRID(y,x)];
                s->smoothed_temp[INDEX_GRID(y,x)] = s->smoothed[INDEX_GRID(y,x)] * (1.0f - SMOOTH_ALPHA)
                + avg               *  SMOOTH_ALPHA;
            }
        }
        memcpy(s->smoothed, s->smoothed_temp, sizeof(float)*GRID_W*GRID_H);
    }
}

/* ------------------------------------------------------------------ */
/*  Full simulation step                                               */
/* ------------------------------------------------------------------ */

void th_updateLiquidSim( th_LiquidHeights *heights,float dt)
{
    float dt_sec = dt*0.001;
    sim_update_flow(heights,dt_sec);
    sim_update_heights(heights,dt_sec);
    sim_smooth_heights(heights);
}

th_GpuData th_generateLiquidMesh(th_Allocator          *alloc,
                                  th_LiquidHeights *heights,fn_mat4 transform)
{



    int verts    = heights->dim_w*heights->dim_h;
    int inds     = (heights->dim_w - 1)*(heights->dim_h - 1)*2*3;

    th_Vertex *data    = th_alloc(alloc, verts * sizeof(th_Vertex));
    GLuint    *indices = th_alloc(alloc, inds  * sizeof(GLuint));

    int indxs = 0;
    for (int y = 0; y < GRID_H - 1; y++) {
        for (int x = 0; x < GRID_W - 1; x++) {
            indices[indxs++] = INDEX_GRID(y,x);
            indices[indxs++] = INDEX_GRID(y,x + 1);
            indices[indxs++] = INDEX_GRID(y + 1,x);

            indices[indxs++] = INDEX_GRID(y + 1,x);
            indices[indxs++] = INDEX_GRID(y,x + 1);
            indices[indxs++] = INDEX_GRID(y + 1,x + 1);
        }
    }


    float aspect = (float)GRID_W/(float)GRID_H;

    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            float h = fn_clamp(heights->smoothed[INDEX_GRID(y,x)],MIN_HEIGHT,MAX_HEIGHT)/MAX_HEIGHT;

            float x_interp = (float)x / (float)(GRID_W - 1.0);
            float y_interp = (float)y / (float)(GRID_H - 1.0);

            data[INDEX_GRID(y,x)].texCoord =
            fn_createVec2((float)(x_interp),
                          (float)(y_interp));


            data[INDEX_GRID(y,x)].position  = fn_createVec3((x_interp*2.0 - 1.0)*1.0*aspect,-h*1.0,(y_interp*2.0 - 1.0)*1.0 );
            data[INDEX_GRID(y,x)].position = fn_transformVec3(data[INDEX_GRID(y,x)].position,transform);

            data[INDEX_GRID(y,x)].normal    = fn_createVec3(0,0,0);
            data[INDEX_GRID(y,x)].tangent   = fn_createVec3(1,0,0);
            data[INDEX_GRID(y,x)].bitangent = fn_cross(data[INDEX_GRID(y,x)].tangent,
                                                      data[INDEX_GRID(y,x)].normal);
        }
    }

    for (int i = 0; i < indxs; i += 3) {
        GLuint ia = indices[i    ];
        GLuint ib = indices[i + 1];
        GLuint ic = indices[i + 2];

        fn_vec3 a = data[ia].position;
        fn_vec3 b = data[ib].position;
        fn_vec3 c = data[ic].position;

        fn_vec3 edge_ab = fn_subVec3(b, a);
        fn_vec3 edge_ac = fn_subVec3(c, a);
        fn_vec3 face_normal = fn_cross(edge_ab, edge_ac);

        data[ia].normal = fn_addVec3(data[ia].normal, face_normal);
        data[ib].normal = fn_addVec3(data[ib].normal, face_normal);
        data[ic].normal = fn_addVec3(data[ic].normal, face_normal);
    }

    for (int y = 0; y < heights->dim_h; y++) {
        for (int x = 0; x < heights->dim_w; x++) {
            int idx = INDEX_GRID(y, x);

            fn_vec3 n = fn_normalizeVec3(data[idx].normal);
            fn_vec3 t = fn_createVec3(1.0f, 0.0f, 0.0f);


            t = fn_normalizeVec3(fn_subVec3(t, fn_multVec3s(n, fn_dot(t, n))));

            data[idx].normal    = n;
            data[idx].tangent   = t;
            data[idx].bitangent = fn_cross(t, n);
        }
    }




    th_GpuData ret;
    ret.verts         = data;
    ret.indices       = indices;
    ret.instances     = th_alloc(alloc, sizeof(fn_mat4));
    ret.instances[0]  = fn_identityMat4();
    ret.vertcount     = verts;
    ret.indicecount   = indxs;
    ret.instancecount = 1;
    return ret;
}
