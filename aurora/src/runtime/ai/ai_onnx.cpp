#include "runtime/ai/ai_common.h"
#include "runtime/memory.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

/* ── Minimal protobuf wire format writer ── */

/* Protobuf wire types */
#define PB_WT_VARINT 0
#define PB_WT_64BIT  1
#define PB_WT_LEN    2
#define PB_WT_32BIT  5

static void pb_write_varint(FILE* f, uint64_t v) {
    do {
        uint8_t byte = (uint8_t)(v & 0x7F);
        v >>= 7;
        if (v) byte |= 0x80;
        fwrite(&byte, 1, 1, f);
    } while (v);
}

static void pb_write_tag(FILE* f, int field, int wire_type) {
    pb_write_varint(f, (uint64_t)((field << 3) | wire_type));
}

static void pb_write_int32(FILE* f, int field, int32_t v) {
    pb_write_tag(f, field, PB_WT_VARINT);
    pb_write_varint(f, (uint64_t)(int64_t)v);
}

static void pb_write_int64(FILE* f, int field, int64_t v) {
    pb_write_tag(f, field, PB_WT_VARINT);
    pb_write_varint(f, (uint64_t)v);
}

static void pb_write_float(FILE* f, int field, float v) {
    pb_write_tag(f, field, PB_WT_32BIT);
    fwrite(&v, sizeof(float), 1, f);
}

static void pb_write_double(FILE* f, int field, double v) {
    pb_write_tag(f, field, PB_WT_64BIT);
    fwrite(&v, sizeof(double), 1, f);
}

static void pb_write_string(FILE* f, int field, const char* s, size_t len) {
    pb_write_tag(f, field, PB_WT_LEN);
    pb_write_varint(f, (uint64_t)len);
    fwrite(s, 1, len, f);
}

static void pb_write_bytes(FILE* f, int field, const void* data, size_t len) {
    pb_write_tag(f, field, PB_WT_LEN);
    pb_write_varint(f, (uint64_t)len);
    fwrite(data, 1, len, f);
}

static void pb_write_submsg(FILE* f, int field, const void* msg_data, size_t msg_len) {
    pb_write_tag(f, field, PB_WT_LEN);
    pb_write_varint(f, (uint64_t)msg_len);
    fwrite(msg_data, 1, msg_len, f);
}

/* Buffer for building sub-messages */
typedef struct {
    uint8_t* data;
    size_t len;
    size_t cap;
} PBBuf;

static void pb_buf_init(PBBuf* b) {
    b->data = (uint8_t*)malloc(4096);
    b->len = 0;
    b->cap = 4096;
}

static void pb_buf_ensure(PBBuf* b, size_t extra) {
    if (b->len + extra > b->cap) {
        b->cap = (b->cap * 2) + extra + 1024;
        b->data = (uint8_t*)aurora_safe_realloc(b->data, b->cap);
    }
}

static void pb_buf_write(PBBuf* b, const void* data, size_t len) {
    pb_buf_ensure(b, len);
    memcpy(b->data + b->len, data, len);
    b->len += len;
}

static void pb_buf_write_varint(PBBuf* b, uint64_t v) {
    uint8_t tmp[10];
    int n = 0;
    do {
        tmp[n] = (uint8_t)(v & 0x7F);
        v >>= 7;
        if (v) tmp[n] |= 0x80;
        n++;
    } while (v);
    pb_buf_write(b, tmp, (size_t)n);
}

static void pb_buf_write_tag(PBBuf* b, int field, int wire_type) {
    pb_buf_write_varint(b, (uint64_t)((field << 3) | wire_type));
}

static void pb_buf_write_int64(PBBuf* b, int field, int64_t v) {
    pb_buf_write_tag(b, field, PB_WT_VARINT);
    pb_buf_write_varint(b, (uint64_t)v);
}

static void pb_buf_write_string(PBBuf* b, int field, const char* s, size_t len) {
    pb_buf_write_tag(b, field, PB_WT_LEN);
    pb_buf_write_varint(b, (uint64_t)len);
    pb_buf_write(b, s, len);
}

static void pb_buf_write_submsg(PBBuf* b, int field, const void* msg, size_t msg_len) {
    pb_buf_write_tag(b, field, PB_WT_LEN);
    pb_buf_write_varint(b, (uint64_t)msg_len);
    pb_buf_write(b, msg, msg_len);
}

