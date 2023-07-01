#include <gambatte.h>
#include <inputgetter.h>
#include <gbint.h>
#include <loadres.h>
#include "runner.h"
#include <stdio.h>
#include <cstddef>
#include <string>

static runner_t runner;
static gambatte::GB *gb;

//enum Button { A     = 0x01, B    = 0x02, SELECT = 0x04, START = 0x08,
//	              RIGHT = 0x10, LEFT = 0x20, UP     = 0x40, DOWN  = 0x80 };
uint8_t inputGlyphs[] = {
    0,0,1,0,0, 1,1,1,1,0, 0,0,0,0,0, 0,1,1,1,1, 1,1,0,0,0, 0,0,0,1,1, 0,0,1,0,0, 1,1,1,1,1,
    0,1,0,1,0, 1,0,0,0,1, 0,0,1,1,0, 1,0,0,0,0, 1,1,1,1,0, 0,1,1,1,1, 0,1,1,1,0, 1,1,1,1,1,
    1,0,0,0,1, 1,1,1,1,0, 0,1,0,0,0, 0,1,1,1,0, 1,1,1,1,1, 1,1,1,1,1, 0,1,1,1,0, 0,1,1,1,0,
    1,1,1,1,1, 1,0,0,0,1, 0,0,1,1,0, 0,0,0,0,1, 1,1,1,1,0, 0,1,1,1,1, 1,1,1,1,1, 0,1,1,1,0,
    1,0,0,0,1, 1,1,1,1,0, 0,1,1,1,0, 1,1,1,1,1, 1,1,0,0,0, 0,0,0,1,1, 1,1,1,1,1, 0,0,1,0,0,
};

unsigned overflowSamples = 0;
const unsigned SAMPLES_PER_FRAME = 35112;
gambatte::uint_least32_t audioBuf[(35112 + 2064) * 2] = {0};
gambatte::uint_least32_t videoBuf[160*144] = {0};
std::ptrdiff_t video_pitch = 160;

void overlay_input_glyph(atg_dtv::Frame *frame, int x, int y, int glyph)
{
    int off = glyph*5;
    int offPerRow = 5*8;
    const int lineWidth = frame->m_lineWidth;
    for(int gy=0; gy<5; gy++)
    {
        uint8_t* glyph_row = &inputGlyphs[gy*offPerRow+off];
        uint8_t *row = &frame->m_rgb[(y+gy)*lineWidth];
        for(int gx=0; gx<5; gx++)
        {
            if(glyph_row[gx])
            {
                uint8_t on = glyph_row[gx] ? 0 : 255;
                const int i = (x+gx)*3;
                row[i+0] = on;
                row[i+1] = on;
                row[i+2] = on;
            }
        }
    }
}

void overlay_input(runner_t* runner, atg_dtv::Frame *frame)
{
    const int offsetPerGlyph = 7;
    const int baseOffsetX = 10;
    const int baseOffsetY = 10;
    uint8_t input = runner_get_input(runner);
    for(int i=0; i<8; i++)
    {
        if(input & (1<<i))
        {
            overlay_input_glyph(frame, baseOffsetX+offsetPerGlyph*i, baseOffsetY, i);
        }
    }
}

void encode_frame(runner_t* runner)
{
    if(runner->state.curr_work->out_encoder)
    {
        atg_dtv::Frame *newFrame = runner->state.curr_work->out_encoder->newFrame(true);
        if(newFrame == nullptr)
            return;
        if(runner->state.curr_work->out_encoder->getError() != atg_dtv::Encoder::Error::None)
        {
            printf("Error encoding: %x\n",runner->state.curr_work->out_encoder->getError());
            return;
        }
        const int lineWidth = newFrame->m_lineWidth;
        for(int y=0; y<144; y++)
        {
            uint8_t *row = &newFrame->m_rgb[y*lineWidth];
            for(int x=0; x<video_pitch; x++)
            {
                gambatte::uint_least32_t pixel = videoBuf[x+y*video_pitch];
                const int i = x*3;
                row[i+0] = pixel&0xff;
                row[i+1] = (pixel>>8)&0xff;
                row[i+2] = (pixel>>16)&0xff;
            }
        }
        overlay_input(runner, newFrame);
        runner->state.curr_work->out_encoder->submitFrame();
    }
}

