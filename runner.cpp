#include "runner.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "workitem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

int runner_init(runner_t* runner)
{
    int ret = runner_init_fifos(runner);
    runner->read_buf_pos = runner->read_buf;
    return ret;
}

int runner_init_fifos(runner_t* runner)
{
    char path_buf[256] = {0};
    runner->fifo_in_fd = -1;
    runner->fifo_out_fd = -1;
    int pid = getpid();

    snprintf(path_buf, sizeof(path_buf), FIFO_FORMAT, FIFO_BASE_PATH, pid, "in");
    if(mkfifo(path_buf, 0700)<0)
    {
        perror("Couldn't create in fifo");
        return -1;
    }
    
    printf("Waiting for other end of in pipe to be opened\n");
    runner->fifo_in_fd = open(path_buf, O_RDONLY);
    if(runner->fifo_in_fd<0)
    {
        remove(path_buf);
        perror("Couldn't open created in fifo");
        return -1;
    }

    //At this point incoming fifo is created and opened
    snprintf(path_buf, sizeof(path_buf), FIFO_FORMAT, FIFO_BASE_PATH, pid, "out");
    if(mkfifo(path_buf, 0700)<0)
    {
        //Cleanup first fifo
        snprintf(path_buf, sizeof(path_buf), FIFO_FORMAT, FIFO_BASE_PATH, pid, "in");
        remove(path_buf);
        perror("Couldn't create out fifo");
        return -1;
    }
    printf("Waiting for other end of out pipe to be opened\n");
    runner->fifo_out_fd = open(path_buf, O_WRONLY);
    if(runner->fifo_out_fd<0) //We created the fifo but couldn't open it for some reason
    {
        remove(path_buf); //Remove out fifo
        snprintf(path_buf, sizeof(path_buf), FIFO_FORMAT, FIFO_BASE_PATH, pid, "in");
        remove(path_buf); //Remove in fifo
        perror("Couldn't open out fifo");
        return -1;
    }
    //At this point both fifos should be created and opened
    return 0;
}

void runner_read_to_buf(runner_t *runner)
{
    size_t remaining_space = runner->read_buf+WORKITEM_INPUT_FILE_MAX_LEN-runner->read_buf_pos;
    size_t read_bytes = read(runner->fifo_in_fd, runner->read_buf_pos, remaining_space);
    runner->read_buf_pos += read_bytes;
}

void runner_consume_from_buf(runner_t *runner, size_t num_bytes)
{
    char* copy_start = runner->read_buf+num_bytes;
    size_t copy_bytes = copy_start - runner->read_buf_pos;
    memcpy(runner->read_buf, copy_start, copy_bytes);
    runner->read_buf_pos = runner->read_buf+copy_bytes;
}

workitem_t* runner_get_workitem(runner_t *runner)
{
    printf("Getting workitem\n");
    char path_buf[WORKITEM_INPUT_FILE_MAX_LEN] = {};
    workitem_t *new_workitem = create_workitem();
    size_t bufsize = runner->read_buf_pos-runner->read_buf;
    while(!bufsize || !memchr(runner->read_buf,0,bufsize))
    {
        runner_read_to_buf(runner);
        bufsize = runner->read_buf_pos-runner->read_buf;
    }
    //We only ever get here if there's a 0 byte in the buffer
    strcpy(path_buf, runner->read_buf);
    size_t n_bytes = strlen(path_buf)+1;
    runner_consume_from_buf(runner, n_bytes);
    if(load_into_workitem(new_workitem, path_buf)<0)
    {
        free(new_workitem);
        return NULL;
    }
    printf("Finished loading\n");
    return new_workitem;
}

int runner_write_outdata(runner_t* runner)
{
    for(size_t i=0; i<runner->state.curr_work->num_outdata; i++)
    {
        uint8_t val = runner->state.outvals[i];
        //printf("%x:%x\n", i, val);
        write(runner->state.curr_work->out_fd, &val, 1);
    }
    return 0;
}

int runner_advance(runner_t* runner)
{
    if(runner->state.curr_frame < runner->state.curr_work->num_inputs-1)
    {
        runner->state.curr_frame++;
        return 0;
    }
    return 1;
}

uint8_t runner_get_input(runner_t* runner)
{
    if(runner->state.curr_frame < runner->state.curr_work->num_inputs)
    {
        return runner->state.curr_work->inputs[runner->state.curr_frame];
    }
    return 0;
}

int runner_finish(runner_t* runner)
{
    destroy_workitem(runner->state.curr_work);

    //Signal that we finished
    write(runner->fifo_out_fd,&runner->state.curr_frame,sizeof(runner->state.curr_frame));

    int wasDone = (runner->state.curr_frame == runner->state.curr_work->num_inputs);
    runner->state.curr_work = 0;
    //We weren't done yet
    if(!wasDone)
    {
        return -1;
    }
    return 0;
}