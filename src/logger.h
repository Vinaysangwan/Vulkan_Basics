#pragma once

#include <stdio.h>

#define LOG_INFO(msg, ...)     \
do {                           \
  printf("[INFO]: ");          \
  printf((msg), ##__VA_ARGS__);\
  printf("\n");                \
} while(0)

#define LOG_ERROR(msg, ...)     \
do {                            \
  printf("\033[1;31m[ERROR]: ");\
  printf((msg), ##__VA_ARGS__); \
  printf("\033[0m\n");          \
} while(0)

#define LOG_ASSERT(x, msg, ...)                                                               \
do {                                                                                          \
  if (!(x))                                                                                   \
  {                                                                                           \
    printf("\033[1;31m[ASSERT]: expr: %s\nfile: %s, Line: %d\nmsg: ", #x, __FILE__, __LINE__);\
    printf((msg), ##__VA_ARGS__);                                                             \
    printf("\033[0m\n");                                                                      \
    __builtin_trap();                                                                         \
  }                                                                                           \
} while(0)
