#include <cart.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    char filename[1024];
    u32 rom_size;
    u8 *rom_data;
    rom_header *header;

    //mbc1 related data
    bool ram_enabled;
    bool ram_banking;

    u8 *rom_bank_x;
    u8 banking_mode;

    u16 rom_bank_value; // enlarged to u16 for MBC5
    u8 rom_bank_low;
    u8 rom_bank_high;
    u8 ram_bank_value;

    u8 *ram_bank; //current selected ram bank
    u8 *ram_banks[16]; //all ram banks

    //for battery
    bool battery; //has battery
    bool need_save; //should save battery backup.

    //mbc3 related data
    bool has_timer;
    u8 ram_rtc_select;
    u8 latch_seq;

    struct {
        u8 s, m, h, dl, dh;
    } rtc;

    bool rtc_halt;
    time_t last_update;
} cart_context;

static cart_context ctx;

bool cart_need_save() {
    return ctx.need_save;
}

bool cart_mbc1() {
    return BETWEEN(ctx.header->type, 1, 3);
}

bool cart_mbc3() {
    return (ctx.header->type >= 0x0F && ctx.header->type <= 0x13);
}

bool cart_mbc5() {
    return (ctx.header->type >= 0x19 && ctx.header->type <= 0x1E);
}

bool cart_battery() {
    switch (ctx.header->type) {
        case 0x03:
        case 0x06:
        case 0x09:
        case 0x0D:
        case 0x0F:
        case 0x10:
        case 0x13:
        case 0x1B:
        case 0x1E:
        case 0x22:
            return true;
        default:
            return false;
    }
}

static void add_seconds_to_rtc(time_t seconds) {
    if (seconds <= 0) return;

    u64 total_sec = ctx.rtc.s + seconds;
    ctx.rtc.s = total_sec % 60;
    u64 carry_min = total_sec / 60;

    u64 total_min = ctx.rtc.m + carry_min;
    ctx.rtc.m = total_min % 60;
    u64 carry_hour = total_min / 60;

    u64 total_hour = ctx.rtc.h + carry_hour;
    ctx.rtc.h = total_hour % 24;
    u64 carry_day = total_hour / 24;

    u64 days = ctx.rtc.dl + ((ctx.rtc.dh & 1) << 8) + carry_day;
    if (days >= 512) {
        days -= 512;
        ctx.rtc.dh |= 0x80; // set carry
    }
    ctx.rtc.dl = days & 0xFF;
    ctx.rtc.dh = (ctx.rtc.dh & 0xC0) | ((days >> 8) & 1);
}

static void update_rtc() {
    if (ctx.rtc_halt) return;
    time_t now = time(NULL);
    add_seconds_to_rtc(difftime(now, ctx.last_update));
    ctx.last_update = now;
}

static const char *ROM_TYPES[] = {
    "ROM ONLY",
    "MBC1",
    "MBC1+RAM",
    "MBC1+RAM+BATTERY",
    "0x04 ???",
    "MBC2",
    "MBC2+BATTERY",
    "0x07 ???",
    "ROM+RAM 1",
    "ROM+RAM+BATTERY 1",
    "0x0A ???",
    "MMM01",
    "MMM01+RAM",
    "MMM01+RAM+BATTERY",
    "0x0E ???",
    "MBC3+TIMER+BATTERY",
    "MBC3+TIMER+RAM+BATTERY 2",
    "MBC3",
    "MBC3+RAM 2",
    "MBC3+RAM+BATTERY 2",
    "0x14 ???",
    "0x15 ???",
    "0x16 ???",
    "0x17 ???",
    "0x18 ???",
    "MBC5",
    "MBC5+RAM",
    "MBC5+RAM+BATTERY",
    "MBC5+RUMBLE",
    "MBC5+RUMBLE+RAM",
    "MBC5+RUMBLE+RAM+BATTERY",
    "0x1F ???",
    "MBC6",
    "0x21 ???",
    "MBC7+SENSOR+RUMBLE+RAM+BATTERY",
};

