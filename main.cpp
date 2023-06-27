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

unsigned overflowSamples = 0;
const unsigned SAMPLES_PER_FRAME = 35112;
gambatte::uint_least32_t audioBuf[(35112 + 2064) * 2] = {0};
gambatte::uint_least32_t videoBuf[160*144] = {0};
std::ptrdiff_t video_pitch = 160;


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
            return 1;
        }
        else
        {
            gb->reset(0);
            return 1;
        }
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
    gb->setSpeedupFlags(1|4); //No sound, no video

    std::string romPath = argv[1];
    printf("Loading rom: %s\n", argv[1]);
    gambatte::LoadRes load_result = gb->load(romPath, 0);
    printf("Load: %s\n", to_string(load_result).c_str());
    if(load_result != gambatte::LoadRes::LOADRES_OK)
    {
        printf("Couldn't load rom %s, error: %s\n", argv[1], to_string(load_result).c_str());
        return -1;
    }

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