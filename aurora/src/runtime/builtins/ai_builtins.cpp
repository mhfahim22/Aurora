#include "runtime/ai/ai_common.h"
#include "runtime/ai/tokenizer.hpp"
#include "runtime/ai/ai_distributed.h"

extern "C" {
    /* Forward declarations for cross-references within this file */
    char* text(const void* path_a);
    int64_t csv(const void* path_a);

    /* Functions defined in ai_train.cpp */
    double loss_compute(AuroraTensor* pred, AuroraTensor* target, int loss_type, AuroraTensor* dloss);
    double metric_accuracy(AuroraTensor* pred, AuroraTensor* target);
    void clip_gradients(Model* m, double max_norm);
    void augment_data(AuroraTensor* data, int64_t nf, int64_t ns);
    double lr_reduce_on_plateau(double current_lr, double factor, double min_lr);
    double train_batch(Model* m, AuroraTensor* X, AuroraTensor* y, OptimState* opt);
    void distributed_sync_gradients(Model* m);
    void distributed_broadcast_weights(Model* m, int root);

    /* Functions defined in ai_train.cpp */
    int64_t transformer_block(Model* m, int64_t embed_dim, int64_t num_heads, int64_t ff_dim);
    void optim_add_optim(int type, int64_t param_count, double lr);
    int model_auto_setup(Model* m, int64_t data_ptr);
    int activation_from_name(const char* name);
    int loss_from_name(const char* name);
    int optim_from_name(const char* name);
    int optim_save(OptimState* o, const char* path);
    int optim_load(OptimState* o, const char* path);

    /* Functions defined in ai_model.cpp */
    int64_t model_create(const void* type_a);
    int64_t model_save(int64_t model_ptr, const void* path_a);
    int64_t model_load(const void* path_a);
}

/* Globals defined in other .cpp files */
extern OptimState g_optims[8];
extern int g_n_optims;
extern double g_temperature;
extern double g_top_p;
extern int64_t g_max_tokens;
extern int64_t* g_vstore;
extern int64_t g_vcnt;
extern int64_t g_vcap;
extern Model* g_active_model;
extern int64_t g_active_tokenizer;

/* Static helpers used by the functions below */

static int tensor_is_valid_row(AuroraTensor* t, int64_t row) {
    int64_t cols = t->shape[1];
    for (int64_t c = 0; c < cols; c++) {
        int64_t idx[2] = { row, c };
        double v = aurora_tensor_get(t, idx);
        if (std::isnan(v) || std::isinf(v)) return 0;
    }
    return 1;
}

static const char* guess_ext(const char* path) {
    const char* e = strrchr(path, '.'); return e ? e : "unknown";
}

static const char* pos_words[] = {"good","great","excellent","amazing","happy","love","wonderful","fantastic","positive","best","beautiful","nice","perfect","outstanding","superb","brilliant",nullptr};
static const char* neg_words[] = {"bad","terrible","awful","horrible","hate","ugly","worst","negative","poor","sad","angry","evil","disgusting","dreadful","atrocious",nullptr};

static int in_list(const char* w, const char** l) { for (int i = 0; l[i]; i++) if (strcmp(w, l[i]) == 0) return 1; return 0; }
static void to_lower(char* s) { for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = *s - 'A' + 'a'; }

static AuroraTensor* forward_pass(Model* m, AuroraTensor* input) { return model_forward(m, input, 0); }

extern "C" {

/* === Data Loading === */

int64_t csv(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    if (!path || !path[0]) return 0;
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char buf[65536];
    int64_t rows = 0, cols = 0;
    while (fgets(buf, sizeof(buf), f)) {
        if (rows == 0) { int64_t c = 0;
            for (int64_t i = 0; buf[i]; i++) if (buf[i] == ',') c++;
            cols = c + 1; }
        rows++;
    }
    rewind(f);
    if (rows == 0 || cols == 0) { fclose(f); return 0; }
    int64_t header_rows = 0;
    if (fgets(buf, sizeof(buf), f)) {
        int fc = -1;
        for (int64_t i = 0; buf[i]; i++) { if (buf[i] == ',') { fc = (int)i; break; } }
        if (fc > 0) { char* end = nullptr; double t = strtod(buf, &end); (void)t; if (end == buf) header_rows = 1; }
        if (!header_rows) rewind(f);
    }
    int64_t data_rows = rows - header_rows;
    if (data_rows <= 0) { fclose(f); return 0; }
    int64_t shape[2] = { data_rows, cols };
    AuroraTensor* t = aurora_tensor_new(2, shape);
    int64_t r = 0;
    while (r < data_rows && fgets(buf, sizeof(buf), f)) {
        if (buf[0] == '#' || buf[0] == '\n') continue;
        int64_t start = 0, c = 0, blen = (int64_t)strlen(buf);
        for (int64_t i = 0; i <= blen && c < cols; i++) {
            if (buf[i] == ',' || buf[i] == '\n' || buf[i] == '\0') {
                int64_t idx[2] = { r, c };
                aurora_tensor_set(t, idx, parse_double(buf + start, i - start));
                start = i + 1; c++;
            }
        }
        r++;
    }
    fclose(f);
    return (int64_t)t;
}

int64_t data(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    if (!path) return 0;
    const char* ext = strrchr(path, '.');
    if (ext) {
        if (strcmp(ext, ".csv") == 0 || strcmp(ext, ".tsv") == 0) return csv(path_a);
        if (strcmp(ext, ".txt") == 0) return (int64_t)text(path_a);
    }
    return 0;
}

int64_t tensor(int64_t rows, int64_t cols, int64_t data_arr) {
    if (rows <= 0 || cols <= 0 || !data_arr) return 0;
    int64_t shape[2] = { rows, cols };
    AuroraTensor* t = aurora_tensor_new(2, shape);
    if (!t) return 0;
    for (int64_t i = 0; i < rows * cols && i < t->total_size; i++)
        t->data[i] = aurora_array_get_float(data_arr, i);
    return (int64_t)t;
}

char* json_load(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    if (!path) return AURORA_STRDUP("json: no path");
    FILE* f = fopen(path, "r");
    if (!f) return AURORA_STRDUP("json: cannot open");
    fseek(f, 0, SEEK_END); long fsize = ftell(f); fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return AURORA_STRDUP("json: empty"); }
    char* content = (char*)malloc((size_t)fsize + 1);
    if (!content) { fclose(f); return AURORA_STRDUP("json: OOM"); }
    size_t nread = fread(content, 1, (size_t)fsize, f);
    content[nread] = '\0'; fclose(f);
    JsonValue* jv = aurora_json_parse(content); free(content);
    if (!jv) return AURORA_STRDUP("json: parse error");
    char* s = aurora_json_serialize(jv);
    char* r = AURORA_STRDUP(s ? s : "{}"); free(s); aurora_json_free(jv);
    return r;
}