static const char *LIC_CODE[0xA5] = {
    [0x00] = "None",
    [0x01] = "Nintendo R&D1",
    [0x08] = "Capcom",
    [0x13] = "Electronic Arts",
    [0x18] = "Hudson Soft",
    [0x19] = "b-ai",
    [0x20] = "kss",
    [0x22] = "pow",
    [0x24] = "PCM Complete",
    [0x25] = "san-x",
    [0x28] = "Kemco Japan",
    [0x29] = "seta",
    [0x30] = "Viacom",
    [0x31] = "Nintendo",
    [0x32] = "Bandai",
    [0x33] = "Ocean/Acclaim",
    [0x34] = "Konami",
    [0x35] = "Hector",
    [0x37] = "Taito",
    [0x38] = "Hudson",
    [0x39] = "Banpresto",
    [0x41] = "Ubi Soft",
    [0x42] = "Atlus",
    [0x44] = "Malibu",
    [0x46] = "angel",
    [0x47] = "Bullet-Proof",
    [0x49] = "irem",
    [0x50] = "Absolute",
    [0x51] = "Acclaim",
    [0x52] = "Activision",
    [0x53] = "American sammy",
    [0x54] = "Konami",
    [0x55] = "Hi tech entertainment",
    [0x56] = "LJN",
    [0x57] = "Matchbox",
    [0x58] = "Mattel",
    [0x59] = "Milton Bradley",
    [0x60] = "Titus",
    [0x61] = "Virgin",
    [0x64] = "LucasArts",
    [0x67] = "Ocean",
    [0x69] = "Electronic Arts",
    [0x70] = "Infogrames",
    [0x71] = "Interplay",
    [0x72] = "Broderbund",
    [0x73] = "sculptured",
    [0x75] = "sci",
    [0x78] = "THQ",
    [0x79] = "Accolade",
    [0x80] = "misawa",
    [0x83] = "lozc",
    [0x86] = "Tokuma Shoten Intermedia",
    [0x87] = "Tsukuda Original",
    [0x91] = "Chunsoft",
    [0x92] = "Video system",
    [0x93] = "Ocean/Acclaim",
    [0x95] = "Varie",
    [0x96] = "Yonezawa/s’pal",
    [0x97] = "Kaneko",
    [0x99] = "Pack in soft",
    [0xA4] = "Konami (Yu-Gi-Oh!)"
};

const char *cart_lic_name() {
    if (ctx.header->new_lic_code <= 0xA4) {
        return LIC_CODE[ctx.header->lic_code];
    }

    return "UNKNOWN";
}

const char *cart_type_name() {
    if (ctx.header->type <= 0x22) {
        return ROM_TYPES[ctx.header->type];
    }

    return "UNKNOWN";
}

void cart_setup_banking() {
    size_t bank_size = 0x2000;
    int max_banks = 16;

    for (int i = 0; i < max_banks; i++) {
        ctx.ram_banks[i] = NULL;
    }

    switch (ctx.header->ram_size) {
        case 1: // 2KB, 1 bank of 0x800
            bank_size = 0x800;
            ctx.ram_banks[0] = malloc(bank_size);
            memset(ctx.ram_banks[0], 0, bank_size);
            break;
        case 2: // 8KB, 1 bank
            ctx.ram_banks[0] = malloc(bank_size);
            memset(ctx.ram_banks[0], 0, bank_size);
            break;
        case 3: // 32KB, 4 banks
            for (int i = 0; i < 4; i++) {
                ctx.ram_banks[i] = malloc(bank_size);
                memset(ctx.ram_banks[i], 0, bank_size);
            }
            break;
        case 4: // 128KB, 16 banks
            for (int i = 0; i < 16; i++) {
                ctx.ram_banks[i] = malloc(bank_size);
                memset(ctx.ram_banks[i], 0, bank_size);
            }
            break;
        case 5: // 64KB, 8 banks
            for (int i = 0; i < 8; i++) {
                ctx.ram_banks[i] = malloc(bank_size);
                memset(ctx.ram_banks[i], 0, bank_size);
            }
            break;
        default:
            break;
    }

    ctx.ram_bank = ctx.ram_banks[0];
    ctx.rom_bank_x = ctx.rom_data + 0x4000; //rom bank 1
}

