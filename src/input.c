#include "input.h"

static unsigned int cur, prev;
static int hold[32];
static float ax, ay;

void input_init(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

static float norm(int v)
{
    float f = (v - 128) / 127.0f;
    if (f > -0.25f && f < 0.25f) return 0;
    return f;
}

void input_update(void)
{
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(&pad, 1);
    prev = cur;
    cur = pad.Buttons;
    for (int i = 0; i < 32; i++) hold[i] = (cur & (1u << i)) ? hold[i] + 1 : 0;
    ax = norm(pad.Lx);
    ay = norm(pad.Ly);
}

int btn_down(unsigned int b) { return (cur & b) != 0; }
int btn_pressed(unsigned int b) { return (cur & b) && !(prev & b); }

int btn_repeat(unsigned int b)
{
    int bit = 0;
    while (bit < 32 && !(b & (1u << bit))) bit++;
    if (bit >= 32) return 0;
    int h = hold[bit];
    if (h == 1) return 1;
    return h > 18 && (h % 5) == 0;
}

float stick_x(void) { return ax; }
float stick_y(void) { return ay; }
