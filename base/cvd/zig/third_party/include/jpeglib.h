#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JPEG_LIB_VERSION 62
#define DCTSIZE 8
#define DCTSIZE2 64

typedef unsigned char JSAMPLE;
typedef JSAMPLE* JSAMPROW;
typedef JSAMPROW* JSAMPARRAY;
typedef JSAMPARRAY* JSAMPIMAGE;
typedef unsigned int JDIMENSION;
typedef int boolean;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef enum {
  JCS_UNKNOWN,
  JCS_GRAYSCALE,
  JCS_RGB,
  JCS_YCbCr,
  JCS_CMYK,
  JCS_YCCK,
  JCS_EXT_RGB,
  JCS_EXT_RGBX,
  JCS_EXT_BGR,
  JCS_EXT_BGRX,
  JCS_EXT_XBGR,
  JCS_EXT_XRGB,
  JCS_EXT_RGBA,
  JCS_EXT_BGRA,
  JCS_EXT_ABGR,
  JCS_EXT_ARGB,
  JCS_RGB565
} J_COLOR_SPACE;

typedef enum {
  JDCT_ISLOW,
  JDCT_IFAST,
  JDCT_FLOAT
} J_DCT_METHOD;

struct jpeg_common_struct;
struct jpeg_compress_struct;
struct jpeg_decompress_struct;

typedef struct jpeg_common_struct* j_common_ptr;
typedef struct jpeg_compress_struct* j_compress_ptr;
typedef struct jpeg_decompress_struct* j_decompress_ptr;

struct jpeg_error_mgr {
  void (*error_exit)(j_common_ptr cinfo);
  void (*emit_message)(j_common_ptr cinfo, int msg_level);
  void (*output_message)(j_common_ptr cinfo);
  void (*format_message)(j_common_ptr cinfo, char* buffer);
  void (*reset_error_mgr)(j_common_ptr cinfo);
  int msg_code;
  union {
    int i[8];
    char s[80];
  } msg_parm;
  int trace_level;
  long num_warnings;
  const char* const* jpeg_message_table;
  int last_jpeg_message;
  const char* const* addon_message_table;
  int first_addon_message;
  int last_addon_message;
};

struct jpeg_destination_mgr {
  unsigned char* next_output_byte;
  size_t free_in_buffer;
  void (*init_destination)(j_compress_ptr cinfo);
  boolean (*empty_output_buffer)(j_compress_ptr cinfo);
  void (*term_destination)(j_compress_ptr cinfo);
};

struct jpeg_source_mgr {
  const unsigned char* next_input_byte;
  size_t bytes_in_buffer;
  void (*init_source)(j_decompress_ptr cinfo);
  boolean (*fill_input_buffer)(j_decompress_ptr cinfo);
  void (*skip_input_data)(j_decompress_ptr cinfo, long num_bytes);
  boolean (*resync_to_restart)(j_decompress_ptr cinfo, int desired);
  void (*term_source)(j_decompress_ptr cinfo);
};

typedef struct {
  int component_id;
  int component_index;
  int h_samp_factor;
  int v_samp_factor;
  int quant_tbl_no;
  int dc_tbl_no;
  int ac_tbl_no;
  JDIMENSION width_in_blocks;
  JDIMENSION height_in_blocks;
  int DCT_scaled_size;
  JDIMENSION downsampled_width;
  JDIMENSION downsampled_height;
  boolean component_needed;
} jpeg_component_info;

struct jpeg_common_struct {
  struct jpeg_error_mgr* err;
  void* mem;
  void* progress;
  void* client_data;
  boolean is_decompressor;
  int global_state;
};

struct jpeg_compress_struct {
  struct jpeg_error_mgr* err;
  void* mem;
  void* progress;
  void* client_data;
  boolean is_decompressor;
  int global_state;

  struct jpeg_destination_mgr* dest;
  JDIMENSION image_width;
  JDIMENSION image_height;
  int input_components;
  J_COLOR_SPACE in_color_space;
  double input_gamma;
  int data_precision;
  int num_components;
  J_COLOR_SPACE jpeg_color_space;
  jpeg_component_info* comp_info;
  boolean raw_data_in;
  J_DCT_METHOD dct_method;
  JDIMENSION next_scanline;
  int max_h_samp_factor;
  int max_v_samp_factor;

