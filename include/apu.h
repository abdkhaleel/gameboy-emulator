#pragma once
#include <common.h>

void apu_init();
void apu_tick();
void apu_write(u16 address, u8 value);
u8 apu_read(u16 address);