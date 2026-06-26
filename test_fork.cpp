#include <unistd.h>
#include <sys/wait.h>
#include <cstdio>
#include <cerrno>
#include <string>
#include <iostream>
int main() {
    int pipefd[2];
    if (pipe(pipefd) < 0) { std::cerr << "pipe failed\n"; return 1; }
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execlp("echo", "echo", "hello from posix", nullptr);
        _exit(127);
    } else if (pid < 0) { std::cerr << "fork failed\n"; return 1; }
    close(pipefd[1]);
    std::string result;
    char buf[256];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf)-1)) > 0) {
        buf[n] = 0; result += buf;
    }
    close(pipefd[0]);
    int status; waitpid(pid, &status, 0);
    std::cout << "CAPTURED: [" << result << "]\n";
    std::cout << "EXIT: " << WEXITSTATUS(status) << "\n";
    return 0;
}