  unsigned char** outbuffer_ptr;
  unsigned long* outsize_ptr;
  jpeg_component_info comp_info_storage[4];
};

struct jpeg_decompress_struct {
  struct jpeg_error_mgr* err;
  void* mem;
  void* progress;
  void* client_data;
  boolean is_decompressor;
  int global_state;

  struct jpeg_source_mgr* src;
  JDIMENSION image_width;
  JDIMENSION image_height;
  int num_components;
  J_COLOR_SPACE jpeg_color_space;
  J_COLOR_SPACE out_color_space;
  unsigned int scale_num, scale_denom;
  double output_gamma;
  boolean buffered_image;
  boolean raw_data_out;
  J_DCT_METHOD dct_method;
  boolean do_fancy_upsampling;
  boolean do_block_smoothing;
  JDIMENSION output_width;
  JDIMENSION output_height;
  int out_color_components;
  int output_components;
  JDIMENSION output_scanline;
  jpeg_component_info* comp_info;
  int max_h_samp_factor;
  int max_v_samp_factor;
};

#define JPEG_HEADER_OK 1
#define JPEG_SUSPENDED 0

struct jpeg_error_mgr* jpeg_std_error(struct jpeg_error_mgr* err);

void jpeg_CreateCompress(j_compress_ptr cinfo, int version, size_t structsize);
void jpeg_CreateDecompress(j_decompress_ptr cinfo, int version,
                           size_t structsize);

#define jpeg_create_compress(cinfo) \
  jpeg_CreateCompress((cinfo), JPEG_LIB_VERSION, sizeof(struct jpeg_compress_struct))
#define jpeg_create_decompress(cinfo) \
  jpeg_CreateDecompress((cinfo), JPEG_LIB_VERSION, sizeof(struct jpeg_decompress_struct))

void jpeg_destroy_compress(j_compress_ptr cinfo);
void jpeg_destroy_decompress(j_decompress_ptr cinfo);
void jpeg_destroy(j_common_ptr cinfo);

void jpeg_stdio_dest(j_compress_ptr cinfo, FILE* outfile);
void jpeg_stdio_src(j_decompress_ptr cinfo, FILE* infile);
void jpeg_mem_dest(j_compress_ptr cinfo, unsigned char** outbuffer,
                   unsigned long* outsize);
void jpeg_mem_src(j_decompress_ptr cinfo, const unsigned char* inbuffer,
                  unsigned long insize);

void jpeg_set_defaults(j_compress_ptr cinfo);
void jpeg_set_colorspace(j_compress_ptr cinfo, J_COLOR_SPACE colorspace);
void jpeg_set_quality(j_compress_ptr cinfo, int quality,
                      boolean force_baseline);
void jpeg_start_compress(j_compress_ptr cinfo, boolean write_all_tables);
JDIMENSION jpeg_write_scanlines(j_compress_ptr cinfo, JSAMPARRAY scanlines,
                                JDIMENSION num_lines);
JDIMENSION jpeg_write_raw_data(j_compress_ptr cinfo, JSAMPIMAGE data,
                               JDIMENSION num_lines);
void jpeg_finish_compress(j_compress_ptr cinfo);

int jpeg_read_header(j_decompress_ptr cinfo, boolean require_image);
boolean jpeg_start_decompress(j_decompress_ptr cinfo);
JDIMENSION jpeg_read_scanlines(j_decompress_ptr cinfo, JSAMPARRAY scanlines,
                               JDIMENSION max_lines);
JDIMENSION jpeg_read_raw_data(j_decompress_ptr cinfo, JSAMPIMAGE data,
                              JDIMENSION max_lines);
boolean jpeg_finish_decompress(j_decompress_ptr cinfo);
void jpeg_abort_decompress(j_decompress_ptr cinfo);

#ifdef __cplusplus
}
#endif
