#include <stdio.h>
#include <stdlib.h>
#include "util/preamble.h"
#include "../../util/virtual_arena/virtual_arena.h"

// void decode_instruction(u32 instruction) {
//
// }

int main(const int argc, char* argv[]) {
  if (argc != 2) {
    printf("Only accepts one argument for the binary filename");
    return 1;
  }

  const char* filename = argv[1];
  FILE* file = fopen(filename, "rb");

  if (file == NULL) {
    printf("Error opening file");
    return 1;
  }

  // using 128 bytes to match cache-line size
  u32 buffer[32];
  while(fread(&buffer, sizeof(buffer), 1, file)) {

  }

  fclose(file);
}