/* p_json alias for backward compatibility */
char* p_json(const void* path_a) { return json_load(path_a); }

/* Forward decl from gfx/image_helper.cpp */
extern "C" void* aurora_image_load(const char* path, int* width, int* height, int* channels);
extern "C" void aurora_image_free(void* data);

char* image(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    if (!path) return AURORA_STRDUP("image: no path");
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = (unsigned char*)aurora_image_load(path, &w, &h, &ch);
    if (!pixels) return AURORA_STRDUP("image: not found");
    /* Compute average brightness and a hash for identity */
    double avg = 0.0; uint64_t hash = 0;
    int64_t n = (int64_t)w * h * ch;
    for (int64_t i = 0; i < n && i < 65536; i++) {
        avg += (double)pixels[i];
        hash = hash * 31 + pixels[i];
    }
    avg = avg / (n < 65536 ? n : 65536);
    aurora_image_free(pixels);
    char buf[512];
    snprintf(buf, sizeof(buf), "image:%d:%d:%d:%.1f:%llu:%s", w, h, ch, avg, (unsigned long long)hash, path);
    return AURORA_STRDUP(buf);
}

char* text(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    if (!path) return AURORA_STRDUP("text: no path");
    FILE* f = fopen(path, "r");
    if (!f) return AURORA_STRDUP("text: cannot open");
    fseek(f, 0, SEEK_END); long fsize = ftell(f); fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return AURORA_STRDUP(""); }
    char* content = (char*)malloc((size_t)fsize + 1);
    if (!content) { fclose(f); return AURORA_STRDUP(""); }
    size_t br = fread(content, 1, (size_t)fsize, f); content[br] = '\0'; fclose(f);
    char* result = AURORA_STRDUP(content); free(content);
    return result;
}

char* audio(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    if (!path) return AURORA_STRDUP("audio: no path");
    FILE* f = fopen(path, "rb"); if (!f) return AURORA_STRDUP("audio: not found");
    unsigned char h[44]; if (fread(h, 1, 44, f) < 44) { fclose(f); return AURORA_STRDUP("audio: invalid"); }
    if (h[0] != 'R' || h[1] != 'I' || h[2] != 'F' || h[3] != 'F') { fclose(f); return AURORA_STRDUP("audio: not WAV"); }
    int ch = *(short*)(h + 22), sr = *(int*)(h + 24), bits = *(short*)(h + 34), ds = *(int*)(h + 40);
    /* Read PCM samples and compute RMS + peak */
    int bps = bits / 8; if (bps < 1) bps = 2;
    int64_t nsamples = ds > 0 ? ds / bps : 0;
    int16_t* buf16 = (int16_t*)malloc(nsamples * bps > 0 ? (size_t)nsamples * bps : 1);
    size_t read = fread(buf16, 1, nsamples * bps, f); fclose(f);
    (void)read;
    double rms = 0.0, peak = 0.0;
    int64_t n = nsamples;
    for (int64_t i = 0; i < n && i < 65536; i++) {
        double s = bps == 1 ? (double)((int16_t)((int8_t*)buf16)[i] * 256) : (double)buf16[i];
        rms += s * s; if (fabs(s) > peak) peak = fabs(s);
    }
    rms = sqrt(rms / (n < 65536 ? n : 65536));
    free(buf16);
    double dur = ds > 0 ? (double)ds / (double)(sr * ch * bps) : 0.0;
    char buf[512];
    snprintf(buf, sizeof(buf), "audio:%d:%d:%d:%.3f:%.1f:%.1f:%s", sr, ch, bits, dur, rms, peak, path);
    return AURORA_STRDUP(buf);
}

