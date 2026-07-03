#include "std/game.hpp"
#include <cstdlib>
#include <cstring>
#include <cmath>

extern "C" {

/* ═══════════════════════════════════════════
 *  Lighting
 * ═══════════════════════════════════════════ */

#define MAX_LIGHTS 64
static AuroraLight g_lights[MAX_LIGHTS];
static int g_light_count = 0;

AuroraLight* aurora_light_create(int type, float r, float g, float b, float intensity) {
    if (g_light_count >= MAX_LIGHTS) return nullptr;
    AuroraLight* l = &g_lights[g_light_count++];
    l->type      = type;
    l->r = r; l->g = g; l->b = b;
    l->intensity = intensity;
    l->x = l->y = l->z = 0.0f;
    l->dx = 0.0f; l->dy = -1.0f; l->dz = 0.0f;
    l->range     = 10.0f;
    l->spot_inner = 15.0f;
    l->spot_outer = 30.0f;
    l->active    = 1;
    return l;
}

void aurora_light_destroy(AuroraLight* light) {
    if (!light) return;
    int idx = (int)(light - g_lights);
    if (idx < 0 || idx >= g_light_count) return;
    g_light_count--;
    if (idx < g_light_count)
        g_lights[idx] = g_lights[g_light_count];
}

void aurora_light_set_position(AuroraLight* light, float x, float y, float z) {
    if (!light) return;
    light->x = x; light->y = y; light->z = z;
}

void aurora_light_set_direction(AuroraLight* light, float x, float y, float z) {
    if (!light) return;
    float len = sqrtf(x * x + y * y + z * z);
    if (len < 1e-8f) return;
    light->dx = x / len; light->dy = y / len; light->dz = z / len;
}

void aurora_light_set_color(AuroraLight* light, float r, float g, float b) {
    if (!light) return;
    light->r = r; light->g = g; light->b = b;
}

void aurora_light_set_intensity(AuroraLight* light, float intensity) {
    if (!light) return;
    light->intensity = intensity;
}

void aurora_light_set_range(AuroraLight* light, float range) {
    if (!light) return;
    light->range = range;
}

void aurora_light_set_spot_angle(AuroraLight* light, float inner, float outer) {
    if (!light) return;
    light->spot_inner = inner;
    light->spot_outer = outer;
}

int aurora_light_get_count() {
    return g_light_count;
}

AuroraLight* aurora_light_get(int index) {
    if (index < 0 || index >= g_light_count) return nullptr;
    return &g_lights[index];
}

/* ═══════════════════════════════════════════
 *  Tilemap
 * ═══════════════════════════════════════════ */

AuroraTilemap* aurora_tilemap_create(int cols, int rows, int tile_w, int tile_h, int layers) {
    if (cols < 1 || rows < 1 || layers < 1) return nullptr;
    AuroraTilemap* tm = (AuroraTilemap*)calloc(1, sizeof(AuroraTilemap));
    if (!tm) return nullptr;
    tm->cols   = cols;
    tm->rows   = rows;
    tm->tile_w = tile_w;
    tm->tile_h = tile_h;
    tm->num_layers = layers;
    tm->tiles = (int*)calloc((size_t)(cols * rows), sizeof(int));
    if (layers > 1) {
        tm->layers = (int**)calloc((size_t)layers, sizeof(int*));
        for (int i = 0; i < layers; i++)
            tm->layers[i] = (int*)calloc((size_t)(cols * rows), sizeof(int));
    }
    tm->props_count = 0;
    tm->prop_keys   = nullptr;
    tm->prop_values = nullptr;
    return tm;
}

void aurora_tilemap_destroy(AuroraTilemap* tm) {
    if (!tm) return;
    free(tm->tiles);
    if (tm->layers) {
        for (int i = 0; i < tm->num_layers; i++)
            free(tm->layers[i]);
        free(tm->layers);
    }
    for (int i = 0; i < tm->props_count; i++) {
        free(tm->prop_keys[i]);
        free(tm->prop_values[i]);
    }
    free(tm->prop_keys);
    free(tm->prop_values);
    free(tm);
}

void aurora_tilemap_set_tile(AuroraTilemap* tm, int col, int row, int layer, int tile_id) {
    if (!tm || col < 0 || col >= tm->cols || row < 0 || row >= tm->rows) return;
    int idx = row * tm->cols + col;
    if (layer <= 0 || !tm->layers) {
        tm->tiles[idx] = tile_id;
    } else if (layer < tm->num_layers) {
        tm->layers[layer][idx] = tile_id;
    }
}

int aurora_tilemap_get_tile(AuroraTilemap* tm, int col, int row, int layer) {
    if (!tm || col < 0 || col >= tm->cols || row < 0 || row >= tm->rows) return -1;
    int idx = row * tm->cols + col;
    if (layer <= 0 || !tm->layers) return tm->tiles[idx];
    if (layer < tm->num_layers) return tm->layers[layer][idx];
    return -1;
}

int aurora_tilemap_get_width(AuroraTilemap* tm) {
    return tm ? tm->cols * tm->tile_w : 0;
}

int aurora_tilemap_get_height(AuroraTilemap* tm) {
    return tm ? tm->rows * tm->tile_h : 0;
}

int aurora_tilemap_get_cols(AuroraTilemap* tm) {
    return tm ? tm->cols : 0;
}

int aurora_tilemap_get_rows(AuroraTilemap* tm) {
    return tm ? tm->rows : 0;
}

int aurora_tilemap_is_solid(AuroraTilemap* tm, int col, int row, int layer) {
    int tile = aurora_tilemap_get_tile(tm, col, row, layer);
    return tile > 0 ? 1 : 0;
}

void aurora_tilemap_set_property(AuroraTilemap* tm, const char* key, const char* value) {
    if (!tm || !key || !value) return;
    tm->props_count++;
    tm->prop_keys   = (char**)realloc(tm->prop_keys,   (size_t)tm->props_count * sizeof(char*));
    tm->prop_values = (char**)realloc(tm->prop_values, (size_t)tm->props_count * sizeof(char*));
    tm->prop_keys[tm->props_count - 1]   = (char*)malloc(strlen(key) + 1);
    tm->prop_values[tm->props_count - 1] = (char*)malloc(strlen(value) + 1);
    if (tm->prop_keys[tm->props_count - 1])
        strcpy(tm->prop_keys[tm->props_count - 1], key);
    if (tm->prop_values[tm->props_count - 1])
        strcpy(tm->prop_values[tm->props_count - 1], value);
}

/* ═══════════════════════════════════════════
 *  Mesh Primitives
 * ═══════════════════════════════════════════ */

static AuroraMesh* mesh_alloc(int verts, int indices) {
    AuroraMesh* m = (AuroraMesh*)calloc(1, sizeof(AuroraMesh));
    if (!m) return nullptr;
    m->vertex_count = verts;
    m->index_count  = indices;
    m->stride       = 8; /* pos3+norm3+tex2 */
    m->vertices     = (float*)calloc((size_t)(verts * m->stride), sizeof(float));
    if (indices > 0)
        m->indices = (unsigned short*)calloc((size_t)indices, sizeof(unsigned short));
    return m;
}

AuroraMesh* aurora_mesh_create_plane(float width, float depth) {
    AuroraMesh* m = mesh_alloc(4, 6);
    if (!m) return nullptr;
    float hw = width * 0.5f, hd = depth * 0.5f;
    float* v = m->vertices;
    /* pos3+norm3+tex2 per vertex */
    /* 0: top-left  */  v[0] = -hw; v[1] = 0; v[2] = -hd;  v[3]=0; v[4]=1; v[5]=0;  v[6]=0; v[7]=0;
    /* 1: top-right */  v[8] =  hw; v[9] = 0; v[10]= -hd;  v[11]=0; v[12]=1; v[13]=0;  v[14]=1; v[15]=0;
    /* 2: bot-right */  v[16]=  hw; v[17]=0; v[18]=  hd;  v[19]=0; v[20]=1; v[21]=0;  v[22]=1; v[23]=1;
    /* 3: bot-left  */  v[24]= -hw; v[25]=0; v[26]=  hd;  v[27]=0; v[28]=1; v[29]=0;  v[30]=0; v[31]=1;
    unsigned short* idx = m->indices;
    idx[0]=0; idx[1]=1; idx[2]=2;  idx[3]=0; idx[4]=2; idx[5]=3;
    return m;
}

AuroraMesh* aurora_mesh_create_sphere(float radius, int segments) {
    if (segments < 3) segments = 3;
    int verts = (segments + 1) * (segments + 1);
    int idx_count = segments * segments * 6;
    AuroraMesh* m = mesh_alloc(verts, idx_count);
    if (!m) return nullptr;
    float* v = m->vertices;
    unsigned short* idx = m->indices;
    int vi = 0;
    for (int lat = 0; lat <= segments; lat++) {
        float theta = (float)lat * 3.14159265f / (float)segments;
        float sin_t = sinf(theta), cos_t = cosf(theta);
        for (int lon = 0; lon <= segments; lon++) {
            float phi = (float)lon * 2.0f * 3.14159265f / (float)segments;
            float sin_p = sinf(phi), cos_p = cosf(phi);
            v[vi+0] = radius * sin_t * cos_p;
            v[vi+1] = radius * cos_t;
            v[vi+2] = radius * sin_t * sin_p;
            v[vi+3] = sin_t * cos_p;
            v[vi+4] = cos_t;
            v[vi+5] = sin_t * sin_p;
            v[vi+6] = (float)lon / (float)segments;
            v[vi+7] = (float)lat / (float)segments;
            vi += 8;
        }
    }
    int ii = 0;
    for (int lat = 0; lat < segments; lat++) {
        for (int lon = 0; lon < segments; lon++) {
            int a = lat * (segments + 1) + lon;
            int b = a + 1;
            int c = (lat + 1) * (segments + 1) + lon;
            int d = c + 1;
            idx[ii++] = (unsigned short)a;
            idx[ii++] = (unsigned short)c;
            idx[ii++] = (unsigned short)b;
            idx[ii++] = (unsigned short)b;
            idx[ii++] = (unsigned short)c;
            idx[ii++] = (unsigned short)d;
        }
    }
    return m;
}

AuroraMesh* aurora_mesh_create_cylinder(float radius, float height, int segments) {
    if (segments < 3) segments = 3;
    int verts = (segments + 1) * 2 + 2;
    int idx_count = segments * 12;
    AuroraMesh* m = mesh_alloc(verts, idx_count);
    if (!m) return nullptr;
    float* v = m->vertices;
    unsigned short* idx = m->indices;
    float hh = height * 0.5f;
    /* side vertices */
    int vi = 0;
    for (int side = 0; side < 2; side++) {
        float y = (side == 0) ? -hh : hh;
        for (int i = 0; i <= segments; i++) {
            float a = (float)i * 2.0f * 3.14159265f / (float)segments;
            float c = cosf(a), s = sinf(a);
            v[vi+0] = radius * c; v[vi+1] = y; v[vi+2] = radius * s;
            v[vi+3] = c; v[vi+4] = 0; v[vi+5] = s;
            v[vi+6] = (float)i / (float)segments;
            v[vi+7] = (float)side;
            vi += 8;
        }
    }
    /* center bottom */
    v[vi+0]=0; v[vi+1]=-hh; v[vi+2]=0;  v[vi+3]=0; v[vi+4]=-1; v[vi+5]=0;  v[vi+6]=0.5f; v[vi+7]=0.5f; vi+=8;
    int cb = (segments + 1) * 2;
    /* center top */
    v[vi+0]=0; v[vi+1]=hh; v[vi+2]=0;  v[vi+3]=0; v[vi+4]=1; v[vi+5]=0;  v[vi+6]=0.5f; v[vi+7]=0.5f; vi+=8;
    int ct = cb + 1;

    int ii = 0;
    for (int i = 0; i < segments; i++) {
        int a = i, b = i + 1;
        int c = (segments + 1) + i, d = (segments + 1) + i + 1;
        idx[ii++] = (unsigned short)a;
        idx[ii++] = (unsigned short)c;
        idx[ii++] = (unsigned short)b;
        idx[ii++] = (unsigned short)b;
        idx[ii++] = (unsigned short)c;
        idx[ii++] = (unsigned short)d;
        /* bottom cap */
        idx[ii++] = (unsigned short)(cb);
        idx[ii++] = (unsigned short)(c);
        idx[ii++] = (unsigned short)(a);
        /* top cap */
        idx[ii++] = (unsigned short)(ct);
        idx[ii++] = (unsigned short)(b);
        idx[ii++] = (unsigned short)(d);
    }
    return m;
}

AuroraMesh* aurora_mesh_create_capsule(float radius, float height, int segments) {
    if (segments < 3) segments = 3;
    int half_seg = segments / 2;
    if (half_seg < 2) half_seg = 2;
    int ring_verts = segments + 1;
    int total_verts = ring_verts * (half_seg * 2 + 1);
    int idx_count = (half_seg * 2) * segments * 6;
    AuroraMesh* m = mesh_alloc(total_verts, idx_count);
    if (!m) return nullptr;
    float* v = m->vertices;
    unsigned short* idx = m->indices;
    float hh = height * 0.5f;
    int vi = 0;
    for (int ring = 0; ring <= half_seg * 2; ring++) {
        float t = (float)ring / (float)(half_seg * 2) * 3.14159265f;
        float sin_t = sinf(t), cos_t = cosf(t);
        float y_off = (ring <= half_seg) ? (-hh + (float)ring / (float)half_seg * hh) : (hh - (float)(ring - half_seg) / (float)half_seg * hh);
        float r = radius * sin_t;
        float y_center = (ring <= half_seg) ? -hh : hh;
        float ny = cos_t;
        for (int lon = 0; lon <= segments; lon++) {
            float p = (float)lon * 2.0f * 3.14159265f / (float)segments;
            float cp = cosf(p), sp = sinf(p);
            float nx = sin_t * cp, nz = sin_t * sp;
            v[vi+0] = r * cp;
            v[vi+1] = y_center + radius * cos_t * (ring <= half_seg ? -1.0f : 1.0f);
            v[vi+2] = r * sp;
            v[vi+3] = nx; v[vi+4] = ny; v[vi+5] = nz;
            v[vi+6] = (float)lon / (float)segments;
            v[vi+7] = (float)ring / (float)(half_seg * 2);
            vi += 8;
        }
    }
    int ii = 0;
    for (int ring = 0; ring < half_seg * 2; ring++) {
        for (int lon = 0; lon < segments; lon++) {
            int a = ring * ring_verts + lon;
            int b = a + 1;
            int c = (ring + 1) * ring_verts + lon;
            int d = c + 1;
            idx[ii++] = (unsigned short)a;
            idx[ii++] = (unsigned short)c;
            idx[ii++] = (unsigned short)b;
            idx[ii++] = (unsigned short)b;
            idx[ii++] = (unsigned short)c;
            idx[ii++] = (unsigned short)d;
        }
    }
    return m;
}

int aurora_mesh_get_vertex_count(AuroraMesh* mesh) {
    return mesh ? mesh->vertex_count : 0;
}

const float* aurora_mesh_get_vertex_data(AuroraMesh* mesh) {
    return mesh ? mesh->vertices : nullptr;
}

int aurora_mesh_get_index_count(AuroraMesh* mesh) {
    return mesh ? mesh->index_count : 0;
}

const unsigned short* aurora_mesh_get_index_data(AuroraMesh* mesh) {
    return mesh ? mesh->indices : nullptr;
}

void aurora_mesh_destroy(AuroraMesh* mesh) {
    if (!mesh) return;
    free(mesh->vertices);
    free(mesh->indices);
    free(mesh);
}

} /* extern "C" */
