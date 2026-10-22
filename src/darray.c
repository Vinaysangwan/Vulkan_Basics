#include "darray.h"

#include <stdlib.h>
#include <string.h>


void* _darray_create(u64 len, u64 stride)
{
  u64 header_size = sizeof(_Darray_Header);
  u64 array_size = len * stride;
  u64 total_size = header_size + array_size;

  _Darray_Header* header = malloc(total_size);
  memset(header, 0, total_size);

  header->cap = len;
  header->len = 0;
  header->stride = stride;

  return (void *)(header + 1);
}

void _darray_destroy(void *array)
{
  _Darray_Header* header = _darray_header(array);
  free((void *)header);
  array = 0;
}

b8 _darray_is_resizable(void *array)
{
  _Darray_Header* header = _darray_header(array);
  u64 cap = header->cap;
  u64 len = header->len;

  if (len + 1 > cap)
  {
    return TRUE;
  }

  return FALSE;
}

void *_darray_resize(void *array)
{
  _Darray_Header *header = _darray_header(array);

  u64 old_len = header->len;
  u64 old_cap = header->cap;
  u64 stride  = header->stride;
  u64 new_cap = old_cap ? old_cap * DARRAY_RESIZE_FACTOR : DARRAY_DEFAULT_CAPACITY;

  void *temp = _darray_create(new_cap, stride);
  memcpy(temp, array, stride * old_len);

  _darray_header(temp)->len = old_len;

  _darray_destroy(array);
  return temp;
}

void *_darray_resize_len(u64 len, u64 stride)
{
  void *array = _darray_create(len, stride);
  _darray_header(array)->len = len;

  return array;
}
