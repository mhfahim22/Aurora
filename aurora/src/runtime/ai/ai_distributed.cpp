#include "runtime/ai/ai_distributed.h"
#include "runtime/ai/ai_common.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

DistributedComm g_comm = { COMM_BACKEND_NONE, 0, 1, nullptr };

/* ── Compile-time backend selection ── */
#ifdef AURORA_MPI
#include <mpi.h>
#endif
#ifdef AURORA_NCCL
#include <nccl.h>
#include <cuda_runtime.h>
#endif
#ifdef AURORA_RCCL
#include <rccl/rccl.h>
#include <hip/hip_runtime.h>
#endif

int comm_backend_available(int backend) {
    switch (backend) {
        case COMM_BACKEND_NONE: return 1;
        case COMM_BACKEND_MPI:
            #ifdef AURORA_MPI
            return 1;
            #else
            return 0;
            #endif
        case COMM_BACKEND_NCCL:
            #ifdef AURORA_NCCL
            return 1;
            #else
            return 0;
            #endif
        case COMM_BACKEND_RCCL:
            #ifdef AURORA_RCCL
            return 1;
            #else
            return 0;
            #endif
        default: return 0;
    }
}

static int detect_backend(void) {
    #ifdef AURORA_NCCL
    return COMM_BACKEND_NCCL;
    #elif defined(AURORA_RCCL)
    return COMM_BACKEND_RCCL;
    #elif defined(AURORA_MPI)
    return COMM_BACKEND_MPI;
    #else
    return COMM_BACKEND_NONE;
    #endif
}

static void comm_init_none(void) {
    g_comm.backend = COMM_BACKEND_NONE;
    g_comm.rank = 0;
    g_comm.world_size = 1;
}

#ifdef AURORA_MPI
static void comm_init_mpi(int* argc, char*** argv) {
    MPI_Init(argc, argv);
    g_comm.backend = COMM_BACKEND_MPI;
    MPI_Comm_rank(MPI_COMM_WORLD, &g_comm.rank);
    MPI_Comm_size(MPI_COMM_WORLD, &g_comm.world_size);
    g_comm.comm_handle = (void*)&MPI_COMM_WORLD;
}
#endif

#ifdef AURORA_NCCL
static void comm_init_nccl(void) {
    int n_device;
    cudaGetDeviceCount(&n_device);
    int local_rank = 0;
    const char* env = getenv("LOCAL_RANK");
    if (env) local_rank = atoi(env);
    cudaSetDevice(local_rank % n_device);
    ncclUniqueId id;
    if (g_comm.rank == 0) ncclGetUniqueId(&id);
    MPI_Bcast(&id, sizeof(id), MPI_BYTE, 0, MPI_COMM_WORLD);
    ncclCommInitRank(&g_comm.comm_handle, g_comm.world_size, id, g_comm.rank);
    g_comm.backend = COMM_BACKEND_NCCL;
}
#endif

#ifdef AURORA_RCCL
static void comm_init_rccl(void) {
    int n_device;
    hipGetDeviceCount(&n_device);
    int local_rank = 0;
    const char* env = getenv("LOCAL_RANK");
    if (env) local_rank = atoi(env);
    hipSetDevice(local_rank % n_device);
    ncclUniqueId id;
    if (g_comm.rank == 0) ncclGetUniqueId(&id);
    MPI_Bcast(&id, sizeof(id), MPI_BYTE, 0, MPI_COMM_WORLD);
    ncclCommInitRank(&g_comm.comm_handle, g_comm.world_size, id, g_comm.rank);
    g_comm.backend = COMM_BACKEND_RCCL;
}
#endif

int comm_init(int* argc, char*** argv) {
    int backend = detect_backend();
    switch (backend) {
        case COMM_BACKEND_NONE:
            comm_init_none();
            break;
        #ifdef AURORA_MPI
        case COMM_BACKEND_MPI:
            comm_init_mpi(argc, argv);
            break;
        #endif
        #ifdef AURORA_NCCL
        case COMM_BACKEND_NCCL:
            if (!comm_backend_available(COMM_BACKEND_MPI)) {
                fprintf(stderr, "[distributed] NCCL backend requires MPI for rank coordination\n");
                comm_init_none();
                return 0;
            }
            comm_init_mpi(argc, argv);
            comm_init_nccl();
            break;
        #endif
        #ifdef AURORA_RCCL
        case COMM_BACKEND_RCCL:
            if (!comm_backend_available(COMM_BACKEND_MPI)) {
                fprintf(stderr, "[distributed] RCCL backend requires MPI for rank coordination\n");
                comm_init_none();
                return 0;
            }
            comm_init_mpi(argc, argv);
            comm_init_rccl();
            break;
        #endif
        default:
            comm_init_none();
            break;
    }
    if (comm_is_root()) {
        printf("[distributed] Backend: %s | Rank: %d/%d\n",
            g_comm.backend == COMM_BACKEND_NONE ? "none" :
            g_comm.backend == COMM_BACKEND_MPI  ? "MPI" :
            g_comm.backend == COMM_BACKEND_NCCL ? "NCCL" : "RCCL",
            g_comm.rank, g_comm.world_size);
    }
    return 1;
}

