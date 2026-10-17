#pragma once

#include "defines.h"

#define DARRAY_DEFAULT_CAPACITY 1
#define DARRAY_RESIZE_FACTOR 2

typedef struct _Darray_Header
{
  u64 cap;
  u64 len;
  u64 stride;
} _Darray_Header;

void* _darray_create(u64 len, u64 stride);
void _darray_destroy(void* array);

b8 _darray_is_resizable(void *array);
void *_darray_resize(void* array);

#define _darray_header(a) ((_Darray_Header *)(a) - 1)

#define darray_create(t) _darray_create(DARRAY_DEFAULT_CAPACITY, sizeof (t))

#define darray_reserve(t, s) _darray_create(s , sizeof (t))

#define darray_len(a) _darray_header((a))->len
#define darray_capacity(a) _darray_header((a))->cap
#define darray_stride(a) _darray_header((a))->stride

#define darray_push(a, v)               \
do {                                    \
  if (_darray_is_resizable((a)))        \
  {                                     \
    (a) = _darray_resize((a));          \
  }                                     \
  (a)[_darray_header((a))->len++] = (v);\
} while(0)

#define darray_pop(a)\
do {\
  if (_darray_header((a))->len)\
  {\
    _darray_header((a))->len--;\
  }\
} while(0)
