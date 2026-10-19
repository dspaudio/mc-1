/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/engine.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static McEngine engine;
static McRenderSlice slice;
static McSynthJob synth;
static uint32_t random_state=0x197a203du;
static int32_t sample(void) {
    random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;
    return (int32_t)(random_state%120001u)-60000;
}
int main(void) {
    mc_init(&engine);
    for(unsigned trial=0;trial<256;trial++) {
        unsigned frames=trial%16u+1u;
        memset(&slice,0,sizeof slice);memset(&synth,0,sizeof synth);
        slice.fx.frames=frames;synth.frames=frames;
        for(unsigned p=0;p<MC_PARTS;p++) {
            engine.project.level[p]=(uint8_t)((trial+p*31u)%128u);
            engine.project.pan[p]=(uint8_t)((trial*3u+p*43u)%128u);
            for(unsigned bank=0;bank<MC_FX_BANKS;bank++) {
                unsigned mode=(trial+p+bank)%3u;
                engine.project.fx_send[p][bank]=(uint8_t)(mode==0?0:mode==1?127:(trial*7u+p*19u+bank)%128u);
            }
            for(unsigned f=0;f<frames;f++) {
                slice.part[p][f]=sample();
                if(p<3)synth.part[p][f]=sample();else synth.sample[f]=sample();
            }
        }
        for(unsigned f=0;f<frames;f++)slice.mute[f]=(uint8_t)((trial+f)%16u);
        for(unsigned bank=0;bank<MC_FX_BANKS;bank++)
            engine.project.fx_bus_type[bank]=(uint8_t)((trial+bank)%8u);
        int32_t dry[16][2]={0},wet[MC_FX_BANKS][16][2]={0};
        unsigned sends=0;
        for(unsigned p=0;p<MC_PARTS;p++)for(unsigned bank=0;bank<MC_FX_BANKS;bank++)
            if(engine.project.fx_send[p][bank])sends|=1u<<bank;
        for(unsigned f=0;f<frames;f++)for(unsigned p=0;p<MC_PARTS;p++) {
            if((slice.mute[f]>>p)&1u)continue;
            int32_t mono=slice.part[p][f];
            if(trial&1u)mono=p<3?mono+synth.part[p][f]:synth.sample[f];
            int32_t v=mono*engine.project.level[p]/127;
            int32_t stereo[2]={v*(127-engine.project.pan[p])/127,v*engine.project.pan[p]/127};
            for(unsigned c=0;c<2;c++) {
                dry[f][c]+=stereo[c];
                for(unsigned bank=0;bank<MC_FX_BANKS;bank++)
                    wet[bank][f][c]+=stereo[c]*engine.project.fx_send[p][bank]/127;
            }
        }
        render_route(&engine,&slice,trial&1u?&synth:0,0);
        assert(slice.fx.sends==sends&&slice.fx.bpm==engine.project.bpm&&!slice.fx.skip);
        assert(!memcmp(slice.fx.type,engine.project.fx_bus_type,MC_FX_BANKS));
        assert(!memcmp(slice.dry,dry,frames*sizeof dry[0]));
        for(unsigned bank=0;bank<MC_FX_BANKS;bank++)
            assert(!memcmp(slice.fx.bus[bank],wet[bank],frames*sizeof wet[bank][0]));
    }
    puts("PASS routing: independent signed-division oracle, all gains/pans, zero/unity/mixed sends, mute and worker sums");
    return 0;
}