void comm_finalize(void) {
    #ifdef AURORA_NCCL
    if (g_comm.backend == COMM_BACKEND_NCCL && g_comm.comm_handle) {
        ncclCommDestroy((ncclComm_t)g_comm.comm_handle);
    }
    #endif
    #ifdef AURORA_RCCL
    if (g_comm.backend == COMM_BACKEND_RCCL && g_comm.comm_handle) {
        ncclCommDestroy((ncclComm_t)g_comm.comm_handle);
    }
    #endif
    #ifdef AURORA_MPI
    if (g_comm.backend == COMM_BACKEND_MPI || g_comm.backend == COMM_BACKEND_NCCL || g_comm.backend == COMM_BACKEND_RCCL) {
        MPI_Finalize();
    }
    #endif
    g_comm.backend = COMM_BACKEND_NONE;
    g_comm.rank = 0;
    g_comm.world_size = 1;
    g_comm.comm_handle = nullptr;
}

/* ── Point-to-point ── */

int comm_send(double* buf, int64_t count, int dest, int tag) {
    if (g_comm.world_size <= 1) return 1;
    #ifdef AURORA_MPI
    if (g_comm.backend == COMM_BACKEND_MPI) {
        MPI_Send(buf, (int)count, MPI_DOUBLE, dest, tag, MPI_COMM_WORLD);
        return 1;
    }
    #endif
    return 0;
}

