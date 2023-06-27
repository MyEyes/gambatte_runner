#ifndef RUNNER_H
#define RUNNER_H
#include "workitem.h"

#define FIFO_BASE_PATH "/tmp/gb_worker_"
#define FIFO_FORMAT "%s%d_%s"

typedef struct
{
    size_t curr_frame;
    uint8_t outvals[WORKITEM_MAX_OUTDATA];
    workitem_t *curr_work;
} runner_state_t;

typedef struct
{
    int fifo_in_fd;
    int fifo_out_fd;
    char *read_buf_pos;
    char read_buf[WORKITEM_INPUT_FILE_MAX_LEN];
    runner_state_t state;
} runner_t;

int runner_init(runner_t* state);
int runner_init_fifos(runner_t* state);

workitem_t* runner_get_workitem(runner_t *runner);

int runner_write_outdata(runner_t* runner);
int runner_advance(runner_t* runner);
uint8_t runner_get_input(runner_t* runner);
int runner_finish(runner_t* runner);
#endif