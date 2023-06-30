#ifndef WORKITEM_H
#define WORKITEM_H

#include <stdint.h>
#include <stdlib.h>
#include <encoder.h>
#define WORKITEM_SAVESTATE_FILE_MAX_LEN 128
#define WORKITEM_OUTPUT_FILE_MAX_LEN 256
#define WORKITEM_INPUT_FILE_MAX_LEN 256
#define WORKITEM_MAX_OUTDATA 32

#define WORKITEM_MAGIC 0x4B524F57
#define WORKITEM_VERSION 0x00000100

typedef struct {
    uint32_t magic;
    uint32_t version;
    size_t len_savestate_file;
    char savestate_file[WORKITEM_SAVESTATE_FILE_MAX_LEN];
    size_t len_output_file;
    char output_file[WORKITEM_OUTPUT_FILE_MAX_LEN];
    size_t len_output_savestate;
    char output_savestate[WORKITEM_SAVESTATE_FILE_MAX_LEN];
    size_t len_video_file;
    char output_video[WORKITEM_OUTPUT_FILE_MAX_LEN];
    size_t num_inputs;
    uint8_t *inputs;
    size_t num_outdata;
    uint32_t *outdata;

    int out_fd;
    atg_dtv::Encoder *out_encoder;
    atg_dtv::Encoder::VideoSettings *video_settings;
} workitem_t;

extern "C"
{
    void init_global_video();
    workitem_t* create_workitem();
    void destroy_workitem(workitem_t* wi);
    int load_into_workitem(workitem_t* wi, char* file);
}

#endif