#pragma once
#include <cstdint>

/* Communication backends */
#define COMM_BACKEND_NONE  0
#define COMM_BACKEND_MPI   1
#define COMM_BACKEND_NCCL  2
#define COMM_BACKEND_RCCL  3

typedef struct {
    int backend;
    int rank;
    int world_size;
    void* comm_handle;
} DistributedComm;

/* ── Global distributed state ── */
extern DistributedComm g_comm;

/* ── Initialization / Finalization ── */
int  comm_init(int* argc, char*** argv);
void comm_finalize(void);
int  comm_backend_available(int backend);

/* ── Point-to-point ── */
int  comm_send(double* buf, int64_t count, int dest, int tag);
int  comm_recv(double* buf, int64_t count, int src, int tag);

/* ── Collective ── */
int  comm_allreduce(double* buf, int64_t count);
int  comm_broadcast(double* buf, int64_t count, int root);
int  comm_allgather(double* send_buf, double* recv_buf, int64_t count);
int  comm_reduce_scatter(double* send_buf, double* recv_buf, int64_t count);
int  comm_barrier(void);

/* ── Helpers ── */
int  comm_rank(void);
int  comm_size(void);
int  comm_is_root(void);

/* ── Tensor Parallelism (TP) helpers ── */
int  tp_shard_dense_weights(double* w, int64_t in_dim, int64_t out_dim, int rank, int size, double** shard);
int  tp_shard_heads(int num_heads, int rank, int size, int* local_heads, int* offset);
void tp_allreduce_output(double* buf, int64_t count);

/* ── Pipeline Parallelism (PP) helpers ── */
void pp_partition_layers(int n_layers, int stages, int* starts, int* ends);
int  pp_forward_stage(double* input, double* output, int64_t count, int stage, int n_stages);
int  pp_backward_stage(double* dout, double* din, int64_t count, int stage, int n_stages);

/* ── Model-level distributed helpers ── */
void model_shard_for_tp(void* model_ptr, int tp_size);
void model_shard_for_pp(void* model_ptr, int pp_size);
void model_allreduce_gradients(void* model_ptr);
