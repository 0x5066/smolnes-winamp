#include "plugin.h"
#include <cstdio>
#include <debugapi.h>
#include <cstdlib>

/* 
A bit of clarification if you're poking around here:
addr    = CPU address
val     = what memory value is it?
write   = "hey, it's a write!"
*/
void SuperMarioBrosSpecificHacks(uint16_t addr, uint8_t val, uint8_t write) {
    char str[40];
    if (addr == 0x000E && val == 0x06){
        //sprintf(str, "player state read/write at %04X val=%02X\n", addr, val);
        //OutputDebugString(str);
        memcpy(smbtmr, smbltmr, sizeof(smbtmr));
        dead = 0;
    } else if (addr == 0x000E && val == 0x0B) {
        //sprintf(str, "player state read/write at %04X val=%02X\n", addr, val);
        //OutputDebugString(str);
        dead = 0;
    } else if (addr == 0x000E && val == 0x0A) {
        //sprintf(str, "player state read/write at %04X val=%02X\n", addr, val);
        //OutputDebugString(str);
        dead = 0;
    } else if (addr == 0x00B5 && val == 0x02) {
        //sprintf(str, "player vertical position high byte write at %04X val=%02X\n", addr, val);
        //OutputDebugString(str);
        dead = 0;
    } else if (addr == 0x000E && val == 0x08) {
        //sprintf(str, "player state read/write at %04X val=%02X\n", addr, val);
        //OutputDebugString(str);
        dead = 1;
    }

    // 07F8, hundreds, 07F9, tens, 07FA, ones
    if ( (addr >= 0x07F8 && addr <= 0x07FA) && write ) {
        smbtmr[addr - 0x07F8] = val;
        //sprintf(str, "Timer digit read/write at %04X val=%02X\n", addr, val);
        //OutputDebugString(str);
    }

    // $0770: level state (00 = title, 01 = in game, 02 = victory, 03 = game over)
    if (addr == 0x0770 && write) {
        if (val == 0x01) {
            in_game = true;
        } else {
            in_game = false;
            length_determiner = false;  // reset when leaving game
            smbtmr[0] = 0; smbtmr[1] = 0; smbtmr[2] = 0;
            smbltmr[0] = 0; smbltmr[1] = 0; smbltmr[2] = 0;
        }
    }

    // $0766/$075F: world number
    if ((addr == 0x0766 && write) || (addr == 0x075F && write)) {
      //sprintf(str, "world at %04X val=%02X\n", addr, val);
        if (val != last_world) {
            last_world = val;
            length_determiner = false;  // new world -> allow recapture
        }
    }

    // $0767/$0760: level number
    if ((addr == 0x0767 && write) || (addr == 0x0760 && write)) {
      //sprintf(str, "level at %04X val=%02X\n", addr, val);
        if (val != last_level) {
            last_level = val;
            length_determiner = false;
        }
    }

    // $000E: player state
    if (addr == 0x000E && write) {
        if (in_game && !length_determiner) {
            if (val == 0x08 || val == 0x07) {
                // copy stored time into smbltmr once
                for (int i = 0; i < 3; i++) {
                    smbltmr[i] = smbtmr[i];
                }
                length_determiner = true;
            }
        }
    }
    // Both in BCD (of course)
    // $07ED and $07EE: Mario's coins
    // $07F3 and $07F4: Luigi's coins

    // BUT we can use $075E to get the onscreen player's coin count
    // Goes up to $63 and then back to $00
    // ($0765 is the "off-screen" player's coins)
    // This is a hex value instead of BCD fuckery

    if (addr == 0x075E && write) { // detect writes on $075E
        uint8_t cash_money = 0;
      sprintf(str, "COIN at %04X val=%02X", addr, val); // tells me what address and what value is there
        char val_str[4];
        sprintf(val_str, "%02X", val);
        cash_money = strtol(val_str, nullptr, 16);
        

        OutputDebugString(str);
        
    }

}