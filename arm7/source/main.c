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

#include <nds.h>
#include <nds/arm7/input.h>
#include <nds/system.h>

static tNDSHeader dummy;

#include "cheat_engine_arm7.h"
#include "card/read_card.h"


void VcountHandler() {
	inputGetAndSend();
}

void VblankHandler(void) {
}

//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------

	irqInit();
	fifoInit();

	// read User Settings from firmware
	readUserSettings();

	// Start the RTC tracking IRQ
	initClockIRQ();

	SetYtrigger(80);

	touchInit();

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT);

	// Keep the ARM7 mostly idle
	while (1) {
		if(fifoCheckValue32(FIFO_USER_01)) {
			(void)fifoGetValue32(FIFO_USER_01);
			u32 chipid;
			cardInit((tNDSHeader*)&dummy, (void*)0x02000000, &chipid);
			fifoSendValue32(FIFO_USER_02, chipid);
		}
		runCheatEngineCheck();
		swiWaitForVBlank();
	}
}


