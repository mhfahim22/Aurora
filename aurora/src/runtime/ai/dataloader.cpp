#include "runtime/dataloader.hpp"
#include "runtime/tensor.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

extern "C" {

/*
 * DataLoader — CSV streaming batched reader
 *
 * Scans the file on create to count rows, then reads batches on demand.
 * Uses a line buffer and strtod for parsing.
 */

/* ── Helper: count lines in a file (first pass) ── */
static int64_t count_lines(const char* path, int64_t* out_cols) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    int64_t lines = 0, cols = 0, max_cols = 0;
    int ch, prev = '\n';
    int in_line = 0, col_count = 0;
    while ((ch = fgetc(f)) != EOF) {
        if (ch == '\n' || ch == '\r') {
            if (in_line) {
                lines++;
                if (col_count > max_cols) max_cols = col_count;
                col_count = 0;
                in_line = 0;
            }
            if (ch == '\r') { ch = fgetc(f); if (ch != EOF && ch != '\n') ungetc(ch, f); }
            prev = ch;
            continue;
        }
        if (!in_line) { in_line = 1; col_count = 1; }
        if (ch == ',') col_count++;
        prev = ch;
    }
    if (in_line) { lines++; if (col_count > max_cols) max_cols = col_count; }
    fclose(f);
    if (out_cols) *out_cols = max_cols;
    return lines;
}

/* ── Helper: parse a single CSV line into doubles ── */
static int parse_csv_line(const char* line, double* out, int64_t max_cols) {
    const char* p = line;
    int col = 0;
    while (*p && col < max_cols) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '"') {
            /* Quoted field */
            p++;
            const char* start = p;
            while (*p && *p != '"') p++;
            if (p > start) {
                char* end = nullptr;
                char* buf = (char*)malloc((size_t)(p - start) + 1);
                memcpy(buf, start, (size_t)(p - start));
                buf[p - start] = '\0';
                out[col++] = strtod(buf, &end);
                free(buf);
            } else {
                out[col++] = 0.0;
            }
            if (*p == '"') p++;
        } else {
            char* end = nullptr;
            out[col++] = strtod(p, &end);
            p = end;
        }
        if (*p == ',') p++;
        else break;
    }
    return col;
}

/* ── Helper: parse a single CSV line into floats ── */
static int parse_csv_line_f32(const char* line, float* out, int64_t max_cols) {
    const char* p = line;
    int col = 0;
    while (*p && col < max_cols) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '"') {
            p++;
            const char* start = p;
            while (*p && *p != '"') p++;
            if (p > start) {
                char* buf = (char*)malloc((size_t)(p - start) + 1);
                memcpy(buf, start, (size_t)(p - start));
                buf[p - start] = '\0';
                out[col++] = (float)atof(buf);
                free(buf);
            } else {
                out[col++] = 0.0f;
            }
            if (*p == '"') p++;
        } else {
            out[col++] = (float)atof(p);
            while (*p && *p != ',' && *p != '\n' && *p != '\r') p++;
        }
        if (*p == ',') p++;
        else break;
    }
    return col;
}

/* ════════════════════════════════════════════════════════════
   Public API
   ════════════════════════════════════════════════════════════ */

DataLoader* dataloader_create(const char* path, int64_t batch_size,
                               int has_target, int shuffle, int dtype) {
    if (!path || batch_size <= 0) return nullptr;

    int64_t cols = 0;
    int64_t total = count_lines(path, &cols);
    if (total <= 0 || cols <= 0) return nullptr;

    /* Skip header row if it looks like text (first char is not digit, -, or +) */
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    char first_line[4096];
    if (!fgets(first_line, sizeof(first_line), f)) { fclose(f); return nullptr; }
    fclose(f);

    int skip_header = 0;
    const char* p = first_line;
    while (*p == ' ' || *p == '\t') p++;
    if (!(*p >= '0' && *p <= '9') && *p != '-' && *p != '+' && *p != '.') {
        skip_header = 1;
    }

    int64_t data_rows = total - (skip_header ? 1 : 0);
    if (data_rows <= 0) return nullptr;

    DataLoader* dl = (DataLoader*)calloc(1, sizeof(DataLoader));
    dl->path = path;
    dl->total_rows = data_rows;
    dl->num_features = has_target ? (cols - 1) : cols;
    dl->batch_size = batch_size;
    dl->current_row = 0;
    dl->has_target = has_target;
    dl->shuffle = shuffle;
    dl->dtype = dtype;
    dl->epoch_done = 0;
    dl->file_handle = nullptr;

    if (shuffle) {
        dl->shuffle_idx = (int64_t*)malloc((size_t)data_rows * sizeof(int64_t));
        for (int64_t i = 0; i < data_rows; i++) dl->shuffle_idx[i] = i;
    }

    return dl;
}

