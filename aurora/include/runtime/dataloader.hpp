#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DataLoader — memory-mapped streaming batched data reader.
 * Loads large CSV/text datasets in chunks without loading everything into RAM.
 */

typedef struct DataLoader {
    const char* path;      /* file path */
    int64_t  total_rows;   /* total rows in file */
    int64_t  num_features; /* columns per row (excluding target) */
    int64_t  batch_size;
    int64_t  current_row;  /* cursor position */
    int      has_target;   /* 1 = last column is target */
    int      shuffle;      /* 1 = shuffle at epoch end */
    int64_t* shuffle_idx;  /* permutation array */
    int      epoch_done;   /* 1 when all data consumed */
    int      dtype;        /* TENSOR_F32 or TENSOR_F64 */
    /* internal buffer */
    void*    file_handle;
} DataLoader;

/* Create a DataLoader from a CSV file */
DataLoader* dataloader_create(const char* path, int64_t batch_size,
                              int has_target, int shuffle, int dtype);

/* Get next batch. Returns a 2D tensor [batch_size, num_features(+1 if has_target)].
   The actual batch size may be smaller for the last batch.
   Returns NULL when epoch is complete. */
void* dataloader_next_batch(DataLoader* dl);

/* Rewind to start of data */
void dataloader_reset(DataLoader* dl);

/* Free the DataLoader */
void dataloader_free(DataLoader* dl);

#ifdef __cplusplus
}
#endif
