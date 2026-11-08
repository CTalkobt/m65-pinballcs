/* sound.c — the table's sounds
 *
 * Samples (made by tools/gen_assets.py, in PBGFX) play on audio DMA
 * channels 0-2, the next free one or else the one nearest its end; channel
 * 3 loops the rolling rumble, its volume and pitch following the ball.
 * SID 2 (voices 3-5) rings the chimes and makes the beeps; SID 1 is the
 * music's. SIDs 1-2 are mirrored on 3-4: both sides.
 */
#include <audio_sid.h>
#include "hw.h"
#include "sound.h"
#include "assets.h"

extern u8 snd_ch, snd_vol, snd_addr[3], snd_rate[3], snd_mode;
extern u16 snd_len;
void snd_on(void);
void snd_off(void);
void snd_play(void);

u8 snd_music_on = 1;

static const u32 s_addr[SFX_COUNT] = {
    SND_FLIPUP, SND_FLIPDN, SND_BUMPER, SND_SLING, SND_RUBBER, SND_METAL, SND_TARGET,
    SND_DROP, SND_KICKIN, SND_KICKOUT, SND_LAUNCH, SND_DRAIN, SND_ROLLOVER, SND_SPIN, SND_KNOCKER
};
static const u16 s_len[SFX_COUNT] = {
    SND_FLIPUP_LEN, SND_FLIPDN_LEN, SND_BUMPER_LEN, SND_SLING_LEN, SND_RUBBER_LEN, SND_METAL_LEN,
    SND_TARGET_LEN, SND_DROP_LEN, SND_KICKIN_LEN, SND_KICKOUT_LEN, SND_LAUNCH_LEN, SND_DRAIN_LEN,
    SND_ROLLOVER_LEN, SND_SPIN_LEN, SND_KNOCKER_LEN
};
/* each channel: frames left of what it plays */
static u8 busy[3];

static void sample(u8 ch, u32 at, u16 len, u32 rate, u8 vol, u8 mode) {
    snd_ch = ch;
    snd_addr[0] = (u8)at;
    snd_addr[1] = (u8)(at >> 8);
    snd_addr[2] = (u8)(at >> 16);
    snd_len = len;
    snd_rate[0] = (u8)rate;
    snd_rate[1] = (u8)(rate >> 8);
    snd_rate[2] = (u8)(rate >> 16);
    snd_vol = vol;
    snd_mode = mode;
    snd_play();
}

void snd_init(void) {
    sid_init();
    sid_set_mirror(1);
    sid_set_volume(15);
    snd_on();
    REG(0xD711) = 0x80;
    sample(3, SND_ROLL, SND_ROLL_LEN, SND_RATE, 0, 0xE2);    /* the roll: looping, silent */
}

void snd_quiet(void) {
    u8 v;
    REG(0xD729 + 3 * 16) = 0;
    REG(0xD71C + 3) = 0;
    for (v = 3; v < 6; v++) sid_gate(v, 0);
}

void sfx(u8 id, u8 vol) {
    u8 ch = 0, i, least = 255;
    u16 frames = (u16)(s_len[id] / 221 + 1);          /* 11025 Hz: 220.5 a frame */
    for (i = 0; i < 3; i++)
        if (busy[i] < least) { least = busy[i]; ch = i; }
    busy[ch] = (u8)(frames > 255 ? 255 : frames);
    sample(ch, s_addr[id], s_len[id], SND_RATE, vol, 0xA2);
}

void roll_sound(u16 speed) {
    static u8 level;
    u8 want = speed < 120 ? 0 : (u8)(speed > 3000 ? 70 : 10 + (speed - 120) / 48);
    u32 rate;
    /* ease toward it: the rumble swells and fades */
    if (level < want) level = (u8)(level + 4 > want ? want : level + 4);
    else if (level > want) level = (u8)(level < want + 3 ? want : level - 3);
    REG(0xD729 + 3 * 16) = level;
    REG(0xD71C + 3) = level;
    rate = SND_RATE * 3 / 4 + (u32)(speed >> 3) * 2;
    REG(0xD724 + 3 * 16) = (u8)rate;
    REG(0xD725 + 3 * 16) = (u8)(rate >> 8);
    REG(0xD726 + 3 * 16) = (u8)(rate >> 16);
}

/* SID 2's voices: a sweep each, once a frame */
static u8 t[6];
static u16 f[6];
static s16 df[6];

static void start(u8 v, u8 wave, u8 ad, u8 sr, u16 pulse, u16 freq, s16 step, u8 frames) {
    sid_gate(v, 0);
    sid_set_adsr(v, (u8)(ad >> 4), (u8)(ad & 15), (u8)(sr >> 4), (u8)(sr & 15));
    sid_set_pulse(v, pulse);
    sid_set_freq_reg(v, freq);
    sid_set_waveform(v, wave);
    sid_gate(v, 1);
    f[v] = freq;
    df[v] = step;
    t[v] = frames;
}

/* chimes: bells of 3 pitches, rung by triangle waves that die away */
void chime(u8 which) {
    static const u16 pitch[3] = {0x8B00, 0x6A00, 0x4580};
    start((u8)(3 + which), SID_TRIANGLE, 0x0A, 0x0A, 0x800, pitch[which], 0, 2);
}

/* the effects: waveform, attack/decay, sustain/release, pulse/256,
 * frequency, its change a frame, frames */
static const u8 fx_t[][7] = {
    {SID_SAWTOOTH, 0x00, 0xF4, 0, 0x04, 0, 50},          /* tilt */
    {SID_PULSE, 0x00, 0xA4, 4, 0x0A, 0, 12},             /* warning */
    {SID_PULSE, 0x05, 0x8A, 6, 0x18, 9, 40},             /* extra ball */
    {SID_SAWTOOTH, 0x08, 0xA9, 0, 0x10, 20, 50},         /* jackpot */
    {SID_PULSE, 0x00, 0x60, 8, 0x24, 0, 3},              /* menu */
    {SID_PULSE, 0x00, 0x80, 8, 0x18, 48, 8},             /* select */
    {SID_TRIANGLE, 0x06, 0xA8, 0, 0x0C, 6, 30},          /* lock */
    {SID_PULSE, 0x00, 0x40, 3, 0x30, 0, 2},              /* countdown */
    {SID_PULSE, 0x00, 0xA9, 2, 0x20, 32, 30},            /* skill shot */
    {SID_SAWTOOTH, 0x00, 0x90, 0, 0x06, (u8)-2, 16},     /* error */
};

void sid_fx(u8 which) {
    const u8 *e = fx_t[which];
    start((u8)(which == FX_TILT ? 3 : which == FX_EXTRA || which == FX_JACKPOT || which == FX_LOCK || which == FX_SKILL ? 5 : 4),
          e[0], e[1], e[2], (u16)e[3] << 8, (u16)e[4] << 8, (s16)((s8)e[5] * 16), e[6]);
}

void snd_update(void) {
    u8 v;
    for (v = 0; v < 3; v++)
        if (busy[v]) busy[v]--;
    for (v = 3; v < 6; v++)
        if (t[v]) {
            f[v] = (u16)(f[v] + (u16)df[v]);
            if (df[v]) sid_set_freq_reg(v, f[v]);
            if (--t[v] == 0) sid_gate(v, 0);
        }
}
