#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stddef.h>

#define FILENAME_SIZE 256
#define COMMAND_SIZE 4096
#define STATUS_SIZE 256

struct shared_data {
    char filename[FILENAME_SIZE];      // Имя файла для результатов
    char command[COMMAND_SIZE];        // Команда от родителя
    char status[STATUS_SIZE];          // Статус от ребенка
    int new_command;                   // Флаг новой команды (1 - есть, 0 - нет)
    int division_by_zero;              // Флаг деления на ноль
    int parent_alive;                  // Флаг что родитель жив
};

#endif