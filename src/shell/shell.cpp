#include "shell.hpp"

#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

uint8_t mcc::shell::run_shell_command(
    fpath cwd,
    string cmd,
    string* output
) {
    if (output != nullptr) {
        output->clear();
    }

    int pipe_fds[2];

    if (pipe(pipe_fds) == -1) {
        return 255;
    }

    const pid_t pid = fork();

    if (pid == -1) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return 255;
    }

    if (pid == 0) {
        // Child process.

        close(pipe_fds[0]);

        // Redirect stdout and stderr to the pipe.
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);

        close(pipe_fds[1]);

        // Change working directory.
        if (chdir(cwd.c_str()) == -1) {
            const char* message = std::strerror(errno);
            write(STDERR_FILENO, message, std::strlen(message));
            write(STDERR_FILENO, "\n", 1);
            _exit(127);
        }

        // Run the command through the user's shell.
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);

        // Only reached if exec fails.
        const char* message = std::strerror(errno);
        write(STDERR_FILENO, message, std::strlen(message));
        write(STDERR_FILENO, "\n", 1);

        _exit(127);
    }

    // Parent process.
    close(pipe_fds[1]);

    char buffer[4096];

    for (;;) {
        const ssize_t count = read(pipe_fds[0], buffer, sizeof(buffer));

        if (count > 0) {
            if (output != nullptr) {
                output->append(buffer, static_cast<std::size_t>(count));
            }
        } else if (count == 0) {
            break; // End of file.
        } else if (errno != EINTR) {
            break;
        }
    }

    close(pipe_fds[0]);

    int status = 0;

    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            return 255;
        }
    }

    if (WIFEXITED(status)) {
        return static_cast<uint8_t>(WEXITSTATUS(status));
    }

    if (WIFSIGNALED(status)) {
        // Represent a signal termination as 128 + signal number.
        const int code = 128 + WTERMSIG(status);
        return static_cast<uint8_t>(code > 255 ? 255 : code);
    }

    return 255;
}
