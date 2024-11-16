/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <nds.h>

#include "sc_mode.h"
#include "load_bin.h"
#include "cheat_engine.h"
#include "common/tonccpy.h"

#define LCDC_BANK_D (u16*)0x06860000

#define DSIMODE_OFFSET 4
#define LANGUAGE_OFFSET 8
#define SDACCESS_OFFSET 12
#define SCFGUNLOCK_OFFSET 16
#define TWLMODE_OFFSET 20
#define TWLCLOCK_OFFSET 24
#define BOOSTVRAM_OFFSET 28
#define TWLTOUCH_OFFSET 32
#define SOUNDFREQ_OFFSET 36
#define SLEEPMODE_OFFSET 40
#define RUNCARDENGINE_OFFSET 44
#define DS_HEADER_OFFSET 48

// #define CHEAT_DATA_LOCATION ((u32*)0x06870000)

// static_assert(load_bin_size < = CHEAT_DATA_LOCATION - CHEAT_DATA_LOCATION, "aa");

// #define LCDC_BANK_C (vu16*)0x06840000

// #define CHEAT_DATA_LOCATION ((u32*)0x06850000)
#define CHEAT_CODE_END	0xCF000000
#define CHEAT_DATA_LOCATION  	((u32*)0x09000000) //use upper 16 mb of the supercard's ram

void runCheatEngine (void* cheats, int cheatLength)
{
	SC_changeMode(SC_MODE_RAM);
	nocashMessage("runLaunchEngine\n");

	irqDisable(IRQ_ALL);

	// Direct CPU access to VRAM bank D
	VRAM_D_CR = VRAM_ENABLE | VRAM_D_LCD;
	// Clear VRAM
	toncset (LCDC_BANK_D, 0x0000, 128 * 1024);
	// Load the loader/patcher into the correct address
	tonccpy (LCDC_BANK_D, load_bin, load_bin_size);
	
	toncset32 ((u8*)LCDC_BANK_D+DSIMODE_OFFSET, 0, 1);	// Not working?
	toncset32 ((u8*)LCDC_BANK_D+LANGUAGE_OFFSET, -1, 1);
	toncset32 ((u8*)LCDC_BANK_D+SDACCESS_OFFSET, 0, 1);
	toncset32 ((u8*)LCDC_BANK_D+SCFGUNLOCK_OFFSET, 0, 1);
	toncset32 ((u8*)LCDC_BANK_D+TWLMODE_OFFSET, 0, 1);
	toncset32 ((u8*)LCDC_BANK_D+TWLCLOCK_OFFSET, 0, 1);
	toncset32 ((u8*)LCDC_BANK_D+BOOSTVRAM_OFFSET, 0, 1);
	toncset32 ((u8*)LCDC_BANK_D+TWLTOUCH_OFFSET, 0, 1);
	toncset32 ((u8*)LCDC_BANK_D+SOUNDFREQ_OFFSET, 0, 1);
	toncset32 ((u8*)LCDC_BANK_D+SLEEPMODE_OFFSET, 1, 1);
	toncset32 ((u8*)LCDC_BANK_D+RUNCARDENGINE_OFFSET, 1, 1);
	toncset ((u8*)LCDC_BANK_D+DS_HEADER_OFFSET, 0, 0x1000);
	tonccpy ((u8*)LCDC_BANK_D+DS_HEADER_OFFSET, __NDSHeader, 0x200);
	u32 chipID = *(vu32*)(0x027FF800);
	tonccpy ((u8*)LCDC_BANK_D+DS_HEADER_OFFSET+0x200, &chipID, 4);
	
	// Put the codes 64KiB after the start of the loader
	tonccpy (CHEAT_DATA_LOCATION, cheats, cheatLength);
	// Mark the end of the code list
	CHEAT_DATA_LOCATION[cheatLength/sizeof(u32)] = CHEAT_CODE_END;
	CHEAT_DATA_LOCATION[cheatLength/sizeof(u32) + 1] = 0;	

	nocashMessage("irqDisable(IRQ_ALL);\n");
	irqDisable(IRQ_ALL);

	// Give the VRAM to the ARM7
	nocashMessage("Give the VRAM to the ARM7\n");
	VRAM_D_CR = VRAM_ENABLE | VRAM_D_ARM7_0x06020000;
	
	
	// Give the VRAM to the ARM7
	// VRAM_C_CR = VRAM_ENABLE | VRAM_C_ARM7_0x06000000;	
	
	// Reset into a passme loop
	
	// Reset into a passme loop
	nocashMessage("Reset into a passme loop\n");
	REG_EXMEMCNT |= ARM7_OWNS_ROM | ARM7_OWNS_CARD;
	
	*(vu32*)0x02FFFFFC = 0;

	// Return to passme loop
	*(vu32*)0x02FFFE04 = (u32)0xE59FF018; // ldr pc, 0x02FFFE24
	*(vu32*)0x02FFFE24 = (u32)0x02FFFE04;  // Set ARM9 Loop address --> resetARM9(0x02FFFE04);
	
	// Reset ARM7
	nocashMessage("resetARM7\n");
	resetARM7(0x06020000);	

	// swi soft reset
	nocashMessage("swiSoftReset\n");
	swiSoftReset();
}
