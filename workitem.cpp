#include "workitem.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
extern "C"
{
    //160*144
    static atg_dtv::Encoder::VideoSettings global_video_settings;

    void init_global_video()
    {
        global_video_settings.fname = "";
        global_video_settings.width = 160;
        global_video_settings.height = 144;
        global_video_settings.inputWidth = 160;
        global_video_settings.inputHeight = 144;
        global_video_settings.frameRate = 60;
        global_video_settings.bitRate = 30000000;
        global_video_settings.hardwareEncoding = false;
        global_video_settings.inputAlpha = false;
    };

    workitem_t* create_workitem()
    {
        workitem_t *wi = (workitem_t*)malloc(sizeof(workitem_t));
        memset(wi,0,sizeof(*wi));
        wi->video_settings = new atg_dtv::Encoder::VideoSettings();
        *wi->video_settings = global_video_settings;
        wi->out_fd = -1;
        return wi;
    }

    void create_encoder(workitem_t* wi)
    {
        if(wi->len_video_file > 0)
        {
            printf("Generating encoder for %s. Hardware Accel: %d\n",wi->video_settings->fname.c_str(), wi->video_settings->hardwareEncoding);
            wi->out_encoder = new atg_dtv::Encoder();
            wi->out_encoder->run(*wi->video_settings, 144*160*4*60);
        }
    }

    void destroy_workitem(workitem_t* wi)
    {
        if(wi->out_fd>=0)
            close(wi->out_fd);
        if(wi->inputs)
            free(wi->inputs);
        if(wi->outdata)
            free(wi->outdata);
        if(wi->out_encoder)
        {
            wi->out_encoder->commit();
            wi->out_encoder->stop();
            delete wi->out_encoder;
            delete wi->video_settings;
        }
        free(wi);
    }

    int load_into_workitem(workitem_t* wi, char* path)
    {
        printf("Trying to load workitem from %s\n", path);
        int fd = open(path, O_RDONLY);
        if(fd<0)
        {
            perror("Couldn't open workitem file");
            return -1;
        }

        if(read(fd,&wi->magic,sizeof(wi->magic))!=sizeof(wi->magic))
        {
            perror("Couldn't read .magic");
            return -1;
        }
        if(wi->magic != WORKITEM_MAGIC)
        {
            perror(".magic value is invalid");
            return -1;
        }

        if(read(fd,&wi->version,sizeof(wi->version))!=sizeof(wi->version))
        {
            perror("Couldn't read .version");
            return -1;
        }
        if(wi->version != WORKITEM_VERSION)
        {
            perror(".version value is invalid");
            return -1;
        }

        if(read(fd,&wi->len_savestate_file,sizeof(wi->len_savestate_file))!=sizeof(wi->len_savestate_file))
        {
            perror("Couldn't read .len_savestate_file");
            return -1;
        }
        if(wi->len_savestate_file > WORKITEM_SAVESTATE_FILE_MAX_LEN)
        {
            perror(".len_savestate_file is invalid");
            return -1;
        }
        if(read(fd,wi->savestate_file,wi->len_savestate_file)!=(ssize_t)wi->len_savestate_file)
        {
            perror("Couldn't read .savestate_file");
            return -1;
        }

        if(read(fd,&wi->len_output_savestate,sizeof(wi->len_output_savestate))!=sizeof(wi->len_output_savestate))
        {
            perror("Couldn't read .len_output_savestate");
            return -1;
        }
        if(wi->len_output_savestate > WORKITEM_SAVESTATE_FILE_MAX_LEN)
        {
            perror(".len_output_savestate is invalid");
            return -1;
        }
        if(read(fd,wi->output_savestate,wi->len_output_savestate)!=(ssize_t)wi->len_output_savestate)
        {
            perror("Couldn't read .output_savestate");
            return -1;
        }

        if(read(fd,&wi->len_video_file,sizeof(wi->len_video_file))!=sizeof(wi->len_video_file))
        {
            perror("Couldn't read .len_video_file");
            return -1;
        }
        if(wi->len_video_file > WORKITEM_OUTPUT_FILE_MAX_LEN)
        {
            perror(".len_video_file is invalid");
            return -1;
        }
        if(read(fd,wi->output_video,wi->len_video_file)!=(ssize_t)wi->len_video_file)
        {
            perror("Couldn't read .output_video");
            return -1;
        }
        if(wi->len_video_file>0)
        {
            wi->video_settings->fname = wi->output_video;
            create_encoder(wi);
        }

        if(read(fd,&wi->len_output_file,sizeof(wi->len_output_file))!=sizeof(wi->len_output_file))
        {
            perror("Couldn't read .len_output_file");
            return -1;
        }
        if(wi->len_output_file > WORKITEM_OUTPUT_FILE_MAX_LEN)
        {
            perror(".len_output_file is invalid");
            return -1;
        }
        if(read(fd,wi->output_file,wi->len_output_file)!=(ssize_t)wi->len_output_file)
        {
            perror("Couldn't read .output_file");
            return -1;
        }

        if(read(fd,&wi->num_inputs,sizeof(wi->num_inputs))!=sizeof(wi->num_inputs))
        {
            perror("Couldn't read .num_inputs");
            return -1;
        }
        wi->inputs = (uint8_t*) malloc(wi->num_inputs);
        if(!wi->inputs)
        {
            perror("Couldn't allocate .inputs");
            return -1;
        }
        if(read(fd,wi->inputs,wi->num_inputs) != (ssize_t)wi->num_inputs)
        {
            perror("Couldn't read .inputs");
            return -1;
        }

        if(read(fd,&wi->num_outdata,sizeof(wi->num_outdata))!=sizeof(wi->num_outdata))
        {
            perror("Couldn't read .num_outdata");
            return -1;
        }
        if(wi->num_outdata>WORKITEM_MAX_OUTDATA)
        {
            printf("Number of requested outdata exceeds WORKITEM_MAX_OUTDATA: %lu\n", wi->num_outdata);
            return -1;
        }
        wi->outdata = (uint32_t*) calloc(wi->num_outdata,sizeof(uint32_t));
        if(!wi->outdata)
        {
            perror("Couldn't allocate .outdata");
            return -1;
        }
        if(read(fd,wi->outdata,wi->num_outdata*sizeof(uint32_t)) != (ssize_t)(wi->num_outdata*sizeof(uint32_t)))
        {
            perror("Couldn't read .outdata");
            return -1;
        }
        if(strlen(wi->output_file)>0)
        {
            wi->out_fd = open(wi->output_file, O_RDWR|O_CREAT, 0700);
            if(wi->out_fd<0)
            {
                perror("Couldn't open output file");
                return -1;
            }
        }
        else 
        {
            printf("Need to specify output file\n");
            return -1;
        }
        close(fd);
        return 0;
    }
}