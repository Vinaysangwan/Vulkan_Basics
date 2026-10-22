#include "utils.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>

char* read_file(const char* file_path, u64 *file_size)
{
  FILE *file = fopen(file_path, "rb");

  if (!file)
  {
    LOG_ASSERT(FALSE, "Failed to Open file: %s", file_path);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  u64 len = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(len + 1);
  fread(buffer, sizeof(char), len, file);
  buffer[len] = 0;

  if (file_size)
  {
    *file_size = len;
  }

  return buffer;
}