char* video(const void* path_a) {
    const char* path = aurora_str_ptr(path_a);
    if (!path) return AURORA_STRDUP("video: no path");
    FILE* f = fopen(path, "rb"); if (!f) return AURORA_STRDUP("video: not found");
    unsigned char h[64]; int rd = (int)fread(h, 1, 64, f); fclose(f);
    if (rd < 12) return AURORA_STRDUP("video: invalid");
    const char* fmt = "unknown"; int64_t w = 0, hh = 0;
    /* MP4: read width/height from tkhd box if present */
    if (h[4] == 0x66 && h[5] == 0x74 && h[6] == 0x79 && h[7] == 0x70) {
        fmt = "MP4";
        for (int i = 0; i < rd - 30; i++)
            if (h[i]==0x74 && h[i+1]==0x6B && h[i+2]==0x68 && h[i+3]==0x64 && h[i+4]==0x01) {
                w  = (int64_t)h[i+24]*16777216 + (int64_t)h[i+25]*65536 + (int64_t)h[i+26]*256 + h[i+27];
                hh = (int64_t)h[i+28]*16777216 + (int64_t)h[i+29]*65536 + (int64_t)h[i+30]*256 + h[i+31];
                break;
            }
    } else if (h[0] == 0x1a && h[1] == 0x45 && h[2] == 0xDF && h[3] == 0xA3) fmt = "WebM/MKV";
    else if (h[0] == 'R' && h[1] == 'I' && h[2] == 'F' && h[3] == 'F') fmt = "AVI";
    int64_t size_mb = 0;
    f = fopen(path, "rb"); if (f) { fseek(f, 0, SEEK_END); size_mb = ftell(f) / (1024*1024); fclose(f); }
    char buf[512];
    snprintf(buf, sizeof(buf), "video:%s:%lld:%lldx%lld:%s", fmt, (long long)size_mb, (long long)w, (long long)hh, path);
    return AURORA_STRDUP(buf);
}

/* === Data Processing === */

int64_t clean(int64_t data_ptr) {
    AuroraTensor* t = (AuroraTensor*)data_ptr;
    if (!t || t->ndim != 2) return (int64_t)t;
    int64_t rows = t->shape[0], cols = t->shape[1], valid = 0;
    for (int64_t r = 0; r < rows; r++) if (tensor_is_valid_row(t, r)) valid++;
    if (valid == rows) return (int64_t)t;
    int64_t shape[2] = { valid, cols };
    AuroraTensor* r = aurora_tensor_new(2, shape);
    int64_t orow = 0;
    for (int64_t i = 0; i < rows; i++) {
        if (tensor_is_valid_row(t, i)) {
            for (int64_t c = 0; c < cols; c++)
                r->data[orow * cols + c] = t->data[i * cols + c];
            orow++;
        }
    }
    return (int64_t)r;
}

int64_t shuffle(int64_t data_ptr) {
    AuroraTensor* t = (AuroraTensor*)data_ptr;
    if (!t || t->ndim != 2) return data_ptr;
    int64_t rows = t->shape[0], cols = t->shape[1];
    AuroraTensor* r = aurora_tensor_new(2, t->shape);
    memcpy(r->data, t->data, (size_t)(rows * cols) * sizeof(double));
    for (int64_t i = rows - 1; i > 0; i--) {
        int64_t j = (int64_t)((double)rand() / RAND_MAX * (i + 1));
        if (j > i) j = i;
        for (int64_t c = 0; c < cols; c++) {
            double tmp = r->data[i * cols + c]; r->data[i * cols + c] = r->data[j * cols + c]; r->data[j * cols + c] = tmp;
        }
    }
    return (int64_t)r;
}

int64_t split_data(int64_t data_ptr, double ratio) {
    AuroraTensor* t = (AuroraTensor*)data_ptr;
    if (!t || t->ndim != 2) return data_ptr;
    if (ratio <= 0.0 || ratio >= 1.0) ratio = 0.8;
    int64_t rows = t->shape[0], cols = t->shape[1], split = (int64_t)((double)rows * ratio);
    if (split < 1) split = 1; if (split >= rows) split = rows - 1;
    int64_t arr = aurora_array_new(2);
    int64_t trs[2] = { split, cols }, tes[2] = { rows - split, cols };
    AuroraTensor* train = aurora_tensor_new(2, trs);
    AuroraTensor* test = aurora_tensor_new(2, tes);
    for (int64_t i = 0; i < split; i++) memcpy(&train->data[i * cols], &t->data[i * cols], (size_t)cols * sizeof(double));
    for (int64_t i = split; i < rows; i++) memcpy(&test->data[(i - split) * cols], &t->data[i * cols], (size_t)cols * sizeof(double));
    aurora_array_push_int(arr, (int64_t)train); aurora_array_push_int(arr, (int64_t)test);
    return arr;
}

int64_t normalize(int64_t data_ptr) {
    AuroraTensor* t = (AuroraTensor*)data_ptr;
    if (!t || t->ndim != 2) return data_ptr;
    int64_t rows = t->shape[0], cols = t->shape[1];
    AuroraTensor* r = aurora_tensor_new(2, t->shape);
    memcpy(r->data, t->data, (size_t)(rows * cols) * sizeof(double));
    for (int64_t c = 0; c < cols; c++) {
        double mn = r->data[c], mx = r->data[c];
        for (int64_t rw = 0; rw < rows; rw++) { double v = r->data[rw * cols + c]; if (v < mn) mn = v; if (v > mx) mx = v; }
        double range = mx - mn; if (range == 0.0) continue;
        for (int64_t rw = 0; rw < rows; rw++) r->data[rw * cols + c] = (r->data[rw * cols + c] - mn) / range;
    }
    return (int64_t)r;
}

