#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        const char msg[] = "Error: output filename not provided\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    const char *filename = argv[1];
    
    // �������� ����� ��� ������ �����������
    int output_file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_file == -1) {
        const char msg[] = "Error: cannot open output file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    char buf[4096];
    ssize_t n;
    size_t pos = 0;

    // ������ ��������� � ����
    const char header[] = "Calculation Results:\n====================\n";
    write(output_file, header, sizeof(header) - 1);

    while ((n = read(STDIN_FILENO, buf + pos, sizeof(buf) - pos - 1)) > 0) {
        pos += n;
        buf[pos] = '\0';

        char *current_line = buf;
        char *line_end;

        // ��������� ������ ������
        while ((line_end = strchr(current_line, '\n'))) {
            *line_end = '\0';

            // ������� ������ �����
            if (strlen(current_line) == 0) {
                current_line = line_end + 1;
                continue;
            }

            float result = 0.0;
            int numbers_seen = 0;
            int division_by_zero = 0;
            int valid_input = 1;
            
            // ������� ����� �� ������
            char *ptr = current_line;
            while (*ptr) {
                // ������� ��������
                while (*ptr && isspace((unsigned char)*ptr)) {
                    ptr++;
                }
                if (!*ptr) {
                    break;
                }

                // �������� �� ������������� �����
                int is_negative = 0;
                if (*ptr == '-') {
                    is_negative = 1;
                    ptr++;
                }

                // ������� ����� �����
                float number = 0.0;
                int digits_found = 0;
                while (*ptr && isdigit((unsigned char)*ptr)) {
                    number = number * 10.0 + (*ptr - '0');
                    ptr++;
                    digits_found = 1;
                }

                // ������� ������� �����
                if (*ptr == '.') {
                    ptr++;
                    float fraction = 0.1;
                    while (*ptr && isdigit((unsigned char)*ptr)) {
                        number += (*ptr - '0') * fraction;
                        fraction *= 0.1;
                        ptr++;
                        digits_found = 1;
                    }
                }

                if (!digits_found) {
                    // ������� ������������ ��������
                    while (*ptr && !isspace((unsigned char)*ptr)) ptr++;
                    continue;
                }

                if (is_negative) {
                    number = -number;
                }

                numbers_seen++;

                if (numbers_seen == 1) {
                    // ������ ����� - �������
                    result = number;
                } else {
                    // ����������� ����� - ��������
                    if (number == 0.0) {
                        division_by_zero = 1;
                        break;
                    }
                    result /= number;
                }

                // ������� ���������� ��-���������� ��������
                while (*ptr && !isspace((unsigned char)*ptr)) {
                    valid_input = 0;
                    break;
                }
                if (!valid_input) {
                    break;
                }
            }

            // ��������� ����������� ����������
            if (!valid_input) {
                const char msg[] = "Error: invalid input format\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                
                char error_entry[128];
                int len = snprintf(error_entry, sizeof(error_entry), 
                                 "Input: \"%s\" -> Error: invalid format\n", current_line);
                write(output_file, error_entry, len);
            } else if (division_by_zero) {
                const char msg[] = "Error: division by zero\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                
                char error_entry[128];
                int len = snprintf(error_entry, sizeof(error_entry), 
                                 "Input: \"%s\" -> Error: division by zero\n", current_line);
                write(output_file, error_entry, len);
                
                // ���������� ������ ��� ������� �� ����
                close(output_file);
                exit(EXIT_FAILURE);
            } else if (numbers_seen < 2) {
                const char msg[] = "Error: not enough numbers (need at least 2)\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                
                char error_entry[128];
                int len = snprintf(error_entry, sizeof(error_entry), 
                                 "Input: \"%s\" -> Error: not enough numbers\n", current_line);
                write(output_file, error_entry, len);
            } else {
                // �������� ���������� - ������ ���������� � ����
                char result_entry[128];
                int len = snprintf(result_entry, sizeof(result_entry), 
                                 "Input: \"%s\" -> Result: %.6f\n", current_line, result);
                write(output_file, result_entry, len);

                const char success_msg[] = "Calculation completed successfully\n";
                write(STDERR_FILENO, success_msg, sizeof(success_msg) - 1);
            }

            current_line = line_end + 1;
        }

        // ���������� �������������� ����� ������
        pos = strlen(current_line);
        memmove(buf, current_line, pos);
    }

    // ������ ������������ ��������� � ����
    const char footer[] = "\nEnd of calculations.\n";
    write(output_file, footer, sizeof(footer) - 1);
    
    close(output_file);
    return 0;
}