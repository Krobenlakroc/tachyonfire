
// layout(early_fragment_tests) in;

// layout(binding=2) uniform sampler2D depthTex;
// layout(binding=3) uniform sampler2D normalTex;


in vec4 sphere;
flat in uint light_id;

//8160 divided by 4
// #define NUM_TILED_DIV4 2040

//2048 lights, divided by 32 bits is 64 uints for a bitmask for all lights
//8160 tiles times 64 divided by 4
// #define NUM_UINTS_DIV4 130560
// #define NUM_TILES_X 120


#define TILE_MAX_LIGHTS 2048
#define TILE_WORDS 64

layout (std430 , binding = 3) restrict buffer TileLightLists
{
    uvec4 light_list_arr[NUM_UINTS_DIV4];
};

layout (std430 , binding = 2) restrict readonly buffer TileData
{
    vec4 depth_boundsMin[NUM_TILED_DIV4];
    vec4 depth_boundsMax[NUM_TILED_DIV4];
    uvec4 cullMask[NUM_TILED_DIV4];
    vec4 tileMinx[NUM_TILED_DIV4];
    vec4 tileMaxx[NUM_TILED_DIV4];
    vec4 tileMiny[NUM_TILED_DIV4];
    vec4 tileMaxy[NUM_TILED_DIV4];
    vec4 tileMinz[NUM_TILED_DIV4];
    vec4 tileMaxz[NUM_TILED_DIV4];
};


bool pointLightIntersectsCluster(
    float cx, float cy, float cz, float r2,
    float minx, float miny, float minz,
    float maxx, float maxy, float maxz)
{

    if (cx < maxx && cx > minx &&
        cy < maxy && cy > miny &&
        cz < maxz && cz > minz  )
    {
        return true;
    }
    // Clamp center to box
    float qx = (cx < minx ? minx : (cx > maxx ? maxx : cx));
    float qy = (cy < miny ? miny : (cy > maxy ? maxy : cy));
    float qz = (cz < minz ? minz : (cz > maxz ? maxz : cz));
    float dx = qx - cx, dy = qy - cy, dz = qz - cz;
    return dx*dx + dy*dy + dz*dz <= r2;
}

void main()
{
    //check for intersection w/aabb
    uint tile_x = uint(gl_FragCoord.x);
    uint tile_y = uint(gl_FragCoord.y);

    uint tile_id = tile_x + tile_y*NUM_TILES_X;

    vec3 AABBmin,AABBmax;
    AABBmin.x = tileMinx[tile_id/4][tile_id%4];
    AABBmin.y = tileMiny[tile_id/4][tile_id%4];
    AABBmin.z = tileMinz[tile_id/4][tile_id%4];

    AABBmax.x = tileMaxx[tile_id/4][tile_id%4];
    AABBmax.y = tileMaxy[tile_id/4][tile_id%4];
    AABBmax.z = tileMaxz[tile_id/4][tile_id%4];

    uint geomMask = cullMask[tile_id/4][tile_id%4];

    float depth_min = depth_boundsMin[tile_id/4][tile_id%4];
    float depth_max = depth_boundsMax[tile_id/4][tile_id%4];

    float fMin = -sphere.z - sphere.w;

    float fMax = -sphere.z + sphere.w;

    float depthRangeRecip = 32.0 / (depth_max - depth_min);

    uint lightMaskcellindexSTART = uint(max(0, min(31, floor((fMin - depth_min) * depthRangeRecip))));

    uint lightMaskcellindexEND = uint(max(0, min(31, floor((fMax - depth_min) * depthRangeRecip))));

    uint lightMask = 0xFFFFFFFF;
    lightMask >>= 31 - (lightMaskcellindexEND - lightMaskcellindexSTART);
    lightMask <<= lightMaskcellindexSTART;

    bool intersect2_5d = bool(geomMask & lightMask);

    /*
     * CULL
     */
    //aabbs in view space
//     if (!pointLightIntersectsCluster(sphere.x,sphere.y,sphere.z,sphere.w,AABBmin.x,AABBmin.y,AABBmin.z,AABBmax.x,AABBmax.y,AABBmax.z) || !intersect2_5d)
//     {
//         discard;
//     }
    // ||
    if (!pointLightIntersectsCluster(sphere.x,sphere.y,sphere.z,sphere.w*sphere.w,AABBmin.x,AABBmin.y,AABBmin.z,AABBmax.x,AABBmax.y,AABBmax.z) || !intersect2_5d)
    {
        discard;
    }


//     uint idx = atomicAdd(light_count[tile_id],1);
//
//     if (idx < TILE_MAX_LIGHTS)
//     {
    uint word_idx = light_id / 32;
    uint bit_idx = light_id % 32;

    uint mask = 1u << bit_idx;

    uint llidx = tile_id*TILE_WORDS + word_idx;
    atomicOr(light_list_arr[llidx/4][llidx%4], mask);
//     light_list_arr[tile_id*TILE_MAX_LIGHTS + idx] = light_id;
    //}



}
