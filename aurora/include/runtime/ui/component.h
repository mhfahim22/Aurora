#ifndef AURORA_COMPONENT_H
#define AURORA_COMPONENT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AuroraComponent {
    char*    name;
    int      x, y, w, h;
    int      visible;
    void*    state;
    void   (*render_fn)(void* state, int x, int y, int w, int h);
    void   (*update_fn)(void* state, double dt);
    struct AuroraComponent* parent;
    struct AuroraComponent** children;
    int      child_count;
    void*    native_handle;
    int      widget_type;
} AuroraComponent;

#ifdef __cplusplus
}
#endif

#endif /* AURORA_COMPONENT_H */
