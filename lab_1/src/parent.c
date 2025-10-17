#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    char filename[1024];
    
    // ������ ����� ����� � ������������
    {
        const char msg[] = "Enter output filename: ";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);

        ssize_t n = read(STDIN_FILENO, filename, sizeof(filename) - 1);
        if (n <= 0) {
            const char msg[] = "Error: cannot read filename\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        filename[n - 1] = '\0'; // ������� ������ ����� ������
    }

    // �������� ������� ��� �������������� ��������������
    int parent_to_child[2];  // pipe1 - ��� �������� ������ �� �������� � �������
    int child_to_parent[2];  // pipe2 - ��� �������� ������� �� ������� � ��������

    if (pipe(parent_to_child) == -1) {
        const char msg[] = "Error: cannot create pipe1\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    if (pipe(child_to_parent) == -1) {
        const char msg[] = "Error: cannot create pipe2\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    // �������� ��������� ��������
    pid_t pid = fork();

    switch(pid) {
        case -1: {
            // ������ �������� ��������
            const char msg[] = "Error: cannot create child process\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        case 0: {
            // �������� �������
            close(parent_to_child[1]);  // ��������� ������ � pipe1
            close(child_to_parent[0]);  // ��������� ������ �� pipe2

            // �������������� ����������� ���� �� ������ �� pipe1
            dup2(parent_to_child[0], STDIN_FILENO);
            close(parent_to_child[0]);

            // �������������� ����������� ����� ������ �� ������ � pipe2
            dup2(child_to_parent[1], STDERR_FILENO);
            close(child_to_parent[1]);

            // ��������� ��������� ��������� �������� � ��������� ����� �����
            execl("./child", "child", filename, NULL);
            
            // ���� execl ������ ���������� - ��������� ������
            const char msg[] = "Error: cannot execute child process\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        default: {
            // ������������ �������
            close(parent_to_child[0]);  // ��������� ������ �� pipe1
            close(child_to_parent[1]);  // ��������� ������ � pipe2

            const char prompt[] = "Enter numbers separated by spaces (or 'exit' to quit):\n";
            write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);

            char buffer[4096];
            int child_alive = 1;

            while (child_alive) {
                // ������ ������� �� ������������
                const char input_prompt[] = "> ";
                write(STDOUT_FILENO, input_prompt, sizeof(input_prompt) - 1);

                ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
                if (n <= 0) break;

                buffer[n - 1] = '\0'; // ������� ������ ����� ������

                // �������� �� ������� ������
                if (strcmp(buffer, "exit") == 0) {
                    break;
                }

                // �������� �� ������ ����
                if (strlen(buffer) == 0) {
                    continue;
                }

                // ��������� ������ ����� ������ ��� ���������� ���������
                buffer[n - 1] = '\n';
                buffer[n] = '\0';

                // �������� ������� ��������� �������� ����� pipe1
                write(parent_to_child[1], buffer, n);

                // �������� ������� �� ��������� �������� ����� pipe2
                char status_buf[256];
                fd_set readfds;
                struct timeval timeout;
                
                FD_ZERO(&readfds);
                FD_SET(child_to_parent[0], &readfds);
                timeout.tv_sec = 0;
                timeout.tv_usec = 100000; // 100ms

                int ready = select(child_to_parent[0] + 1, &readfds, NULL, NULL, &timeout);
                if (ready > 0) {
                    ssize_t status_n = read(child_to_parent[0], status_buf, sizeof(status_buf) - 1);
                    if (status_n > 0) {
                        status_buf[status_n] = '\0';
                        if (strstr(status_buf, "division by zero") != NULL) {
                            const char error_msg[] = "Error: division by zero detected. Terminating...\n";
                            write(STDERR_FILENO, error_msg, sizeof(error_msg) - 1);
                            child_alive = 0;
                        }
                        // ����� ������ ��������� �� ��������� ��������
                        write(STDERR_FILENO, status_buf, status_n);
                    }
                }
            }

            // �������� ������� � �������� ���������� ��������� ��������
            close(parent_to_child[1]);
            close(child_to_parent[0]);

            // �������� ���������� ��������� ��������
            int status;
            wait(&status);

            const char exit_msg[] = "Parent process terminated.\n";
            write(STDOUT_FILENO, exit_msg, sizeof(exit_msg) - 1);
        }
    }

    return 0;
}