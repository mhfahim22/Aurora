#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

int64_t aurora_process_spawn(const char* command);
int     aurora_process_wait(int64_t handle);
int     aurora_process_kill(int64_t handle);
int     aurora_process_pipe_read(int64_t handle, char* buffer, int buffer_size);
int     aurora_process_pipe_write(int64_t handle, const char* data, int len);
void    aurora_process_close(int64_t handle);

#ifdef __cplusplus
}
#endif
