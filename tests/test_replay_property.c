#include <stdint.h>
#include <stdio.h>
#include "laststate/latch.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"replay property failed: %s:%d\n",#x,__LINE__);return 1;}}while(0)
static uint32_t rng=0x5a17c9e3u;
static uint32_t next_u32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){
    /* A duplicate inside the replay window must never be accepted twice. */
    for(unsigned round=0;round<250u;round++){
        ls_envelope_replay_t replay={0};
        uint32_t start=next_u32();
        CHECK(ls_envelope_replay_accept(&replay,start));
        CHECK(!ls_envelope_replay_accept(&replay,start));
        for(uint32_t step=1;step<96u;step++){
            uint32_t seq=start+step;
            CHECK(ls_envelope_replay_accept(&replay,seq));
            CHECK(!ls_envelope_replay_accept(&replay,seq));
            if(step>1u) CHECK(!ls_envelope_replay_accept(&replay,seq-1u));
        }
        /* Anything older than the 64-entry window is rejected. */
        CHECK(!ls_envelope_replay_accept(&replay,start+1u));
    }
    /* Explicit wrap-around: UINT32_MAX -> 0 -> 1 is forward progress. */
    ls_envelope_replay_t wrap={0};
    CHECK(ls_envelope_replay_accept(&wrap,UINT32_MAX-1u));
    CHECK(ls_envelope_replay_accept(&wrap,UINT32_MAX));
    CHECK(ls_envelope_replay_accept(&wrap,0u));
    CHECK(ls_envelope_replay_accept(&wrap,1u));
    CHECK(!ls_envelope_replay_accept(&wrap,UINT32_MAX));
    return 0;
}
