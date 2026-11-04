#pragma once

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PNG_LIBPNG_VER_STRING "1.6.37"
#define PNG_HEADER_VERSION_STRING " libpng version 1.6.37\n"
#define PNG_LIBPNG_VER 10637

typedef unsigned int png_uint_32;
typedef int png_int_32;
typedef uint16_t png_uint_16;
typedef int16_t png_int_16;
typedef uint8_t png_byte;
typedef size_t png_size_t;
typedef char* png_charp;
typedef const char* png_const_charp;
typedef png_byte* png_bytep;
typedef const png_byte* png_const_bytep;
typedef png_byte** png_bytepp;
typedef void* png_voidp;

typedef struct png_struct_def png_struct;
typedef png_struct* png_structp;
typedef png_struct** png_structpp;
typedef const png_struct* png_const_structrp;
typedef png_struct* png_structrp;

typedef struct png_info_def png_info;
typedef png_info* png_infop;
typedef png_info** png_infopp;
typedef const png_info* png_const_inforp;
typedef png_info* png_inforp;

typedef void (*png_error_ptr)(png_structp, png_const_charp);
typedef void (*png_rw_ptr)(png_structp, png_bytep, png_size_t);
typedef void (*png_flush_ptr)(png_structp);

#define PNG_COLOR_MASK_PALETTE 1
#define PNG_COLOR_MASK_COLOR 2
#define PNG_COLOR_MASK_ALPHA 4

#define PNG_COLOR_TYPE_GRAY 0
#define PNG_COLOR_TYPE_PALETTE (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE)
#define PNG_COLOR_TYPE_RGB (PNG_COLOR_MASK_COLOR)
#define PNG_COLOR_TYPE_RGB_ALPHA (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_ALPHA)
#define PNG_COLOR_TYPE_GRAY_ALPHA (PNG_COLOR_MASK_ALPHA)
#define PNG_COLOR_TYPE_RGBA PNG_COLOR_TYPE_RGB_ALPHA
#define PNG_COLOR_TYPE_GA PNG_COLOR_TYPE_GRAY_ALPHA

#define PNG_INTERLACE_NONE 0
#define PNG_INTERLACE_ADAM7 1

#define PNG_COMPRESSION_TYPE_BASE 0
#define PNG_COMPRESSION_TYPE_DEFAULT PNG_COMPRESSION_TYPE_BASE

#define PNG_FILTER_TYPE_BASE 0
#define PNG_FILTER_TYPE_DEFAULT PNG_FILTER_TYPE_BASE

#define PNG_TRANSFORM_IDENTITY 0x0000

jmp_buf* png_jmpbuf_impl(png_structp png_ptr);
#define png_jmpbuf(png_ptr) (*png_jmpbuf_impl(png_ptr))

png_structp png_create_write_struct(png_const_charp user_png_ver,
                                    png_voidp error_ptr, png_error_ptr error_fn,
                                    png_error_ptr warn_fn);
png_structp png_create_read_struct(png_const_charp user_png_ver,
                                   png_voidp error_ptr, png_error_ptr error_fn,
                                   png_error_ptr warn_fn);
png_infop png_create_info_struct(png_const_structrp png_ptr);

void png_destroy_write_struct(png_structpp png_ptr_ptr,
                              png_infopp info_ptr_ptr);
void png_destroy_read_struct(png_structpp png_ptr_ptr, png_infopp info_ptr_ptr,
                             png_infopp end_info_ptr_ptr);

void png_init_io(png_structrp png_ptr, FILE* fp);
void png_set_write_fn(png_structrp png_ptr, png_voidp io_ptr,
                      png_rw_ptr write_data_fn, png_flush_ptr output_flush_fn);
void png_set_read_fn(png_structrp png_ptr, png_voidp io_ptr,
                     png_rw_ptr read_data_fn);
png_voidp png_get_io_ptr(png_const_structrp png_ptr);

void png_set_IHDR(png_const_structrp png_ptr, png_inforp info_ptr,
                  png_uint_32 width, png_uint_32 height, int bit_depth,
                  int color_type, int interlace_method, int compression_method,
                  int filter_method);

void png_set_rows(png_const_structrp png_ptr, png_inforp info_ptr,
                  png_bytepp row_pointers);
void png_write_png(png_structrp png_ptr, png_inforp info_ptr, int transforms,
                   png_voidp params);
void png_write_info(png_structrp png_ptr, png_const_inforp info_ptr);
void png_write_image(png_structrp png_ptr, png_bytepp image);
void png_write_row(png_structrp png_ptr, png_const_bytep row);
void png_write_end(png_structrp png_ptr, png_inforp info_ptr);

#ifdef __cplusplus
}
#endif
