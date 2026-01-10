#include <apu.h>
#include <SDL2/SDL.h>

// Audio Constants
#define SAMPLE_RATE 44100
#define CYCLES_PER_SAMPLE (4194304 / SAMPLE_RATE)

typedef struct {
    bool enabled;
    u8 channel_1_regs[5]; // NR10 - NR14
    u8 channel_2_regs[5]; // NR21 - NR24
    u8 nr50, nr51, nr52;  // Control Registers

    // Oscillator State (Internal)
    int ch1_freq_timer;
    int ch1_duty_pos;
    
    int ch2_freq_timer;
    int ch2_duty_pos;

    // Sampling
    u32 sample_ticks;
    float audio_buffer[1024];
    int buffer_pos;
    SDL_AudioDeviceID device_id;

} apu_context;

static apu_context ctx;

// Duty Cycle Patterns (Binary representation of the waveform)
// 0: 12.5%, 1: 25%, 2: 50%, 3: 75%
static u8 duty_cycles[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 0}
};

void apu_init() {
    memset(&ctx, 0, sizeof(ctx));
    
    // SDL Audio Setup
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_F32SYS; // 32-bit Float Audio
    want.channels = 2;          // Stereo
    want.samples = 1024;        // Buffer size
    want.callback = NULL;       // We will use SDL_QueueAudio

    ctx.device_id = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (ctx.device_id == 0) {
        printf("Failed to open audio: %s\n", SDL_GetError());
    }
    SDL_PauseAudioDevice(ctx.device_id, 0); // Start playing
}

// Helper: Get Frequency from Registers
// Formula: 131072 / (2048 - x)
// We just return the raw 11-bit value (x) here
u16 get_freq(u8 *regs) {
    return (regs[3] | ((regs[4] & 0b111) << 8));
}

void apu_tick() {
    // 1. Tick Channel 1 (Square)
    if (ctx.ch1_freq_timer <= 0) {
        // Reload timer: (2048 - frequency) * 4
        ctx.ch1_freq_timer = (2048 - get_freq(ctx.channel_1_regs)) * 4;
        ctx.ch1_duty_pos = (ctx.ch1_duty_pos + 1) % 8;
    }
    ctx.ch1_freq_timer--;

    // 2. Tick Channel 2 (Square)
    if (ctx.ch2_freq_timer <= 0) {
        ctx.ch2_freq_timer = (2048 - get_freq(ctx.channel_2_regs)) * 4;
        ctx.ch2_duty_pos = (ctx.ch2_duty_pos + 1) % 8;
    }
    ctx.ch2_freq_timer--;

    // 3. Sample Generation
    ctx.sample_ticks++;
    if (ctx.sample_ticks >= CYCLES_PER_SAMPLE) {
        ctx.sample_ticks -= CYCLES_PER_SAMPLE;

        // Generate Sample
        float left_sample = 0;
        float right_sample = 0;

        // CH1 Logic
        if (ctx.channel_1_regs[4] & 0x80) { // If Triggered/Enabled
             // Duty Cycle Look up
             u8 duty = (ctx.channel_1_regs[1] >> 6) & 0x3;
             u8 volume = (ctx.channel_1_regs[2] >> 4) & 0xF;
             
             if (duty_cycles[duty][ctx.ch1_duty_pos]) {
                 left_sample += (float)volume / 15.0f;
                 right_sample += (float)volume / 15.0f;
             }
        }

        // CH2 Logic
        if (ctx.channel_2_regs[4] & 0x80) {
             u8 duty = (ctx.channel_2_regs[1] >> 6) & 0x3;
             u8 volume = (ctx.channel_2_regs[2] >> 4) & 0xF;
             
             if (duty_cycles[duty][ctx.ch2_duty_pos]) {
                 left_sample += (float)volume / 15.0f;
                 right_sample += (float)volume / 15.0f;
             }
        }
        
        // Push to buffer
        ctx.audio_buffer[ctx.buffer_pos++] = left_sample / 4.0f; // Scale down
        ctx.audio_buffer[ctx.buffer_pos++] = right_sample / 4.0f;

        // Send to SDL when buffer full
        if (ctx.buffer_pos >= 1024) {
            SDL_QueueAudio(ctx.device_id, ctx.audio_buffer, sizeof(ctx.audio_buffer));
            ctx.buffer_pos = 0;
        }
    }
}

u8 apu_read(u16 address) {
    if (BETWEEN(address, 0xFF10, 0xFF14)) return ctx.channel_1_regs[address - 0xFF10];
    if (BETWEEN(address, 0xFF16, 0xFF19)) return ctx.channel_2_regs[address - 0xFF15]; // Note offset
    if (address == 0xFF26) return 0xFF; // Audio ON/OFF status
    return 0; 
}

void apu_write(u16 address, u8 value) {
    // Debug print (Remove later!)
    if (address == 0xFF26) printf("APU MASTER CONTROL WRITE: %02X\n", value);

    if (BETWEEN(address, 0xFF10, 0xFF14)) ctx.channel_1_regs[address - 0xFF10] = value;
    else if (BETWEEN(address, 0xFF16, 0xFF19)) ctx.channel_2_regs[address - 0xFF15] = value;
    else if (address == 0xFF26) ctx.nr52 = value; 
}