int64_t standard(int64_t data_ptr) {
    AuroraTensor* t = (AuroraTensor*)data_ptr;
    if (!t || t->ndim != 2) return data_ptr;
    int64_t rows = t->shape[0], cols = t->shape[1];
    AuroraTensor* r = aurora_tensor_new(2, t->shape);
    memcpy(r->data, t->data, (size_t)(rows * cols) * sizeof(double));
    for (int64_t c = 0; c < cols; c++) {
        double sum = 0.0;
        for (int64_t rw = 0; rw < rows; rw++) sum += r->data[rw * cols + c];
        double mn = sum / (double)rows, var = 0.0;
        for (int64_t rw = 0; rw < rows; rw++) { double d = r->data[rw * cols + c] - mn; var += d * d; }
        double sd = sqrt(var / (double)rows); if (sd == 0.0) continue;
        for (int64_t rw = 0; rw < rows; rw++) r->data[rw * cols + c] = (r->data[rw * cols + c] - mn) / sd;
    }
    return (int64_t)r;
}

int64_t remove_null(int64_t data_ptr) { return clean(data_ptr); }

/* === Layer Functions === */

int64_t dense(int64_t units, const void* activation_name) {
    const char* name = activation_name ? aurora_str_ptr(activation_name) : "sigmoid";
    Layer* l = (Layer*)calloc(1, sizeof(Layer));
    if (!l) return 0;
    l->type = LAYER_DENSE; l->units = units;
    l->activation = activation_from_name(name);
    l->w = nullptr; l->b = nullptr; l->epsilon = 1e-8;
    return (int64_t)l;
}

int64_t add(int64_t model_ptr, int64_t layer_ptr) {
    Model* m = (Model*)model_ptr;
    Layer* src = (Layer*)layer_ptr;
    if (!m || !src || m->n_layers >= MAX_LAYERS) return 0;
    if (src->type == LAYER_TRANSFORMER) {
        free(src);
        int64_t res = transformer_block(m, 0, 0, 0);
        return res >= 0 ? 1 : 0;
    }
    if (src->type == LAYER_UNEMBED && !src->w) {
        for (int i = m->n_layers - 1; i >= 0; i--)
            if (m->layers[i].type == LAYER_EMBEDDING) { src->w = m->layers[i].w; src->tied_w = m->layers[i].w; break; }
    }
    memcpy(&m->layers[m->n_layers++], src, sizeof(Layer));
    free(src);
    return 1;
}

int64_t conv(int64_t filters, int64_t kernel_size, int64_t stride) {
    Layer* l = (Layer*)calloc(1, sizeof(Layer));
    if (!l) return 0;
    l->type = LAYER_CONV; l->units = filters;
    l->kernel_size = kernel_size > 0 ? kernel_size : 3;
    l->stride = stride > 0 ? stride : 1;
    l->activation = ACT_LINEAR; l->w = nullptr; l->b = nullptr;
    return (int64_t)l;
}

int64_t lstm(int64_t units) {
    Layer* l = (Layer*)calloc(1, sizeof(Layer));
    if (!l) return 0;
    l->type = LAYER_LSTM; l->units = units;
    l->activation = ACT_TANH; l->w = nullptr; l->b = nullptr;
    l->hc_w = nullptr; l->hc_b = nullptr;
    return (int64_t)l;
}

int64_t gru(int64_t units) {
    Layer* l = (Layer*)calloc(1, sizeof(Layer));
    if (!l) return 0;
    l->type = LAYER_GRU; l->units = units;
    l->activation = ACT_TANH; l->w = nullptr; l->b = nullptr;
    l->hc_w = nullptr; l->hc_b = nullptr;
    return (int64_t)l;
}

int64_t dropout(double rate) {
    Layer* l = (Layer*)calloc(1, sizeof(Layer));
    if (!l) return 0;
    l->type = LAYER_DROPOUT;
    l->dropout_rate = (rate < 0.0 ? 0.0 : (rate >= 1.0 ? 0.5 : rate));
    l->activation = ACT_LINEAR;
    return (int64_t)l;
}
int64_t batchnorm() {
    Layer* l = (Layer*)calloc(1, sizeof(Layer));
    if (!l) return 0;
    l->type = LAYER_BATCHNORM; l->activation = ACT_LINEAR;
    return (int64_t)l;
}
int64_t attention() {
    Layer* l = (Layer*)calloc(1, sizeof(Layer));
    if (!l) return 0;
    l->type = LAYER_ATTENTION; l->activation = ACT_LINEAR;
    return (int64_t)l;
}
int64_t transformer() {
    Layer* l = (Layer*)calloc(1, sizeof(Layer));
    if (!l) return 0;
    l->type = LAYER_TRANSFORMER;
    return (int64_t)l;
}

int64_t swiglu_ffn(int64_t hidden, int64_t out_units) {
    Layer* l = (Layer*)calloc(1, sizeof(Layer));
    if (!l) return 0;
    l->type = LAYER_SWIGLU;
    l->units = hidden;
    l->embed_dim = out_units;
    return (int64_t)l;
}

/* === Training API === */

