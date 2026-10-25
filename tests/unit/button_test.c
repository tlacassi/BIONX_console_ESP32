#include <assert.h>
#include "bionx_button.h"
int main(void) {
    bx_button_t b={0};
    assert(bx_button_tick(&b,true,0)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,true,3000000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,false,3010000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,false,3040000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,true,3100000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,false,3110000)==BX_BUTTON_NONE); /* bounce */
    assert(bx_button_tick(&b,true,3120000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,true,3150000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,false,3200000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,false,3230000)==BX_BUTTON_START);
    assert(bx_button_tick(&b,false,3300000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,true,3400000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,true,3430000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,true,5429999)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,true,5430000)==BX_BUTTON_STOP);
    assert(bx_button_tick(&b,true,6000000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,false,6100000)==BX_BUTTON_NONE);
    assert(bx_button_tick(&b,false,6130000)==BX_BUTTON_NONE);
    return 0;
}
