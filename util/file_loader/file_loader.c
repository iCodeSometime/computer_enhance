//
// Created by Kenneth Cochran on 10/29/26.
//

#include "file_loader.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

// TODO: I guess since these are so small, it's better to use open/read
file load_file(char* file_name) {
    file file;

    int handle = open(file_name, O_RDONLY);
    if (handle == -1) {
        fprintf(stderr, "Failed to open file %s\n", file_name);
        file.size = -1;
        file.data = NULL;
        return file;
    }

    struct stat stats;
    if (fstat(handle, &stats) == -1) {
        close(handle);
        fprintf(stderr, "Failed to stat file %s\n", file_name);
        file.size = -1;
        file.data = NULL;
        return file;
    }

    char* data = mmap(NULL, stats.st_size, PROT_READ, MAP_PRIVATE, handle, 0);
    if (data == MAP_FAILED) {
        close(handle);
        fprintf(stderr, "Failed to mmap file %s\n", file_name);
        file.size = -1;
        file.data = NULL;
        return file;
    }

    madvise(data, stats.st_size, MADV_WILLNEED);

    file.size = stats.st_size;
    file.data = data;
    return file;
}