int64_t train(int64_t model_ptr, int64_t data_ptr) {
    Model* m = (Model*)model_ptr;
    AuroraTensor* data = (AuroraTensor*)data_ptr;
    if (!m || !data || data->ndim < 2 || data->ndim > 10) return 0;
    if (!model_auto_setup(m, data_ptr)) return 0;
    int64_t nf = data->shape[1] - 1, ns = data->shape[0];
    int64_t bs = m->batch_size > 0 ? m->batch_size : 32;
    int64_t ep = m->epochs > 0 ? m->epochs : 10;
    int64_t vc = (int64_t)((double)ns * m->validation_split);
    if (vc < 1) vc = 0;
    int64_t tc = ns - vc; if (tc < 1) tc = ns;


    if (m->n_layers > 0 && m->layers[0].w == nullptr) {
        int64_t input_size = nf;
        for (int i = 0; i < m->n_layers; i++) {
            Layer* l = &m->layers[i];
            if (l->w != nullptr) { input_size = l->units; continue; }
            if (l->type == LAYER_DENSE || l->type == LAYER_LSTM || l->type == LAYER_GRU ||
                l->type == LAYER_ATTENTION || l->type == LAYER_MOE || l->type == LAYER_SWIGLU) {
                model_init_layer_weights(m, i, input_size);
                input_size = l->units;
            }
        }
    }

    g_n_optims = 0;
    int64_t total_p = 0;
    for (int i = 0; i < m->n_layers; i++) {
        if (m->layers[i].w) total_p += m->layers[i].w->total_size;
        if (m->layers[i].b) total_p += m->layers[i].b->total_size;
        if (m->layers[i].hc_w) total_p += m->layers[i].hc_w->total_size;
        if (m->layers[i].hc_b) total_p += m->layers[i].hc_b->total_size;
    }
    if (m->learning_rate <= 0.0) m->learning_rate = 0.001;

    optim_add_optim(m->optimizer_type, total_p, m->learning_rate);
    OptimState* opt = &g_optims[0];

    int64_t n_batches = (tc + bs - 1) / bs;

    for (int64_t e = 0; e < ep; e++) {
        m->current_epoch = e + 1;
        double epoch_loss = 0.0;
        int64_t correct = 0, total = 0;

        if (m->augment) augment_data(data, nf, ns);

        for (int64_t i = tc - 1; i > 0; i--) {
            int64_t j = (int64_t)((double)rand() / RAND_MAX * (i + 1));
            if (j > i) j = i;
            for (int64_t cc = 0; cc <= nf; cc++) {
                double tmp = data->data[i * (nf + 1) + cc];
                data->data[i * (nf + 1) + cc] = data->data[j * (nf + 1) + cc];
                data->data[j * (nf + 1) + cc] = tmp;
            }
        }


        for (int64_t b = 0; b < n_batches; b++) {
            int64_t start = b * bs, end = start + bs;
            if (end > tc) end = tc;
            int64_t cb = end - start;
            double* xd = (double*)malloc((size_t)(cb * nf) * sizeof(double));
            double* yd = (double*)malloc((size_t)(cb * 1) * sizeof(double));
            for (int64_t i = start; i < end; i++) {
                for (int64_t f = 0; f < nf; f++) xd[(i - start) * nf + f] = data->data[i * (nf + 1) + f];
                yd[i - start] = data->data[i * (nf + 1) + nf];
            }
            int64_t xs[2] = { cb, nf }, ys2[2] = { cb, 1 };
            AuroraTensor* Xb = aurora_tensor_new(2, xs);
            AuroraTensor* Yb = aurora_tensor_new(2, ys2);
            memcpy(Xb->data, xd, (size_t)(cb * nf) * sizeof(double));
            memcpy(Yb->data, yd, (size_t)(cb * 1) * sizeof(double));
            free(xd); free(yd);

            double bl = train_batch(m, Xb, Yb, opt);
            epoch_loss += bl * (double)cb;

            AuroraTensor* pred_b = model_forward(m, Xb, 0);
            if (pred_b) {
                for (int64_t p = 0; p < cb; p++) {
                    double pp = pred_b->data[p] > 0.5 ? 1.0 : 0.0;
                    if (fabs(pp - Yb->data[p]) < 0.5) correct++;
                }
                total += cb;
                aurora_tensor_free(pred_b);
            }

            aurora_tensor_free(Xb); aurora_tensor_free(Yb);
        }
        epoch_loss /= (double)tc;

        double vl = 0.0, va = 0.0;
        if (vc > 0) {
            int64_t n_vb = (vc + bs - 1) / bs, vcorrect = 0;
            for (int64_t b = 0; b < n_vb; b++) {
                int64_t start = tc + b * bs, end = start + bs;
                if (end > ns) end = ns;
                int64_t cb = end - start;
                double* xd = (double*)malloc((size_t)(cb * nf) * sizeof(double));
                double* yd = (double*)malloc((size_t)(cb * 1) * sizeof(double));
                for (int64_t i = start; i < end; i++) {
                    for (int64_t f = 0; f < nf; f++) xd[(i - start) * nf + f] = data->data[i * (nf + 1) + f];
                    yd[i - start] = data->data[i * (nf + 1) + nf];
                }
                int64_t xs[2] = { cb, nf }, ys2[2] = { cb, 1 };
                AuroraTensor* Xv = aurora_tensor_new(2, xs);
                AuroraTensor* Yv = aurora_tensor_new(2, ys2);
                memcpy(Xv->data, xd, (size_t)(cb * nf) * sizeof(double));
                memcpy(Yv->data, yd, (size_t)(cb * 1) * sizeof(double));
                free(xd); free(yd);

                AuroraTensor* pred = model_forward(m, Xv, 0);
                if (pred) {
                    vl += loss_compute(pred, Yv, m->loss_type, nullptr) * (double)cb;
                    vcorrect += (int64_t)(metric_accuracy(pred, Yv) * (double)cb);
                    aurora_tensor_free(pred);
                }
                aurora_tensor_free(Xv); aurora_tensor_free(Yv);
            }
            vl /= (double)vc; va = (double)vcorrect / (double)vc;
        }

        m->last_loss = epoch_loss;
        m->last_accuracy = vc > 0 ? va : (total > 0 ? (double)correct / (double)total : 0.0);

        if (m->verbose && (e < 5 || e % 10 == 0 || e == ep - 1)) {
            char buf[256];
            if (vc > 0)
                snprintf(buf, sizeof(buf), "[MrCode] Epoch %lld/%lld | loss: %.6f | val_loss: %.6f | val_acc: %.4f",
                    (long long)(e + 1), (long long)ep, epoch_loss, vl, va);
            else
                snprintf(buf, sizeof(buf), "[MrCode] Epoch %lld/%lld | loss: %.6f",
                    (long long)(e + 1), (long long)ep, epoch_loss);
            fprintf(stdout, "%s\n", buf);
        }

        if (vc > 0 && vl < m->best_val_loss) { m->best_val_loss = vl; m->best_epoch = e + 1; m->no_improve_count = 0; }
        else if (vc > 0) { m->no_improve_count++; }

        if (m->early_stop_patience > 0 && m->no_improve_count >= m->early_stop_patience) {
            if (m->verbose) fprintf(stdout, "[MrCode] Early stop at epoch %lld (best: epoch %lld, val_loss=%.6f)\n",
                (long long)(e + 1), (long long)m->best_epoch, m->best_val_loss);
            break;
        }

        if (vc > 0 && m->lr_factor > 0.0 && m->no_improve_count >= m->early_stop_patience / 2 && m->no_improve_count > 0) {
            double old_lr = m->learning_rate;
            m->learning_rate = lr_reduce_on_plateau(m->learning_rate, m->lr_factor, m->min_lr);
            if (m->learning_rate < old_lr && m->verbose)
                fprintf(stdout, "[MrCode] LR reduced: %.10f -> %.10f\n", old_lr, m->learning_rate);
            m->no_improve_count = 0;
            if (opt) opt->lr = m->learning_rate;
        }

        if (m->checkpoint_interval > 0 && (e + 1) % m->checkpoint_interval == 0) {
            char cp[128]; snprintf(cp, sizeof(cp), "model_epoch_%lld.aurora", (long long)(e + 1));
            model_save(model_ptr, cp);
            if (opt) {
                char op[128]; snprintf(op, sizeof(op), "model_epoch_%lld.optim", (long long)(e + 1));
                optim_save(opt, op);
            }
            if (m->verbose) fprintf(stdout, "[MrCode] Checkpoint: %s\n", cp);
        }
    }
    g_active_model = m;
    return 1;
}

