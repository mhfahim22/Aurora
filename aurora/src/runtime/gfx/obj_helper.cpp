/* ── OBJ Wavefront file loader for Aurora FFI ── */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

struct ObjData {
    std::vector<float> vertices;
};

extern "C" {

void* aurora_obj_load(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;

    std::vector<float> pos, txc, nrm;
    struct Ref { int v, t, n; };
    std::vector<Ref> refs;

    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            buf[--len] = '\0';

        char* s = buf;
        while (*s == ' ' || *s == '\t') s++;

        if (*s == '#' || *s == '\0') continue;

        if (s[0] == 'v' && s[1] == ' ') {
            float x, y, z;
            if (sscanf(s + 2, "%f %f %f", &x, &y, &z) == 3) {
                pos.push_back(x); pos.push_back(y); pos.push_back(z);
            }
        } else if (s[0] == 'v' && s[1] == 't' && s[2] == ' ') {
            float u, v;
            if (sscanf(s + 3, "%f %f", &u, &v) >= 1) {
                txc.push_back(u); txc.push_back(v);
            }
        } else if (s[0] == 'v' && s[1] == 'n' && s[2] == ' ') {
            float x, y, z;
            if (sscanf(s + 3, "%f %f %f", &x, &y, &z) == 3) {
                nrm.push_back(x); nrm.push_back(y); nrm.push_back(z);
            }
        } else if (s[0] == 'f' && s[1] == ' ') {
            s += 2;
            int fv[4], ft[4], fn[4];
            int cnt = 0;

            while (*s && cnt < 4) {
                while (*s == ' ' || *s == '\t') s++;
                if (!*s) break;

                int vi = 0, ti = 0, ni = 0;
                bool has_t = false, has_n = false;
                char* e;

                vi = (int)strtol(s, &e, 10);
                if (e == s) break;
                s = e;

                if (*s == '/') {
                    s++;
                    if (*s == '/') {
                        s++;
                        ni = (int)strtol(s, &e, 10);
                        if (e != s) { has_n = true; s = e; }
                    } else {
                        ti = (int)strtol(s, &e, 10);
                        if (e != s) { has_t = true; s = e; }
                        if (*s == '/') {
                            s++;
                            ni = (int)strtol(s, &e, 10);
                            if (e != s) { has_n = true; s = e; }
                        }
                    }
                }

                fv[cnt] = vi > 0 ? vi - 1 : (int)(pos.size() / 3) + vi;
                ft[cnt] = ti > 0 ? ti - 1 : (has_t ? (int)(txc.size() / 2) + ti : -1);
                fn[cnt] = ni > 0 ? ni - 1 : (has_n ? (int)(nrm.size() / 3) + ni : -1);
                cnt++;
            }

            if (cnt == 3) {
                for (int i = 0; i < 3; i++)
                    refs.push_back({fv[i], ft[i], fn[i]});
            } else if (cnt == 4) {
                static const int t0[] = {0, 1, 2};
                static const int t1[] = {0, 2, 3};
                for (int i = 0; i < 3; i++) refs.push_back({fv[t0[i]], ft[t0[i]], fn[t0[i]]});
                for (int i = 0; i < 3; i++) refs.push_back({fv[t1[i]], ft[t1[i]], fn[t1[i]]});
            }
        }
    }

    fclose(f);

    auto* obj = new ObjData();
    obj->vertices.reserve(refs.size() * 8);

    for (auto& r : refs) {
        if (r.v >= 0 && r.v * 3 + 2 < (int)pos.size()) {
            obj->vertices.push_back(pos[r.v * 3]);
            obj->vertices.push_back(pos[r.v * 3 + 1]);
            obj->vertices.push_back(pos[r.v * 3 + 2]);
        } else {
            obj->vertices.push_back(0); obj->vertices.push_back(0); obj->vertices.push_back(0);
        }
        if (r.n >= 0 && r.n * 3 + 2 < (int)nrm.size()) {
            obj->vertices.push_back(nrm[r.n * 3]);
            obj->vertices.push_back(nrm[r.n * 3 + 1]);
            obj->vertices.push_back(nrm[r.n * 3 + 2]);
        } else {
            obj->vertices.push_back(0); obj->vertices.push_back(0); obj->vertices.push_back(1);
        }
        if (r.t >= 0 && r.t * 2 + 1 < (int)txc.size()) {
            obj->vertices.push_back(txc[r.t * 2]);
            obj->vertices.push_back(txc[r.t * 2 + 1]);
        } else {
            obj->vertices.push_back(0); obj->vertices.push_back(0);
        }
    }

    return obj;
}

int aurora_obj_vertex_count(void* obj) {
    auto* d = static_cast<ObjData*>(obj);
    return (int)(d->vertices.size() / 8);
}

const float* aurora_obj_vertex_data(void* obj) {
    auto* d = static_cast<ObjData*>(obj);
    return d->vertices.data();
}

void aurora_obj_free(void* obj) {
    delete static_cast<ObjData*>(obj);
}

} /* extern "C" */