int comm_recv(double* buf, int64_t count, int src, int tag) {
    if (g_comm.world_size <= 1) return 1;
    #ifdef AURORA_MPI
    if (g_comm.backend == COMM_BACKEND_MPI) {
        MPI_Recv(buf, (int)count, MPI_DOUBLE, src, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        return 1;
    }
    #endif
    return 0;
}

/* ── Collective ── */

int comm_allreduce(double* buf, int64_t count) {
    if (g_comm.world_size <= 1) return 1;
    #ifdef AURORA_NCCL
    if (g_comm.backend == COMM_BACKEND_NCCL) {
        ncclAllReduce(buf, buf, count, ncclDouble, ncclSum, (ncclComm_t)g_comm.comm_handle, nullptr);
        cudaStreamSynchronize(nullptr);
        return 1;
    }
    #endif
    #ifdef AURORA_MPI
    if (g_comm.backend == COMM_BACKEND_MPI) {
        double* tmp = (double*)malloc((size_t)count * sizeof(double));
        if (!tmp) return 0;
        memcpy(tmp, buf, (size_t)count * sizeof(double));
        MPI_Allreduce(tmp, buf, (int)count, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        free(tmp);
        return 1;
    }
    #endif
    return 0;
}

int comm_broadcast(double* buf, int64_t count, int root) {
    if (g_comm.world_size <= 1) return 1;
    #ifdef AURORA_MPI
    if (g_comm.backend == COMM_BACKEND_MPI || g_comm.backend == COMM_BACKEND_NCCL) {
        MPI_Bcast(buf, (int)count, MPI_DOUBLE, root, MPI_COMM_WORLD);
        return 1;
    }
    #endif
    return 0;
}

int comm_allgather(double* send_buf, double* recv_buf, int64_t count) {
    if (g_comm.world_size <= 1) {
        memcpy(recv_buf, send_buf, (size_t)count * sizeof(double));
        return 1;
    }
    #ifdef AURORA_MPI
    if (g_comm.backend == COMM_BACKEND_MPI) {
        MPI_Allgather(send_buf, (int)count, MPI_DOUBLE, recv_buf, (int)count, MPI_DOUBLE, MPI_COMM_WORLD);
        return 1;
    }
    #endif
    return 0;
}

int comm_reduce_scatter(double* send_buf, double* recv_buf, int64_t count) {
    if (g_comm.world_size <= 1) {
        memcpy(recv_buf, send_buf, (size_t)count * sizeof(double));
        return 1;
    }
    #ifdef AURORA_MPI
    if (g_comm.backend == COMM_BACKEND_MPI) {
        int* rcounts = (int*)malloc((size_t)g_comm.world_size * sizeof(int));
        if (!rcounts) return 0;
        for (int i = 0; i < g_comm.world_size; i++) rcounts[i] = (int)count;
        MPI_Reduce_scatter(send_buf, recv_buf, rcounts, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        free(rcounts);
        return 1;
    }
    #endif
    return 0;
}

int comm_barrier(void) {
    if (g_comm.world_size <= 1) return 1;
    #ifdef AURORA_MPI
    if (g_comm.backend == COMM_BACKEND_MPI || g_comm.backend == COMM_BACKEND_NCCL) {
        MPI_Barrier(MPI_COMM_WORLD);
        return 1;
    }
    #endif
    return 0;
}

/* ── Helpers ── */

int comm_rank(void) { return g_comm.rank; }
int comm_size(void) { return g_comm.world_size; }
int comm_is_root(void) { return g_comm.rank == 0; }

/* ── Tensor Parallelism (TP) helpers ── */

int tp_shard_dense_weights(double* w, int64_t in_dim, int64_t out_dim, int rank, int size, double** shard) {
    if (!w || size <= 0 || rank >= size) return 0;
    int64_t local_out = out_dim / size;
    if (out_dim % size != 0) local_out++;
    if (rank == size - 1) local_out = out_dim - (size - 1) * local_out;
    if (local_out <= 0) return 0;
    *shard = (double*)malloc((size_t)in_dim * local_out * sizeof(double));
    if (!*shard) return 0;
    int64_t offset = rank * (out_dim / size);
    for (int64_t i = 0; i < in_dim; i++)
        for (int64_t j = 0; j < local_out; j++)
            (*shard)[i * local_out + j] = w[i * out_dim + offset + j];
    return (int)local_out;
}

int tp_shard_heads(int num_heads, int rank, int size, int* local_heads, int* offset) {
    if (num_heads < size) return 0;
    int base = num_heads / size;
    int rem = num_heads % size;
    *local_heads = base + (rank < rem ? 1 : 0);
    *offset = rank * base + (rank < rem ? rank : rem);
    return 1;
}

void tp_allreduce_output(double* buf, int64_t count) {
    comm_allreduce(buf, count);
}

/* ── Pipeline Parallelism (PP) helpers ── */

void pp_partition_layers(int n_layers, int stages, int* starts, int* ends) {
    if (!starts || !ends || stages <= 0 || n_layers <= 0) return;
    int base = n_layers / stages;
    int rem = n_layers % stages;
    int cur = 0;
    for (int i = 0; i < stages; i++) {
        starts[i] = cur;
        cur += base + (i < rem ? 1 : 0);
        ends[i] = cur;
    }
}

int pp_forward_stage(double* input, double* output, int64_t count, int stage, int n_stages) {
    if (n_stages <= 1) return 1;
    if (stage > 0) {
        comm_recv(input, count, stage - 1, 0);
    }
    if (stage < n_stages - 1) {
        comm_send(output, count, stage + 1, 0);
    }
    return 1;
}

int pp_backward_stage(double* dout, double* din, int64_t count, int stage, int n_stages) {
    if (n_stages <= 1) return 1;
    if (stage < n_stages - 1) {
        comm_recv(dout, count, stage + 1, 1);
    }
    if (stage > 0) {
        comm_send(din, count, stage - 1, 1);
    }
    return 1;
}

/* ── Model-level distributed helpers ── */

void model_shard_for_tp(void* model_ptr, int tp_size) {
    Model* m = (Model*)model_ptr;
    if (!m || tp_size <= 1) return;
    m->parallel_strategy = PARALLEL_TP;
    m->tp_group_rank = comm_rank() % tp_size;
    m->tp_group_size = tp_size;
    /* Shard each layer's weights */
    for (int i = 0; i < m->n_layers; i++) {
        Layer* l = &m->layers[i];
        if (l->type == LAYER_DENSE && l->w && l->w->ndim >= 2) {
            int64_t in_d = l->w->shape[0], out_d = l->w->shape[1];
            double* shard = nullptr;
            int local_out = tp_shard_dense_weights(l->w->data, in_d, out_d, m->tp_group_rank, tp_size, &shard);
            if (local_out > 0) {
                free(l->w->data);
                l->w->data = shard;
                l->w->shape[1] = local_out;
                l->w->total_size = in_d * local_out;
                l->units = local_out;
            }
            if (l->b && l->b->total_size == out_d) {
                int64_t offset = m->tp_group_rank * (out_d / tp_size);
                int64_t lo = (m->tp_group_rank == tp_size - 1) ? (out_d - offset) : (out_d / tp_size);
                double* b_shard = (double*)malloc((size_t)lo * sizeof(double));
                if (b_shard) {
                    memcpy(b_shard, l->b->data + offset, (size_t)lo * sizeof(double));
                    free(l->b->data);
                    l->b->data = b_shard;
                    l->b->shape[0] = lo;
                    l->b->total_size = lo;
                }
            }
        }
        if (l->type == LAYER_ATTENTION) {
            /* Shard attention heads */
            int local_h, offset;
            if (tp_shard_heads((int)l->num_heads, m->tp_group_rank, tp_size, &local_h, &offset)) {
                l->num_heads = local_h;
            }
            if (l->w) {
                int64_t d = l->w->shape[0], td = l->w->shape[1]; /* d -> 3*d */
                int64_t heads_3d = td / 3;
                int64_t heads_per_gpu = (heads_3d + tp_size - 1) / tp_size;
                int64_t local_start = m->tp_group_rank * heads_per_gpu;
                int64_t local_end = (m->tp_group_rank == tp_size - 1) ? heads_3d : (local_start + heads_per_gpu);
                if (local_start < heads_3d) {
                    int64_t local_3d = (local_end - local_start);
                    double* new_w = (double*)malloc((size_t)d * local_3d * sizeof(double));
                    if (new_w) {
                        for (int64_t r = 0; r < d; r++)
                            memcpy(new_w + r * local_3d, l->w->data + r * td + local_start, (size_t)local_3d * sizeof(double));
                        free(l->w->data);
                        l->w->data = new_w;
                        l->w->shape[1] = local_3d;
                        l->w->total_size = d * local_3d;
                    }
                }
            }
            if (l->hc_w) { /* output projection: d -> d */
                int64_t d = l->hc_w->shape[0];
                double* shard = nullptr;
                tp_shard_dense_weights(l->hc_w->data, d, d, m->tp_group_rank, tp_size, &shard);
                if (shard) {
                    free(l->hc_w->data);
                    l->hc_w->data = shard;
                    l->hc_w->shape[1] = d / tp_size;
                    l->hc_w->total_size = d * (d / tp_size);
                }
            }
        }
    }
}

void model_shard_for_pp(void* model_ptr, int pp_size) {
    Model* m = (Model*)model_ptr;
    if (!m || pp_size <= 1) return;
    m->parallel_strategy = PARALLEL_PP;
    m->pp_stage_rank = comm_rank() % pp_size;
    m->pp_group_size = pp_size;
    int* starts = (int*)malloc((size_t)pp_size * sizeof(int));
    int* ends = (int*)malloc((size_t)pp_size * sizeof(int));
    if (!starts || !ends) { free(starts); free(ends); return; }
    pp_partition_layers(m->n_layers, pp_size, starts, ends);
    m->pp_start = starts[m->pp_stage_rank];
    m->pp_end = ends[m->pp_stage_rank];
    /* Free layers not in our stage */
    for (int i = 0; i < m->n_layers; i++) {
        if (i < (int)m->pp_start || i >= (int)m->pp_end) {
            Layer* l = &m->layers[i];
            if (l->w) { aurora_tensor_free(l->w); l->w = nullptr; }
            if (l->b) { aurora_tensor_free(l->b); l->b = nullptr; }
            if (l->dw) { aurora_tensor_free(l->dw); l->dw = nullptr; }
            if (l->db) { aurora_tensor_free(l->db); l->db = nullptr; }
            if (l->hc_w) { aurora_tensor_free(l->hc_w); l->hc_w = nullptr; }
            if (l->hc_b) { aurora_tensor_free(l->hc_b); l->hc_b = nullptr; }
            if (l->hc_c) { aurora_tensor_free(l->hc_c); l->hc_c = nullptr; }
        }
    }
    free(starts); free(ends);
}

void model_allreduce_gradients(void* model_ptr) {
    distributed_sync_gradients((Model*)model_ptr);
}