bool cart_load(char *cart) {
    snprintf(ctx.filename, sizeof(ctx.filename), "%s", cart);

    FILE *fp = fopen(cart, "r");

    if (!fp) {
        printf("Failed to open: %s\n", cart);
        return false;
    }

    printf("Opened: %s\n", ctx.filename);

    fseek(fp, 0, SEEK_END);
    ctx.rom_size = ftell(fp);

    rewind(fp);

    ctx.rom_data = malloc(ctx.rom_size);
    fread(ctx.rom_data, ctx.rom_size, 1, fp);
    fclose(fp);

    ctx.header = (rom_header *)(ctx.rom_data + 0x100);
    ctx.header->title[15] = 0;
    ctx.battery = cart_battery();
    ctx.need_save = false;
    ctx.has_timer = (ctx.header->type == 0x0F || ctx.header->type == 0x10);
    ctx.ram_enabled = false;
    ctx.ram_banking = false;
    ctx.banking_mode = 0;
    ctx.rom_bank_value = 1;
    ctx.rom_bank_low = 1;
    ctx.rom_bank_high = 0;
    ctx.ram_bank_value = 0;
    ctx.ram_rtc_select = 0;
    ctx.latch_seq = 0xFF;
    ctx.rtc.s = ctx.rtc.m = ctx.rtc.h = ctx.rtc.dl = ctx.rtc.dh = 0;
    ctx.rtc_halt = true;
    ctx.last_update = time(NULL);

    printf("Cartridge Loaded:\n");
    printf("\t Title    : %s\n", ctx.header->title);
    printf("\t Type     : %2.2X (%s)\n", ctx.header->type, cart_type_name());
    printf("\t ROM Size : %d KB\n", 32 << ctx.header->rom_size);
    printf("\t RAM Size : %2.2X\n", ctx.header->ram_size);
    printf("\t LIC Code : %2.2X (%s)\n", ctx.header->lic_code, cart_lic_name());
    printf("\t ROM Vers : %2.2X\n", ctx.header->version);

    cart_setup_banking();

    u16 x = 0;
    for (u16 i=0x0134; i<=0x014C; i++) {
        x = x - ctx.rom_data[i] - 1;
    }

    printf("\t Checksum : %2.2X (%s)\n", ctx.header->checksum, (x & 0xFF) ? "PASSED" : "FAILED");

    if (ctx.battery) {
        cart_battery_load();
    }

    return true;
}

void cart_battery_load() {
    char fn[1048];
    sprintf(fn, "%s.battery", ctx.filename);
    FILE *fp = fopen(fn, "rb");

    if (!fp) {
        fprintf(stderr, "FAILED TO OPEN: %s\n", fn);
        return;
    }

    int num_banks = 0;
    size_t bank_size = 0x2000;

    switch (ctx.header->ram_size) {
        case 1:
            bank_size = 0x800;
            num_banks = 1;
            break;
        case 2:
            num_banks = 1;
            break;
        case 3:
            num_banks = 4;
            break;
        case 4:
            num_banks = 16;
            break;
        case 5:
            num_banks = 8;
            break;
        default:
            break;
    }

    for (int i = 0; i < num_banks; i++) {
        if (ctx.ram_banks[i]) {
            fread(ctx.ram_banks[i], bank_size, 1, fp);
        }
    }

    if (ctx.has_timer) {
        fread(&ctx.rtc.s, 1, 1, fp);
        fread(&ctx.rtc.m, 1, 1, fp);
        fread(&ctx.rtc.h, 1, 1, fp);
        fread(&ctx.rtc.dl, 1, 1, fp);
        fread(&ctx.rtc.dh, 1, 1, fp);
        ctx.rtc_halt = (ctx.rtc.dh & 0x40);

        time_t file_time;
        fread(&file_time, sizeof(time_t), 1, fp);

        ctx.last_update = time(NULL);
        if (!ctx.rtc_halt) {
            add_seconds_to_rtc(difftime(ctx.last_update, file_time));
        }
    }

    fclose(fp);
}