int64_t fit(int64_t model_ptr, int64_t x_ptr, int64_t y_ptr) {
    Model* m = (Model*)model_ptr;
    AuroraTensor* X = (AuroraTensor*)x_ptr; AuroraTensor* Y = (AuroraTensor*)y_ptr;
    if (!m || !X || !Y || X->ndim < 2 || Y->ndim < 1) return 0;
    int64_t n = X->shape[0], d = X->shape[1];
    int64_t yc = Y->ndim >= 2 ? Y->shape[1] : 1;
    int64_t sh[2] = { n, d + yc };
    AuroraTensor* combined = aurora_tensor_new(2, sh);
    for (int64_t r = 0; r < n; r++) {
        memcpy(&combined->data[r * (d + yc)], &X->data[r * d], (size_t)d * sizeof(double));
        combined->data[r * (d + yc) + d] = Y->data[r * yc];
    }
    int64_t result = train(model_ptr, (int64_t)combined);
    aurora_tensor_free(combined);
    return result;
}

int64_t test(int64_t model_ptr, int64_t data_ptr) {
    Model* m = (Model*)model_ptr;
    AuroraTensor* data = (AuroraTensor*)data_ptr;
    if (!m || !data || data->ndim < 2 || data->ndim > 10) return 0;
    int64_t nf = data->shape[1] - 1, nr = data->shape[0], correct = 0;
    for (int64_t r = 0; r < nr; r++) {
        int64_t is[2] = { 1, nf };
        AuroraTensor* input = aurora_tensor_new(2, is);
        memcpy(input->data, &data->data[r * data->shape[1]], (size_t)nf * sizeof(double));
        double target = data->data[r * data->shape[1] + nf];
        AuroraTensor* out = model_forward(m, input, 0);
        double pred = out ? out->data[0] : 0.0;
        if (m->loss_type == LOSS_BINARY_CROSS_ENTROPY) pred = pred > 0.5 ? 1.0 : 0.0;
        if (fabs(pred - target) < 0.5) correct++;
        aurora_tensor_free(input);
        if (out) aurora_tensor_free(out);
    }
    m->last_accuracy = (double)correct / (double)nr;
    return 1;
}

int64_t predict(int64_t model_ptr, int64_t input_ptr) {
    Model* m = (Model*)model_ptr;
    AuroraTensor* t = (AuroraTensor*)input_ptr;
    if (!m || !t || t->ndim < 1) return 0;
    int64_t nf = m->n_layers > 0 && m->layers[0].w ? m->layers[0].w->shape[0] : (t->ndim >= 2 ? t->shape[1] : t->total_size);
    if (t->ndim >= 2 && t->shape[1] > nf) {
        int64_t nr = t->shape[0];
        int64_t xs[2] = { nr, nf };
        AuroraTensor* in = aurora_tensor_new(2, xs);
        if (!in) return 0;
        for (int64_t r = 0; r < nr; r++)
            for (int64_t c = 0; c < nf; c++)
                in->data[r * nf + c] = t->data[r * t->shape[1] + c];
        AuroraTensor* out = forward_pass(m, in);
        aurora_tensor_free(in);
        return (int64_t)out;
    }
    return (int64_t)forward_pass(m, t);
}