int gambatte_step()
{
    while (true) {
		std::size_t emusamples = SAMPLES_PER_FRAME - overflowSamples;
		//std::ptrdiff_t res = 
        gb->runFor(videoBuf, video_pitch, audioBuf, emusamples);
        overflowSamples = 0;
        break;
        //printf("%lu samples\n",emusamples);

		overflowSamples += emusamples;

		// if(gambatte.p_->cpu.hitInterruptAddress != 0) { // go into frame
		// 	break;
		// }

		if (overflowSamples >= SAMPLES_PER_FRAME) { // new frame timing
			overflowSamples -= SAMPLES_PER_FRAME;
			break;
		}
	}
    return 0;
}

int load_state_if_needed()
{
    runner_state_t *state = &runner.state;
    if(state->curr_frame==0)
    {
        if(strlen(state->curr_work->savestate_file)>0)
        {
            printf("Loading savestate: %s\n", state->curr_work->savestate_file);
            std::string savestate_file = state->curr_work->savestate_file;
            gb->loadState(savestate_file);
            //gambatte_step(); //Step once to finish current frame
        }
        else
        {
            gb->reset(0);
        }
        if(state->curr_work->out_encoder)
        {
            gb->setSpeedupFlags(0); //If we want to encode video, reenable video writes
        }
        else
        {
            gb->setSpeedupFlags(1|4); //No sound, no video
        }
        return 1;
    }
    return 0;
}

uint8_t gambatte_get_memval(uint32_t off)
{
    uint8_t *val = 0;
    int len = 0;
    if(off>0xC000)
    {
        //Get from WRAM
        gb->getMemoryArea(2, &val, &len);
        return val[off-0xC000];
    }
    return 0;
}

void gambatte_runner_update_outdata(runner_t* runner)
{
    //Creation of workinfo checks that num_outdata < MAX
    for(size_t i=0; i<runner->state.curr_work->num_outdata; i++)
    {
        runner->state.outvals[i] = gambatte_get_memval(runner->state.curr_work->outdata[i]);
    }
}

unsigned int gambatte_runner_get_input(void* runner)
{
    return runner_get_input((runner_t*)runner);
}

int step()
{
    gambatte_runner_update_outdata(&runner);
    gambatte_step();
    return 0;
}

int main(int argc, char* argv[])
{
    if(argc<2)
    {
        printf("Usage: gambatte_runner ROM_file\n");
        return -1;
    }
    gb = new gambatte::GB();

    std::string biosPath = "dmg.bin";
    printf("Loading bios: %s\n", biosPath.c_str());
    int bios_result = gb->loadBios(biosPath, 0, 0);
    if(bios_result < 0)
    {
        printf("Couldn't load bios %s\n",biosPath.c_str());
    }

    gb->setCartBusPullUpTime(8);

    std::string romPath = argv[1];
    printf("Loading rom: %s\n", argv[1]);
    gambatte::LoadRes load_result = gb->load(romPath, 0);
    printf("Load: %s\n", to_string(load_result).c_str());
    if(load_result != gambatte::LoadRes::LOADRES_OK)
    {
        printf("Couldn't load rom %s, error: %s\n", argv[1], to_string(load_result).c_str());
        return -1;
    }
    init_global_video();
    runner_init(&runner);
    gb->setInputGetter(gambatte_runner_get_input, &runner);

    for(;;)
    {
        if(!runner.state.curr_work)
        {
            runner.state.curr_work = runner_get_workitem(&runner);
            runner.state.curr_frame = 0;
        }
        else
        {
            load_state_if_needed();
            step();
            encode_frame(&runner);
            if(runner_advance(&runner)) //runner is done with work
            {
                printf("Workitem done: %lu/%lu\n", runner.state.curr_frame, runner.state.curr_work->num_inputs);
                if(strlen(runner.state.curr_work->output_savestate)>0)
                {
                    printf("Writing savestate %s\n", runner.state.curr_work->output_savestate);
                    std::string savePath = runner.state.curr_work->output_savestate;
                    gb->saveState(nullptr,0,savePath);
                }
                runner_finish(&runner);
            }
            else
            {
                runner_write_outdata(&runner);
            }
        }
    }
    return 0;
}