static void pb_buf_free(PBBuf* b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

/* ONNX tensor proto builder */
static void build_tensor_proto(PBBuf* buf, const char* name,
                                int64_t* dims, int ndim,
                                int data_type, const void* raw_data, size_t raw_size)
{
    PBBuf msg;
    pb_buf_init(&msg);
    pb_buf_write_string(&msg, 1, name, strlen(name));
    pb_buf_write_int64(&msg, 2, data_type);
    for (int i = 0; i < ndim; i++)
        pb_buf_write_int64(&msg, 3, dims[i]);
    pb_buf_write(&msg, raw_data, raw_size);
    /* raw_data field 5 (bytes) — but we already included it above */
    /* Actually ONNX uses field 5 for raw data. Let's write it separately. */
    /* Re-build properly: dims go in repeated field 3, raw in field 5 */
    /* For simplicity, write raw data as field 5 */
    PBBuf msg2;
    pb_buf_init(&msg2);
    pb_buf_write_string(&msg2, 1, name, strlen(name));
    pb_buf_write_int64(&msg2, 2, data_type);
    for (int i = 0; i < ndim; i++)
        pb_buf_write_int64(&msg2, 3, dims[i]);
    if (raw_data && raw_size > 0) {
        pb_buf_write_tag(&msg2, 5, PB_WT_LEN);
        pb_buf_write_varint(&msg2, (uint64_t)raw_size);
        pb_buf_write(&msg2, raw_data, raw_size);
    }
    pb_buf_free(&msg);
    pb_buf_write_submsg(buf, 0, msg2.data, msg2.len);
    pb_buf_free(&msg2);
}

/* ONNX value info proto builder */
static void build_value_info(PBBuf* buf, const char* name,
                              int64_t* dims, int ndim, int elem_type)
{
    PBBuf msg, type_msg, tensor_type;
    pb_buf_init(&msg);
    pb_buf_init(&type_msg);
    pb_buf_init(&tensor_type);

    /* tensor_type: elem_type (field 1) + shape.dim (field 2 repeated) */
    /* shape = field 2, each dim = field 1 inside shape */
    pb_buf_write_int64(&tensor_type, 1, elem_type);
    for (int i = 0; i < ndim; i++) {
        PBBuf dim_msg;
        pb_buf_init(&dim_msg);
        pb_buf_write_int64(&dim_msg, 1, dims[i]);
        pb_buf_write_submsg(&tensor_type, 2, dim_msg.data, dim_msg.len);
        pb_buf_free(&dim_msg);
    }

    pb_buf_write_submsg(&type_msg, 1, tensor_type.data, tensor_type.len);
    pb_buf_write_string(&msg, 1, name, strlen(name));
    pb_buf_write_submsg(&msg, 4, type_msg.data, type_msg.len);

    pb_buf_free(&tensor_type);
    pb_buf_free(&type_msg);
    pb_buf_write_submsg(buf, 0, msg.data, msg.len);
    pb_buf_free(&msg);
}

/* ── Main ONNX export function ── */
extern "C" {

int model_export_onnx(Model* m, const char* path) {
    if (!m || !path) return 0;

    FILE* f = fopen(path, "wb");
    if (!f) return 0;

    /* Build the ModelProto */
    PBBuf model_buf, graph_buf, nodes_buf, inits_buf, inputs_buf, outputs_buf;
    pb_buf_init(&model_buf);
    pb_buf_init(&graph_buf);
    pb_buf_init(&nodes_buf);
    pb_buf_init(&inits_buf);
    pb_buf_init(&inputs_buf);
    pb_buf_init(&outputs_buf);

    /* ── Build initializers (weights and biases) ── */
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        char w_name[64], b_name[64];

        if (l->w) {
            snprintf(w_name, sizeof(w_name), "layer_%d_weight", i);
            int dt = (l->dtype == TENSOR_F32) ? 1 : 11; /* ONNX: 1=float, 11=double */
            size_t elem_size = (l->dtype == TENSOR_F32) ? sizeof(float) : sizeof(double);
            build_tensor_proto(&inits_buf, w_name, l->w->shape, l->w->ndim,
                               dt, l->w->data_ptr, (size_t)l->w->total_size * elem_size);
        }
        if (l->b) {
            snprintf(b_name, sizeof(b_name), "layer_%d_bias", i);
            int dt = (l->dtype == TENSOR_F32) ? 1 : 11;
            size_t elem_size = (l->dtype == TENSOR_F32) ? sizeof(float) : sizeof(double);
            build_tensor_proto(&inits_buf, b_name, l->b->shape, l->b->ndim,
                               dt, l->b->data_ptr, (size_t)l->b->total_size * elem_size);
        }
    }

    /* ── Build graph inputs/outputs ── */
    int64_t input_shape[2] = { -1, m->layers[0].w ? m->layers[0].w->shape[0] : 1 };
    build_value_info(&inputs_buf, "input", input_shape, 2, 1);

    int64_t output_shape[2] = { -1, 1 };
    for (int i = m->n_layers - 1; i >= 0; i--) {
        if (m->layers[i].type == LAYER_DENSE && m->layers[i].w) {
            output_shape[1] = m->layers[i].w->shape[1];
            break;
        }
    }
    build_value_info(&outputs_buf, "output", output_shape, 2, 1);

    /* ── Build graph nodes ── */
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        PBBuf node_msg;
        pb_buf_init(&node_msg);

        char node_name[64], input_name[64], output_name[64];
        snprintf(node_name, sizeof(node_name), "node_%d", i);
        snprintf(input_name, sizeof(input_name), "node_%d_input", i);
        snprintf(output_name, sizeof(output_name), "node_%d_output", i);

        const char* op_type = "Identity";
        switch (l->type) {
            case 0:  op_type = "Gemm"; break;              /* LAYER_DENSE */
            case 1:  op_type = "Conv"; break;              /* LAYER_CONV */
            case 4:  op_type = "Dropout"; break;            /* LAYER_DROPOUT */
            case 5:  op_type = "BatchNormalization"; break; /* LAYER_BATCHNORM */
            case 6:  op_type = "com.microsoft::Attention"; break; /* LAYER_ATTENTION */
            case 7:  op_type = "Flatten"; break;            /* LAYER_FLATTEN */
            case 8:  op_type = "LayerNormalization"; break; /* LAYER_LAYERNORM */
            case 9:  op_type = "Gather"; break;             /* LAYER_EMBEDDING */
            default: break;
        }

        pb_buf_write_string(&node_msg, 1, op_type, strlen(op_type));

        /* Input names */
        /* field 2: repeated string inputs */
        if (l->type == LAYER_DENSE && l->w) {
            char w_name[64], b_name[64];
            snprintf(w_name, sizeof(w_name), "layer_%d_weight", i);
            snprintf(b_name, sizeof(b_name), "layer_%d_bias", i);
            pb_buf_write_string(&node_msg, 2, (i == 0) ? "input" : input_name, strlen((i == 0) ? "input" : input_name));
            pb_buf_write_string(&node_msg, 2, w_name, strlen(w_name));
            pb_buf_write_string(&node_msg, 2, b_name, strlen(b_name));
        } else {
            pb_buf_write_string(&node_msg, 2, (i == 0) ? "input" : input_name, strlen((i == 0) ? "input" : input_name));
        }

        /* Output names */
        pb_buf_write_string(&node_msg, 3, output_name, strlen(output_name));

        /* Node name */
        pb_buf_write_string(&node_msg, 7, node_name, strlen(node_name));

        pb_buf_write_submsg(&nodes_buf, 0, node_msg.data, node_msg.len);
        pb_buf_free(&node_msg);
    }

    /* ── Build GraphProto ── */
    pb_buf_write_string(&graph_buf, 1, "torch-graph", strlen("torch-graph"));
    /* Inputs (field 4) */
    pb_buf_write_submsg(&graph_buf, 4, inputs_buf.data, inputs_buf.len);
    /* Outputs (field 8) */
    pb_buf_write_submsg(&graph_buf, 8, outputs_buf.data, outputs_buf.len);
    /* Nodes (field 1) */
    pb_buf_write(&graph_buf, nodes_buf.data, nodes_buf.len);
    /* Initializers (field 5) */
    pb_buf_write(&graph_buf, inits_buf.data, inits_buf.len);

    /* ── Build ModelProto ── */
    /* ir_version = 6 (field 1) */
    pb_buf_write_int64(&model_buf, 1, 6);
    /* opset_import (field 8) */
    {
        PBBuf opset_msg;
        pb_buf_init(&opset_msg);
        pb_buf_write_string(&opset_msg, 1, "", 0); /* empty domain */
        pb_buf_write_int64(&opset_msg, 2, 14); /* opset version */
        pb_buf_write_submsg(&model_buf, 8, opset_msg.data, opset_msg.len);
        pb_buf_free(&opset_msg);
    }
    /* producer_name (field 2) */
    {
        const char* pname = "aurora_engine";
        pb_buf_write_string(&model_buf, 2, pname, strlen(pname));
    }
    /* graph (field 7) */
    pb_buf_write_submsg(&model_buf, 7, graph_buf.data, graph_buf.len);

    /* Write the model proto */
    fwrite(model_buf.data, 1, model_buf.len, f);
    fclose(f);

    pb_buf_free(&model_buf);
    pb_buf_free(&graph_buf);
    pb_buf_free(&nodes_buf);
    pb_buf_free(&inits_buf);
    pb_buf_free(&inputs_buf);
    pb_buf_free(&outputs_buf);

    return 1;
}

} /* extern "C" */