int64_t retrain(int64_t model_ptr) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    for (int i = 0; i < m->n_layers; i++) {
        if (m->layers[i].w) {
            double sc = sqrt(2.0 / (double)m->layers[i].w->total_size);
            for (int64_t j = 0; j < m->layers[i].w->total_size; j++)
                m->layers[i].w->data[j] = rand_uniform() * sc;
        }
        if (m->layers[i].b) memset(m->layers[i].b->data, 0, (size_t)m->layers[i].b->total_size * sizeof(double));
    }
    m->last_loss = 0.0; m->last_accuracy = 0.0; return 1;
}

/* === Configuration === */

int64_t set_loss(int64_t model_ptr, const void* name) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    m->loss_type = loss_from_name(aurora_str_ptr(name)); return 1;
}
int64_t set_optimizer(int64_t model_ptr, const void* name) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    m->optimizer_type = optim_from_name(aurora_str_ptr(name)); return 1;
}
int64_t set_lr(int64_t model_ptr, double lr) {
    Model* m = (Model*)model_ptr; if (!m || lr <= 0.0) return 0;
    m->learning_rate = lr; return 1;
}
int64_t set_batch_size(int64_t model_ptr, int64_t bs) {
    Model* m = (Model*)model_ptr; if (!m || bs <= 0) return 0;
    m->batch_size = bs; return 1;
}
int64_t set_epochs(int64_t model_ptr, int64_t ep) {
    Model* m = (Model*)model_ptr; if (!m || ep <= 0) return 0;
    m->epochs = ep; return 1;
}
int64_t set_validation_split(int64_t model_ptr, double split) {
    Model* m = (Model*)model_ptr; if (!m || split < 0.0 || split >= 1.0) return 0;
    m->validation_split = split; return 1;
}
int64_t set_early_stop(int64_t model_ptr, int64_t p) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    m->early_stop_patience = p; return 1;
}
int64_t set_checkpoint_interval(int64_t model_ptr, int64_t iv) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    m->checkpoint_interval = iv; return 1;
}
int64_t set_verbose(int64_t model_ptr, int64_t v) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    m->verbose = v; return 1;
}
int64_t set_augment(int64_t model_ptr, int64_t val) {
    Model* m = (Model*)model_ptr; if (!m) return 0;
    m->augment = val; return 1;
}
int64_t set_gradient_clip(int64_t model_ptr, double val) {
    Model* m = (Model*)model_ptr; if (!m || val < 0.0) return 0;
    m->gradient_clip = val; return 1;
}
int64_t set_lr_factor(int64_t model_ptr, double val) {
    Model* m = (Model*)model_ptr; if (!m || val <= 0.0 || val >= 1.0) return 0;
    m->lr_factor = val; return 1;
}
int64_t set_min_lr(int64_t model_ptr, double val) {
    Model* m = (Model*)model_ptr; if (!m || val <= 0.0) return 0;
    m->min_lr = val; return 1;
}

/* === Computer Vision === */