void cart_battery_save() {
    char fn[1048];
    sprintf(fn, "%s.battery", ctx.filename);
    FILE *fp = fopen(fn, "wb");

    if (!fp) {
        fprintf(stderr, "FAILED TO OPEN: %s\n", fn);
        return;
    }

    int num_banks = 0;
    size_t bank_size = 0x2000;

    switch (ctx.header->ram_size) {
        case 1:
            bank_size = 0x800;
            num_banks = 1;
            break;
        case 2:
            num_banks = 1;
            break;
        case 3:
            num_banks = 4;
            break;
        case 4:
            num_banks = 16;
            break;
        case 5:
            num_banks = 8;
            break;
        default:
            break;
    }

    for (int i = 0; i < num_banks; i++) {
        if (ctx.ram_banks[i]) {
            fwrite(ctx.ram_banks[i], bank_size, 1, fp);
        }
    }

    if (ctx.has_timer) {
        update_rtc();
        fwrite(&ctx.rtc.s, 1, 1, fp);
        fwrite(&ctx.rtc.m, 1, 1, fp);
        fwrite(&ctx.rtc.h, 1, 1, fp);
        fwrite(&ctx.rtc.dl, 1, 1, fp);
        fwrite(&ctx.rtc.dh, 1, 1, fp);
        time_t now = time(NULL);
        fwrite(&now, sizeof(time_t), 1, fp);
    }

    fclose(fp);
}

u8 cart_read(u16 address) {
    // Fixed bank
    if (address < 0x4000) {
        return ctx.rom_data[address];
    }

    if (cart_mbc1()) {
        if ((address & 0xE000) == 0xA000) {
            if (!ctx.ram_enabled) {
                return 0xFF;
            }

            if (!ctx.ram_bank) {
                return 0xFF;
            }

            return ctx.ram_bank[address - 0xA000];
        }

        return ctx.rom_bank_x[address - 0x4000];
    } else if (cart_mbc3()) {
        if (address >= 0x4000 && address < 0x8000) {
            return ctx.rom_bank_x[address - 0x4000];
        }

        if (address >= 0xA000 && address < 0xC000) {
            if (!ctx.ram_enabled) {
                return 0xFF;
            }

            if (ctx.ram_rtc_select <= 0x03) {
                if (!ctx.ram_bank) {
                    return 0xFF;
                }
                return ctx.ram_bank[address - 0xA000];
            } else if (ctx.has_timer && ctx.ram_rtc_select >= 0x08 && ctx.ram_rtc_select <= 0x0C) {
                update_rtc();
                switch (ctx.ram_rtc_select) {
                    case 0x08: return ctx.rtc.s;
                    case 0x09: return ctx.rtc.m;
                    case 0x0A: return ctx.rtc.h;
                    case 0x0B: return ctx.rtc.dl;
                    case 0x0C: return ctx.rtc.dh;
                }
            }
            return 0xFF;
        }
    } else if (cart_mbc5()) {
        if (address >= 0x4000 && address < 0x8000) {
            return ctx.rom_bank_x[address - 0x4000];
        }

        if (address >= 0xA000 && address < 0xC000) {
            if (!ctx.ram_enabled) {
                return 0xFF;
            }

            if (!ctx.ram_bank) {
                return 0xFF;
            }

            return ctx.ram_bank[address - 0xA000];
        }
    }

    // For no MBC or unhandled
    if (address < ctx.rom_size) {
        return ctx.rom_data[address];
    }
    return 0xFF;
}

