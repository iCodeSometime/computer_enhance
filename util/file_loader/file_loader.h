//
// Created by Kenneth Cochran on 10/29/26.
//

#ifndef FILE_LOADER_H
#define FILE_LOADER_H

typedef struct file {
    int size;
    char* data;
} file;

file load_file(const char* path);

#endif //FILE_LOADER_H
