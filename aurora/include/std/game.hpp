#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Light types ── */
#define AURORA_LIGHT_DIRECTIONAL 0
#define AURORA_LIGHT_POINT       1
#define AURORA_LIGHT_SPOT        2
#define AURORA_LIGHT_AMBIENT     3

/* ── Lighting ── */
typedef struct AuroraLight {
    int    type;
    float  r, g, b;
    float  intensity;
    float  x, y, z;
    float  dx, dy, dz;
    float  range;
    float  spot_inner, spot_outer;
    int    active;
} AuroraLight;

AuroraLight* aurora_light_create(int type, float r, float g, float b, float intensity);
void         aurora_light_destroy(AuroraLight* light);
void         aurora_light_set_position(AuroraLight* light, float x, float y, float z);
void         aurora_light_set_direction(AuroraLight* light, float x, float y, float z);
void         aurora_light_set_color(AuroraLight* light, float r, float g, float b);
void         aurora_light_set_intensity(AuroraLight* light, float intensity);
void         aurora_light_set_range(AuroraLight* light, float range);
void         aurora_light_set_spot_angle(AuroraLight* light, float inner, float outer);
int          aurora_light_get_count(void);
AuroraLight* aurora_light_get(int index);

/* ── Tilemap ── */
typedef struct AuroraTilemap {
    int    cols, rows;
    int    tile_w, tile_h;
    int*   tiles;        /* flat array cols * rows */
    int    num_layers;
    int**  layers;
    int    props_count;
    char** prop_keys;
    char** prop_values;
} AuroraTilemap;

AuroraTilemap* aurora_tilemap_create(int cols, int rows, int tile_w, int tile_h, int layers);
void           aurora_tilemap_destroy(AuroraTilemap* tm);
void           aurora_tilemap_set_tile(AuroraTilemap* tm, int col, int row, int layer, int tile_id);
int            aurora_tilemap_get_tile(AuroraTilemap* tm, int col, int row, int layer);
int            aurora_tilemap_get_width(AuroraTilemap* tm);
int            aurora_tilemap_get_height(AuroraTilemap* tm);
int            aurora_tilemap_get_cols(AuroraTilemap* tm);
int            aurora_tilemap_get_rows(AuroraTilemap* tm);
int            aurora_tilemap_is_solid(AuroraTilemap* tm, int col, int row, int layer);
void           aurora_tilemap_set_property(AuroraTilemap* tm, const char* key, const char* value);

/* ── Mesh primitives ── */
typedef struct AuroraMesh {
    int             vertex_count;
    float*          vertices;   /* interleaved pos3+norm3+tex2 = 8 floats per vertex */
    int             index_count;
    unsigned short* indices;
    int             stride;     /* 8 floats */
} AuroraMesh;

AuroraMesh* aurora_mesh_create_plane(float width, float depth);
AuroraMesh* aurora_mesh_create_sphere(float radius, int segments);
AuroraMesh* aurora_mesh_create_cylinder(float radius, float height, int segments);
AuroraMesh* aurora_mesh_create_capsule(float radius, float height, int segments);
int         aurora_mesh_get_vertex_count(AuroraMesh* mesh);
const float* aurora_mesh_get_vertex_data(AuroraMesh* mesh);
int         aurora_mesh_get_index_count(AuroraMesh* mesh);
const unsigned short* aurora_mesh_get_index_data(AuroraMesh* mesh);
void        aurora_mesh_destroy(AuroraMesh* mesh);

#ifdef __cplusplus
}
#endif