void cart_write(u16 address, u8 value) {
    if (cart_mbc1()) {
        if (address < 0x2000) {
            ctx.ram_enabled = ((value & 0xF) == 0xA);
        }

        if ((address & 0xE000) == 0x2000) {
            //rom bank number
            if (value == 0) {
                value = 1;
            }

            value &= 0b11111;

            ctx.rom_bank_value = value;
            ctx.rom_bank_x = ctx.rom_data + (0x4000 * ctx.rom_bank_value);
        }

        if ((address & 0xE000) == 0x4000) {
            //ram bank number
            ctx.ram_bank_value = value & 0b11;

            if (ctx.ram_banking) {
                if (cart_need_save()) {
                    cart_battery_save();
                }

                ctx.ram_bank = ctx.ram_banks[ctx.ram_bank_value];
            }
        }

        if ((address & 0xE000) == 0x6000) {
            //banking mode select
            ctx.banking_mode = value & 1;

            ctx.ram_banking = ctx.banking_mode;

            if (ctx.ram_banking) {
                if (cart_need_save()) {
                    cart_battery_save();
                }

                ctx.ram_bank = ctx.ram_banks[ctx.ram_bank_value];
            }
        }

        if ((address & 0xE000) == 0xA000) {
            if (!ctx.ram_enabled) {
                return;
            }

            if (!ctx.ram_bank) {
                return;
            }

            ctx.ram_bank[address - 0xA000] = value;

            if (ctx.battery) {
                ctx.need_save = true;
            }
        }
    } else if (cart_mbc3()) {
        if (address < 0x2000) {
            ctx.ram_enabled = ((value & 0xF) == 0xA);
        } else if (address < 0x4000) {
            value &= 0x7F;
            if (value == 0) value = 1;
            ctx.rom_bank_value = value;
            ctx.rom_bank_x = ctx.rom_data + (0x4000 * value);
        } else if (address < 0x6000) {
            ctx.ram_rtc_select = value;
            if (ctx.ram_rtc_select <= 0x03) {
                ctx.ram_bank = ctx.ram_banks[ctx.ram_rtc_select];
            }
        } else if (address < 0x8000) {
            // Latch clock (simple ignore for now, as we update on read)
            u8 prev = ctx.latch_seq;
            ctx.latch_seq = value & 0x01;
            if (ctx.has_timer && prev == 0 && ctx.latch_seq == 1) {
                // Latch would copy current to latched, but since simplified, no-op
            }
        } else if (address >= 0xA000 && address < 0xC000) {
            if (!ctx.ram_enabled) return;
            if (ctx.ram_rtc_select <= 0x03) {
                if (!ctx.ram_bank) return;
                ctx.ram_bank[address - 0xA000] = value;
                if (ctx.battery) ctx.need_save = true;
            } else if (ctx.has_timer && ctx.ram_rtc_select >= 0x08 && ctx.ram_rtc_select <= 0x0C) {
                update_rtc();
                switch (ctx.ram_rtc_select) {
                    case 0x08:
                        ctx.rtc.s = value & 0x3F;
                        break;
                    case 0x09:
                        ctx.rtc.m = value & 0x3F;
                        break;
                    case 0x0A:
                        ctx.rtc.h = value & 0x1F;
                        break;
                    case 0x0B:
                        ctx.rtc.dl = value;
                        break;
                    case 0x0C: {
                        u8 new_dh = value & 0xC1;
                        if ((value & 0x80) == 0) {
                            new_dh &= ~0x80;
                        } else {
                            new_dh |= (ctx.rtc.dh & 0x80);
                        }
                        ctx.rtc.dh = new_dh;
                        ctx.rtc_halt = (ctx.rtc.dh & 0x40);
                        break;
                    }
                }
                ctx.last_update = time(NULL);
                if (ctx.battery) ctx.need_save = true;
            }
        }
    } else if (cart_mbc5()) {
        if (address < 0x2000) {
            ctx.ram_enabled = ((value & 0xF) == 0xA);
        } else if (address < 0x3000) {
            ctx.rom_bank_low = value;
            ctx.rom_bank_value = (ctx.rom_bank_high << 8) | ctx.rom_bank_low;
            if (ctx.rom_bank_value == 0) ctx.rom_bank_value = 1;
            ctx.rom_bank_x = ctx.rom_data + (0x4000 * ctx.rom_bank_value);
        } else if (address < 0x4000) {
            ctx.rom_bank_high = value & 0x01;
            ctx.rom_bank_value = (ctx.rom_bank_high << 8) | ctx.rom_bank_low;
            if (ctx.rom_bank_value == 0) ctx.rom_bank_value = 1;
            ctx.rom_bank_x = ctx.rom_data + (0x4000 * ctx.rom_bank_value);
        } else if (address < 0x6000) {
            ctx.ram_bank_value = value & 0x0F;
            ctx.ram_bank = ctx.ram_banks[ctx.ram_bank_value];
        } else if (address >= 0xA000 && address < 0xC000) {
            if (!ctx.ram_enabled || !ctx.ram_bank) return;
            ctx.ram_bank[address - 0xA000] = value;
            if (ctx.battery) ctx.need_save = true;
        }
    }
}