void* dataloader_next_batch(DataLoader* dl) {
    if (!dl || dl->epoch_done) return nullptr;

    int64_t start = dl->current_row;
    int64_t remaining = dl->total_rows - start;
    int64_t bs = (remaining < dl->batch_size) ? remaining : dl->batch_size;
    if (bs <= 0) { dl->epoch_done = 1; return nullptr; }

    int ncols = dl->num_features + (dl->has_target ? 1 : 0);
    int64_t shape[2] = { bs, ncols };

    /* Create tensor */
    AuroraTensor* batch = aurora_tensor_new_with_dtype(2, shape, dl->dtype);

    /* Open file and seek to data start */
    FILE* f = fopen(dl->path, "rb");
    if (!f) { aurora_tensor_free(batch); return nullptr; }

    /* Skip header if needed (read first line but we already know it's there) */
    /* We need to skip to the right line. Re-count from start. */
    int64_t header_lines = dl->total_rows; /* total_rows is data rows, so header was subtracted.
                                              Figure out how many header lines to skip. */
    /* Quick check: read first line to see if it's a header */
    rewind(f);
    char tmp[4096];
    if (fgets(tmp, sizeof(tmp), f)) {
        const char* cp = tmp;
        while (*cp == ' ' || *cp == '\t') cp++;
        if (!(*cp >= '0' && *cp <= '9') && *cp != '-' && *cp != '+' && *cp != '.') {
            /* First line is a header — we need to skip it */
            /* But we already determined total_rows is without header, so just skip first line */
            rewind(f);
            fgets(tmp, sizeof(tmp), f); /* skip header */
        } else {
            rewind(f);
        }
    } else {
        rewind(f);
    }

    /* Seek to start row */
    int64_t row = 0;
    while (row < start) {
        if (!fgets(tmp, sizeof(tmp), f)) break;
        row++;
    }

    /* Read batch rows */
    for (int64_t r = 0; r < bs; r++) {
        if (!fgets(tmp, sizeof(tmp), f)) break;
        if (dl->dtype == TENSOR_F32) {
            float* row_data = batch->data_f32 + r * ncols;
            int parsed = parse_csv_line_f32(tmp, row_data, ncols);
            for (int c = parsed; c < ncols; c++) row_data[c] = 0.0f;
        } else {
            double* row_data = batch->data + r * ncols;
            int parsed = parse_csv_line(tmp, row_data, ncols);
            for (int c = parsed; c < ncols; c++) row_data[c] = 0.0;
        }
    }

    fclose(f);
    dl->current_row = start + bs;
    if (dl->current_row >= dl->total_rows) {
        dl->epoch_done = 1;
        if (dl->shuffle) {
            /* Fisher-Yates shuffle */
            for (int64_t i = dl->total_rows - 1; i > 0; i--) {
                int64_t j = rand() % (i + 1);
                int64_t t = dl->shuffle_idx[i];
                dl->shuffle_idx[i] = dl->shuffle_idx[j];
                dl->shuffle_idx[j] = t;
            }
        }
    }

    return (void*)batch;
}

void dataloader_reset(DataLoader* dl) {
    if (!dl) return;
    dl->current_row = 0;
    dl->epoch_done = 0;
}

void dataloader_free(DataLoader* dl) {
    if (!dl) return;
    free(dl->shuffle_idx);
    free(dl);
}

} /* extern "C" */
