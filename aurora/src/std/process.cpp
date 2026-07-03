#include "std/process.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#ifdef _WIN32

struct ProcessHandle {
    HANDLE hProcess;
    HANDLE hStdinWrite;
    HANDLE hStdoutRead;
    DWORD  pid;
};

int64_t aurora_process_spawn(const char* command) {
    HANDLE hStdinRead, hStdinWrite;
    HANDLE hStdoutRead, hStdoutWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) return -1;
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        CloseHandle(hStdinRead); CloseHandle(hStdinWrite);
        return -1;
    }

    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi = {0};
    STARTUPINFOA si = {0};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    char* cmd_copy = _strdup(command);
    BOOL ok = CreateProcessA(NULL, cmd_copy, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmd_copy);

    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);

    if (!ok) {
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        return -1;
    }

    CloseHandle(pi.hThread);

    ProcessHandle* ph = (ProcessHandle*)malloc(sizeof(ProcessHandle));
    ph->hProcess = pi.hProcess;
    ph->hStdinWrite = hStdinWrite;
    ph->hStdoutRead = hStdoutRead;
    ph->pid = pi.dwProcessId;
    return (int64_t)(intptr_t)ph;
}

int aurora_process_wait(int64_t handle) {
    ProcessHandle* ph = (ProcessHandle*)(intptr_t)handle;
    if (!ph || !ph->hProcess) return -1;
    WaitForSingleObject(ph->hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(ph->hProcess, &exitCode);
    return (int)exitCode;
}

int aurora_process_kill(int64_t handle) {
    ProcessHandle* ph = (ProcessHandle*)(intptr_t)handle;
    if (!ph || !ph->hProcess) return 0;
    return TerminateProcess(ph->hProcess, 1) ? 1 : 0;
}

int aurora_process_pipe_read(int64_t handle, char* buffer, int buffer_size) {
    ProcessHandle* ph = (ProcessHandle*)(intptr_t)handle;
    if (!ph || !ph->hStdoutRead || !buffer || buffer_size <= 0) return -1;
    DWORD read = 0;
    if (!ReadFile(ph->hStdoutRead, buffer, (DWORD)buffer_size, &read, NULL))
        return -1;
    return (int)read;
}

int aurora_process_pipe_write(int64_t handle, const char* data, int len) {
    ProcessHandle* ph = (ProcessHandle*)(intptr_t)handle;
    if (!ph || !ph->hStdinWrite || !data || len <= 0) return -1;
    DWORD written = 0;
    if (!WriteFile(ph->hStdinWrite, data, (DWORD)len, &written, NULL))
        return -1;
    return (int)written;
}

void aurora_process_close(int64_t handle) {
    ProcessHandle* ph = (ProcessHandle*)(intptr_t)handle;
    if (!ph) return;
    if (ph->hProcess) CloseHandle(ph->hProcess);
    if (ph->hStdinWrite) CloseHandle(ph->hStdinWrite);
    if (ph->hStdoutRead) CloseHandle(ph->hStdoutRead);
    free(ph);
}

#else

#include <vector>

struct ProcessHandle {
    pid_t pid;
    int   stdin_fd;
    int   stdout_fd;
};

int64_t aurora_process_spawn(const char* command) {
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);

        execl("/bin/sh", "sh", "-c", command, (char*)NULL);
        _exit(127);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    ProcessHandle* ph = (ProcessHandle*)malloc(sizeof(ProcessHandle));
    ph->pid = pid;
    ph->stdin_fd = stdin_pipe[1];
    ph->stdout_fd = stdout_pipe[0];
    return (int64_t)(intptr_t)ph;
}

int aurora_process_wait(int64_t handle) {
    ProcessHandle* ph = (ProcessHandle*)(intptr_t)handle;
    if (!ph) return -1;
    int status;
    waitpid(ph->pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

int aurora_process_kill(int64_t handle) {
    ProcessHandle* ph = (ProcessHandle*)(intptr_t)handle;
    if (!ph) return 0;
    return kill(ph->pid, SIGTERM) == 0 ? 1 : 0;
}

int aurora_process_pipe_read(int64_t handle, char* buffer, int buffer_size) {
    ProcessHandle* ph = (ProcessHandle*)(intptr_t)handle;
    if (!ph || ph->stdout_fd < 0 || !buffer || buffer_size <= 0) return -1;
    int n = (int)read(ph->stdout_fd, buffer, (size_t)buffer_size);
    return n;
}

int aurora_process_pipe_write(int64_t handle, const char* data, int len) {
    ProcessHandle* ph = (ProcessHandle*)(intptr_t)handle;
    if (!ph || ph->stdin_fd < 0 || !data || len <= 0) return -1;
    int n = (int)write(ph->stdin_fd, data, (size_t)len);
    return n;
}

void aurora_process_close(int64_t handle) {
    ProcessHandle* ph = (ProcessHandle*)(intptr_t)handle;
    if (!ph) return;
    if (ph->stdin_fd >= 0) close(ph->stdin_fd);
    if (ph->stdout_fd >= 0) close(ph->stdout_fd);
    free(ph);
}

#endif
