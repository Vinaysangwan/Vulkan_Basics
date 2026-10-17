#pragma once

#include <assert.h>

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef float f32;
typedef double f64;

typedef char b8;
#define TRUE 1
#define FALSE 0

static_assert(sizeof(i8) == 1, "Size of i8 is not 1 byte");
static_assert(sizeof(i16) == 2, "Size of i16 is not 2 byte");
static_assert(sizeof(i32) == 4, "Size of i32 is not 4 byte");
static_assert(sizeof(i64) == 8, "Size of i64 is not 8 byte");

static_assert(sizeof(u8) == 1, "Size of u8 is not 1 byte");
static_assert(sizeof(u16) == 2, "Size of u16 is not 2 byte");
static_assert(sizeof(u32) == 4, "Size of u32 is not 4 byte");
static_assert(sizeof(u64) == 8, "Size of u64 is not 8 byte");

static_assert(sizeof(f32) == 4, "Size of f32 is not 4 byte");
static_assert(sizeof(f64) == 8, "Size of f64 is not 8 byte");

static_assert(sizeof(b8) == 1, "Size of b8 is not 1 byte");