char* detect(const void* img_a) {
    const char* img = aurora_str_ptr(img_a);
    if (!img) return AURORA_STRDUP("[]");
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = (unsigned char*)aurora_image_load(img, &w, &h, &ch);
    if (!pixels) { char buf[256]; snprintf(buf, sizeof(buf), "[{\"object\":\"%s\",\"confidence\":0.0,\"bbox\":[0,0,%d,%d]}]", guess_ext(img), w, h); return AURORA_STRDUP(buf); }
    /* Edge detection via simple horizontal gradient */
    int n_boxes = 0; int boxes[16][4];
    for (int y = 0; y < h - 1 && n_boxes < 8; y++) {
        for (int x = 0; x < w - 1 && n_boxes < 8; x++) {
            int idx = (y * w + x) * ch;
            int gx = abs((int)pixels[idx] - (int)pixels[idx + ch]);
            if (gx > 60) { /* edge detected */
                boxes[n_boxes][0] = x; boxes[n_boxes][1] = y;
                boxes[n_boxes][2] = x + 10 > w ? w - x : 10;
                boxes[n_boxes][3] = 10;
                n_boxes++; x += 12;
            }
        }
    }
    aurora_image_free(pixels);
    char buf[1024]; int pos = 0; pos += snprintf(buf + pos, sizeof(buf) - pos, "[");
    for (int i = 0; i < n_boxes; i++) {
        if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"object\":\"edge_%d\",\"confidence\":0.6,\"bbox\":[%d,%d,%d,%d]}", i, boxes[i][0], boxes[i][1], boxes[i][2], boxes[i][3]);
    }
    if (n_boxes == 0) pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"object\":\"%s\",\"confidence\":0.5,\"bbox\":[0,0,%d,%d]}", guess_ext(img), w, h);
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]");
    return AURORA_STRDUP(buf);
}
char* classify_image(const void* img_a) {
    const char* img = aurora_str_ptr(img_a);
    if (!img) return AURORA_STRDUP("unknown");
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = (unsigned char*)aurora_image_load(img, &w, &h, &ch);
    if (!pixels) { char buf[128]; snprintf(buf, sizeof(buf), "%s (unreadable)", guess_ext(img)); return AURORA_STRDUP(buf); }
    /* Color histogram-based classification */
    int64_t n = (int64_t)w * h;
    double hist[3][8] = {{0}}; /* 8-bin per channel (RGB) */
    for (int64_t i = 0; i < n; i++) {
        int idx = (int)(i * ch);
        for (int c = 0; c < 3 && c < ch; c++) {
            int bin = (int)(pixels[idx + c] / 32); if (bin > 7) bin = 7;
            hist[c][bin]++;
        }
    }
    for (int c = 0; c < 3; c++)
        for (int b = 0; b < 8; b++)
            hist[c][b] = hist[c][b] / n;
    /* Dominant color class */
    int dc = 0; double dm = 0;
    for (int c = 0; c < 3; c++) {
        double m = 0;
        for (int b = 0; b < 8; b++) if (hist[c][b] > m) m = hist[c][b];
        if (m > dm) { dm = m; dc = c; }
    }
    const char* classes[] = {"red_dominant", "green_dominant", "blue_dominant"};
    double brightness = (hist[0][7] + hist[1][7] + hist[2][7]) / 3.0;
    aurora_image_free(pixels);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"class\":\"%s\",\"dominant_ch\":%d,\"brightness\":%.3f,\"w\":%d,\"h\":%d,\"ch\":%d,\"fmt\":\"%s\"}",
             classes[dc], dc, brightness, w, h, ch, guess_ext(img));
    return AURORA_STRDUP(buf);
}
char* segment(const void* img_a) {
    const char* img = aurora_str_ptr(img_a);
    if (!img) return AURORA_STRDUP("[]");
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = (unsigned char*)aurora_image_load(img, &w, &h, &ch);
    if (!pixels) { char buf[256]; snprintf(buf, sizeof(buf), "[{\"class\":\"%s\",\"mask\":\"rle\",\"score\":0.0}]", guess_ext(img)); return AURORA_STRDUP(buf); }
    /* Simple color-based segmentation: cluster pixels into 4 regions by RGB */
    double centroids[4][3];
    for (int k = 0; k < 4; k++)
        for (int c = 0; c < 3; c++)
            centroids[k][c] = (double)rand() / RAND_MAX * 255;
    int64_t n = (int64_t)w * h;
    for (int iter = 0; iter < 5; iter++) {
        int counts[4] = {0}; double sums[4][3] = {{0}};
        for (int64_t i = 0; i < n; i++) {
            int idx = (int)(i * ch); double best = 1e18; int bestk = 0;
            for (int k = 0; k < 4; k++) {
                double d = 0;
                for (int c = 0; c < 3 && c < ch; c++) d += (pixels[idx + c] - centroids[k][c]) * (pixels[idx + c] - centroids[k][c]);
                if (d < best) { best = d; bestk = k; }
            }
            counts[bestk]++;
            for (int c = 0; c < 3 && c < ch; c++) sums[bestk][c] += pixels[idx + c];
        }
        for (int k = 0; k < 4; k++)
            if (counts[k] > 0)
                for (int c = 0; c < 3; c++)
                    centroids[k][c] = sums[k][c] / counts[k];
    }
    aurora_image_free(pixels);
    char buf[1024]; int pos = 0; pos += snprintf(buf + pos, sizeof(buf) - pos, "[");
    const char* cnames[] = {"bg", "fg1", "fg2", "fg3"};
    for (int k = 0; k < 4; k++) {
        if (k > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"class\":\"%s\",\"color\":[%.0f,%.0f,%.0f],\"score\":%.2f}",
                        cnames[k], centroids[k][0], centroids[k][1], centroids[k][2], 0.25);
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]");
    return AURORA_STRDUP(buf);
}
char* face(const void* img_a) {
    const char* img = aurora_str_ptr(img_a);
    if (!img) return AURORA_STRDUP("[]");
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = (unsigned char*)aurora_image_load(img, &w, &h, &ch);
    if (!pixels) return AURORA_STRDUP("[]");
    /* Skin-color detection: find largest skin-colored region */
    int cx = 0, cy = 0, cnt = 0, minx = w, maxx = 0, miny = h, maxy = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * ch;
            int r = pixels[idx], g = pixels[idx + 1], b = pixels[idx + 2];
            if (r > 95 && g > 40 && b > 20 && r > g && r > b && abs(r - g) > 15) {
                cx += x; cy += y; cnt++;
                if (x < minx) minx = x; if (x > maxx) maxx = x;
                if (y < miny) miny = y; if (y > maxy) maxy = y;
            }
        }
    }
    aurora_image_free(pixels);
    char buf[512];
    if (cnt > 50) {
        double conf = cnt > 500 ? 0.92 : 0.5 + (double)cnt / 500.0 * 0.42;
        snprintf(buf, sizeof(buf), "[{\"bbox\":[%d,%d,%d,%d],\"confidence\":%.2f}]", minx, miny, maxx - minx, maxy - miny, conf);
    } else {
        snprintf(buf, sizeof(buf), "[{\"bbox\":[%d,%d,%d,%d],\"confidence\":0.0}]", w/4, h/4, w/2, h/2);
    }
    return AURORA_STRDUP(buf);
}
char* ocr(const void* img_a) {
    const char* img = aurora_str_ptr(img_a);
    if (!img) return AURORA_STRDUP("");
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = (unsigned char*)aurora_image_load(img, &w, &h, &ch);
    if (!pixels) return AURORA_STRDUP("");
    /* Simple connected-component analysis for text regions */
    int text_regions = 0; double total_contrast = 0.0;
    int64_t n = (int64_t)w * h;
    for (int64_t i = ch; i < n * ch; i++) {
        int diff = abs((int)pixels[i] - (int)pixels[i - ch]);
        total_contrast += diff;
        if (diff > 40) text_regions++;
    }
    total_contrast /= n;
    aurora_image_free(pixels);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"text\":\"[OCR] %dx%d img, %.0f%% contrast, %d edges\",\"confidence\":%.2f}",
             w, h, total_contrast, text_regions, text_regions > 100 ? 0.7 : 0.3);
    return AURORA_STRDUP(buf);
}

} /* extern "C" */
