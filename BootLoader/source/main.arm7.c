/*
 main.arm7.c
 
 By Michael Chisholm (Chishm)
 
 All resetMemory and startBinary functions are based 
 on the MultiNDS loader by Darkain.
 Original source available at:
 http://cvs.sourceforge.net/viewcvs.py/ndslib/ndslib/examples/loader/boot/main.cpp

 License:
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

#ifndef ARM7
# define ARM7
#endif
#include <nds/ndstypes.h>
#include <nds/arm7/codec.h>
#include <nds/system.h>
#include <nds/interrupts.h>
#include <nds/timers.h>
#include <nds/dma.h>
#include <nds/arm7/audio.h>
#include <nds/ipc.h>
#include <string.h>


#ifndef NULL
#define NULL 0
#endif

#define REG_GPIO_WIFI *(vu16*)0x4004C04

#include "common.h"
#include "common/tonccpy.h"
#include "read_card.h"
#include "module_params.h"
#include "hook.h"
#include "find.h"


// extern unsigned long intr_orig_return_offset;

// extern unsigned long cheat_engine_size;
// extern const u8 cheat_engine_start[]; 
extern u32 language;
extern u32 sdAccess;
extern u32 scfgUnlock;
extern u32 twlMode;
extern u32 twlClock;
extern u32 boostVram;
extern u32 twlTouch;
extern u32 soundFreq;
extern u32 sleepMode;
extern u32 runCardEngine;

extern bool arm9_runCardEngine;

bool gameSoftReset = false;

static const u32 cheatDataEndSignature[2] = {0xCF000000, 0x00000000};

// Module params
static const u32 moduleParamsSignature[2] = {0xDEC00621, 0x2106C0DE};

// Sleep input write
static const u32 sleepInputWriteEndSignature1[2]     = {0x04000136, 0x027FFFA8};
static const u32 sleepInputWriteEndSignature5[2]     = {0x04000136, 0x02FFFFA8};
static const u32 sleepInputWriteSignature[1]         = {0x13A04902};
static const u16 sleepInputWriteBeqSignatureThumb[1] = {0xD000};

static u32 chipID;

const char* getRomTid(const tNDSHeader* ndsHeader) {
	//u32 ROM_TID = *(u32*)ndsHeader->gameCode;
	static char romTid[5];
	strncpy(romTid, ndsHeader->gameCode, 4);
	romTid[4] = '\0';
	return romTid;
}

static module_params_t* moduleParams;

u32* findModuleParamsOffset(const tNDSHeader* ndsHeader) {
	//dbg_printf("findModuleParamsOffset:\n");

	u32* moduleParamsOffset = findOffset(
			(u32*)ndsHeader->arm9destination, ndsHeader->arm9binarySize,
			moduleParamsSignature, 2
		);
	return moduleParamsOffset;
}

u32* findSleepInputWriteOffset(const tNDSHeader* ndsHeader, const module_params_t* moduleParams) {
	// dbg_printf("findSleepInputWriteOffset:\n");

	u32* offset = NULL;
	u32* endOffset = findOffset(
		(u32*)ndsHeader->arm7destination, ndsHeader->arm7binarySize,
		(moduleParams->sdk_version > 0x5000000) ? sleepInputWriteEndSignature5 : sleepInputWriteEndSignature1, 2
	);
	if (endOffset) {
		offset = findOffsetBackwards(
			endOffset, 0x38,
			sleepInputWriteSignature, 1
		);
		if (!offset) {
			u32 thumbOffset = (u32)findOffsetBackwardsThumb(
				(u16*)endOffset, 0x30,
				sleepInputWriteBeqSignatureThumb, 1
			);
			if (thumbOffset) {
				thumbOffset += 2;
				offset = (u32*)thumbOffset;
			}
		}
	}
	/* if (offset) {
		dbg_printf("Sleep input write found\n");
	} else {
		dbg_printf("Sleep input write not found\n");
	}

	dbg_printf("\n"); */
	return offset;
}

static void patchSleepInputWrite(const tNDSHeader* ndsHeader, const module_params_t* moduleParams) {
	if (sleepMode) {
		return;
	}

	u32* offset = findSleepInputWriteOffset(ndsHeader, moduleParams);
	if (!offset) {
		return;
	}

	if (*offset == 0x13A04902) {
		*offset = 0xE1A00000; // nop
	} else {
		u16* offsetThumb = (u16*)offset;
		*offsetThumb = 0x46C0; // nop
	}

	/* dbg_printf("Sleep input write location : ");
	dbg_hexa((u32)offset);
	dbg_printf("\n\n"); */
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Important things
#define NDS_HEADER         0x027FFE00
#define NDS_HEADER_SDK5    0x02FFFE00 // __NDSHeader
#define NDS_HEADER_POKEMON 0x027FF000

#define DSI_HEADER         0x027FE000
#define DSI_HEADER_SDK5    0x02FFE000 // __DSiHeader

#define ENGINE_LOCATION_ARM7  	0x08000000
#define CHEAT_DATA_LOCATION  	((u32*)0x09000000) //use upper 16 mb of the supercard's ram

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Used for debugging purposes
/* Disabled for now. Re-enable to debug problems
static void errorOutput (u32 code) {
	// Wait until the ARM9 is ready
	while (arm9_stateFlag != ARM9_READY);
	// Set the error code, then tell ARM9 to display it
	arm9_errorCode = code;
	arm9_errorClearBG = true;
	arm9_stateFlag = ARM9_DISPERR;
	// Stop
	while (1);
}
*/

static void debugOutput (u32 code) {
	// Wait until the ARM9 is ready
	while (arm9_stateFlag != ARM9_READY);
	// Set the error code, then tell ARM9 to display it
	arm9_errorCode = code;
	arm9_errorClearBG = false;
	arm9_stateFlag = ARM9_DISPERR;
	// Wait for completion
	while (arm9_stateFlag != ARM9_READY);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Firmware stuff

static void my_readUserSettings(tNDSHeader* ndsHeader) {
	PERSONAL_DATA slot1;
	PERSONAL_DATA slot2;

	short slot1count, slot2count; //u8
	short slot1CRC, slot2CRC;

	u32 userSettingsBase;

	// Get settings location
	readFirmware(0x20, &userSettingsBase, 2);

	u32 slot1Address = userSettingsBase * 8;
	u32 slot2Address = userSettingsBase * 8 + 0x100;

	// Reload DS Firmware settings
	readFirmware(slot1Address, &slot1, sizeof(PERSONAL_DATA)); //readFirmware(slot1Address, personalData, 0x70);
	readFirmware(slot2Address, &slot2, sizeof(PERSONAL_DATA)); //readFirmware(slot2Address, personalData, 0x70);
	readFirmware(slot1Address + 0x70, &slot1count, 2); //readFirmware(slot1Address + 0x70, &slot1count, 1);
	readFirmware(slot2Address + 0x70, &slot2count, 2); //readFirmware(slot1Address + 0x70, &slot2count, 1);
	readFirmware(slot1Address + 0x72, &slot1CRC, 2);
	readFirmware(slot2Address + 0x72, &slot2CRC, 2);

	// Default to slot 1 user settings
	void *currentSettings = &slot1;

	short calc1CRC = swiCRC16(0xFFFF, &slot1, sizeof(PERSONAL_DATA));
	short calc2CRC = swiCRC16(0xFFFF, &slot2, sizeof(PERSONAL_DATA));

	// Bail out if neither slot is valid
	if (calc1CRC != slot1CRC && calc2CRC != slot2CRC) {
		return;
	}

	// If both slots are valid pick the most recent
	if (calc1CRC == slot1CRC && calc2CRC == slot2CRC) { 
		currentSettings = (slot2count == ((slot1count + 1) & 0x7f) ? &slot2 : &slot1); //if ((slot1count & 0x7F) == ((slot2count + 1) & 0x7F)) {
	} else {
		if (calc2CRC == slot2CRC) {
			currentSettings = &slot2;
		}
	}

	PERSONAL_DATA* personalData = (PERSONAL_DATA*)((u32)__NDSHeader - (u32)ndsHeader + (u32)PersonalData); //(u8*)((u32)ndsHeader - 0x180)

	tonccpy(PersonalData, currentSettings, sizeof(PERSONAL_DATA));

	if (language >= 0 && language <= 7) {
		// Change language
		personalData->language = language; //*(u8*)((u32)ndsHeader - 0x11C) = language;
	}

	if (personalData->language != 6 && ndsHeader->reserved1[8] == 0x80) {
		ndsHeader->reserved1[8] = 0;	// Patch iQue game to be region-free
		ndsHeader->headerCRC16 = swiCRC16(0xFFFF, ndsHeader, 0x15E);	// Fix CRC
	}
}

void memset_addrs_arm7(u32 start, u32 end)
{
	toncset((u32*)start, 0, ((int)end - (int)start));
}

/*-------------------------------------------------------------------------
arm7_resetMemory
Clears all of the NDS's RAM that is visible to the ARM7
Written by Darkain.
Modified by Chishm:
 * Added STMIA clear mem loop
--------------------------------------------------------------------------*/
void arm7_resetMemory (void)
{
	REG_IME = 0;

	for (int i=0; i<16; i++) {
		SCHANNEL_CR(i) = 0;
		SCHANNEL_TIMER(i) = 0;
		SCHANNEL_SOURCE(i) = 0;
		SCHANNEL_LENGTH(i) = 0;
	}

	REG_SOUNDCNT = 0;
	REG_SNDCAP0CNT = 0;
	REG_SNDCAP1CNT = 0;

	REG_SNDCAP0DAD = 0;
	REG_SNDCAP0LEN = 0;
	REG_SNDCAP1DAD = 0;
	REG_SNDCAP1LEN = 0;

	// Clear out ARM7 DMA channels and timers
	for (int i=0; i<4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
	}

	REG_RCNT = 0;

	// Clear out FIFO
	REG_IPC_SYNC = 0;
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
	REG_IPC_FIFO_CR = 0;

	memset_addrs_arm7(0x03800000 - 0x8000, 0x03800000 + 0x10000);

	// clear most of EXRAM - except before 0x023F0000, which has the cheat data
	memset_addrs_arm7(0x02004000, 0x023DA000);
	memset_addrs_arm7(0x023DB000, 0x02400000);

	// clear more of EXRAM, skipping the cheat data section
	// toncset ((void*)0x023F8000, 0, 0x8000);

	if(swiIsDebugger())
		memset_addrs_arm7(0x02400000, 0x02800000); // Clear the rest of EXRAM


	REG_IE = 0;
	REG_IF = ~0;
	REG_AUXIE = 0;
	REG_AUXIF = ~0;
	(*(vu32*)(0x04000000-4)) = 0;  //IRQ_HANDLER ARM7 version
	(*(vu32*)(0x04000000-8)) = ~0; //VBLANK_INTR_WAIT_FLAGS, ARM7 version
	REG_POWERCNT = 1;  //turn off power to stuffs

	// Load FW header 
	//readFirmware((u32)0x000000, (u8*)0x027FF830, 0x20);
}

// SDK 5
static bool ROMsupportsDsiMode(const tNDSHeader* ndsHeader) {
	return (ndsHeader->unitCode > 0);
}

/*void decrypt_modcrypt_area(dsi_context* ctx, u8 *buffer, unsigned int size)
{
	uint32_t len = size / 0x10;
	u8 block[0x10];

	while (len>0) {
		toncset(block, 0, 0x10);
		dsi_crypt_ctr_block(ctx, buffer, block);
		tonccpy(buffer, block, 0x10);
		buffer+=0x10;
		len--;
	}
}*/

int arm7_loadBinary (const tDSiHeader* dsiHeaderTemp) {
	u32 errorCode;
	
	// Init card
	nocashMessage("initializing card\n");
	errorCode = cardInit((sNDSHeaderExt*)dsiHeaderTemp, &chipID);
	if (errorCode) {
		return errorCode;
	}

	// Fix Pokemon games needing header data.
	tonccpy((u32*)NDS_HEADER_POKEMON, (u32*)NDS_HEADER, 0x170);

	char* romTid = (char*)NDS_HEADER_POKEMON+0xC;
	if (
		memcmp(romTid, "ADA", 3) == 0    // Diamond
		|| memcmp(romTid, "APA", 3) == 0 // Pearl
		|| memcmp(romTid, "CPU", 3) == 0 // Platinum
		|| memcmp(romTid, "IPK", 3) == 0 // HG
		|| memcmp(romTid, "IPG", 3) == 0 // SS
	) {
		// Make the Pokemon game code ADAJ.
		const char gameCodePokemon[] = { 'A', 'D', 'A', 'J' };
		tonccpy((char*)NDS_HEADER_POKEMON+0xC, gameCodePokemon, 4);
	}
	nocashMessage("reading binaries\n");

	cardRead(dsiHeaderTemp->ndshdr.arm9romOffset, (u32*)dsiHeaderTemp->ndshdr.arm9destination, dsiHeaderTemp->ndshdr.arm9binarySize);
	nocashMessage("read arm9binaries\n");
	cardRead(dsiHeaderTemp->ndshdr.arm7romOffset, (u32*)dsiHeaderTemp->ndshdr.arm7destination, dsiHeaderTemp->ndshdr.arm7binarySize);
	nocashMessage("read arm7binaries\n");

	moduleParams = (module_params_t*)findModuleParamsOffset(&dsiHeaderTemp->ndshdr);
	nocashMessage("found moduleParams\n");

	return ERR_NONE;
}

static tNDSHeader* loadHeader(tDSiHeader* dsiHeaderTemp) {
	tNDSHeader* ndsHeader = (tNDSHeader*)(isSdk5(moduleParams) ? NDS_HEADER_SDK5 : NDS_HEADER);

	*ndsHeader = dsiHeaderTemp->ndshdr;

	return ndsHeader;
}

/*-------------------------------------------------------------------------
arm7_startBinary
Jumps to the ARM7 NDS binary in sync with the display and ARM9
Written by Darkain, modified by Chishm.
--------------------------------------------------------------------------*/
void arm7_startBinary (void) {	
	// Get the ARM9 to boot
	arm9_stateFlag = ARM9_BOOTBIN;

	while (REG_VCOUNT!=191);
	while (REG_VCOUNT==191);

	// Start ARM7
	VoidFn arm7code = (VoidFn)ndsHeader->arm7executeAddress;
	arm7code();
}


/*void fixFlashcardForDSiMode(void) {
	if ((memcmp(ndsHeader->gameTitle, "PASS", 4) == 0)
	&& (memcmp(ndsHeader->gameCode, "ASME", 4) == 0))		// CycloDS Evolution
	{
		*(u16*)(0x0200197A) = 0xDF02;	// LZ77UnCompReadByCallbackWrite16bit
		*(u16*)(0x020409FA) = 0xDF02;	// LZ77UnCompReadByCallbackWrite16bit
	}
}*/

void fixDSBrowser(void) {
	toncset((char*)0x0C400000, 0xFF, 0xC0);
	toncset((u8*)0x0C4000B2, 0, 3);
	toncset((u8*)0x0C4000B5, 0x24, 3);
	*(u16*)0x0C4000BE = 0x7FFF;
	*(u16*)0x0C4000CE = 0x7FFF;

	// Opera RAM patch (ARM9)
	*(u32*)0x02003D48 = 0xC400000;
	*(u32*)0x02003D4C = 0xC400004;

	*(u32*)0x02010FF0 = 0xC400000;
	*(u32*)0x02010FF4 = 0xC4000CE;

	*(u32*)0x020112AC = 0xC400080;

	*(u32*)0x020402BC = 0xC4000C2;
	*(u32*)0x020402C0 = 0xC4000C0;
	*(u32*)0x020402CC = 0xCFFFFFE;
	*(u32*)0x020402D0 = 0xC800000;
	*(u32*)0x020402D4 = 0xC9FFFFF;
	*(u32*)0x020402D8 = 0xCBFFFFF;
	*(u32*)0x020402DC = 0xCFFFFFF;
	*(u32*)0x020402E0 = 0xD7FFFFF;	// ???
	toncset((char*)0xC800000, 0xFF, 0x800000);		// Fill fake MEP with FFs

	// Opera RAM patch (ARM7)
	*(u32*)0x0238C7BC = 0xC400000;
	*(u32*)0x0238C7C0 = 0xC4000CE;

	//*(u32*)0x0238C950 = 0xC400000;
}


static void setMemoryAddress(const tNDSHeader* ndsHeader) {
	if (ROMsupportsDsiMode(ndsHeader)) {
	//	u8* deviceListAddr = (u8*)((u8*)0x02FFE1D4);
	//	tonccpy(deviceListAddr, deviceList_bin, deviceList_bin_len);

	//	const char *ndsPath = "nand:/dsiware.nds";
	//	tonccpy(deviceListAddr+0x3C0, ndsPath, sizeof(ndsPath));

		//tonccpy((u32*)0x02FFC000, (u32*)DSI_HEADER_SDK5, 0x1000);		// Make a duplicate of DSi header (Already used by dsiHeaderTemp)
		tonccpy((u32*)0x02FFFA80, (u32*)NDS_HEADER_SDK5, 0x160);	// Make a duplicate of DS header

		*(u32*)(0x02FFA680) = 0x02FD4D80;
		*(u32*)(0x02FFA684) = 0x00000000;
		*(u32*)(0x02FFA688) = 0x00001980;

		*(u32*)(0x02FFF00C) = 0x0000007F;
		*(u32*)(0x02FFF010) = 0x550E25B8;
		*(u32*)(0x02FFF014) = 0x02FF4000;

		// Set region flag
		if (strncmp(getRomTid(ndsHeader)+3, "J", 1) == 0) {
			*(u8*)(0x02FFFD70) = 0;
		} else if (strncmp(getRomTid(ndsHeader)+3, "E", 1) == 0) {
			*(u8*)(0x02FFFD70) = 1;
		} else if (strncmp(getRomTid(ndsHeader)+3, "P", 1) == 0) {
			*(u8*)(0x02FFFD70) = 2;
		} else if (strncmp(getRomTid(ndsHeader)+3, "U", 1) == 0) {
			*(u8*)(0x02FFFD70) = 3;
		} else if (strncmp(getRomTid(ndsHeader)+3, "C", 1) == 0) {
			*(u8*)(0x02FFFD70) = 4;
		} else if (strncmp(getRomTid(ndsHeader)+3, "K", 1) == 0) {
			*(u8*)(0x02FFFD70) = 5;
		}
	}

    // Set memory values expected by loaded NDS
    // from NitroHax, thanks to Chism
	*((u32*)(isSdk5(moduleParams) ? 0x02fff800 : 0x027ff800)) = chipID;					// CurrentCardID
	*((u32*)(isSdk5(moduleParams) ? 0x02fff804 : 0x027ff804)) = chipID;					// Command10CardID
	*((u16*)(isSdk5(moduleParams) ? 0x02fff808 : 0x027ff808)) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)(isSdk5(moduleParams) ? 0x02fff80a : 0x027ff80a)) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]

	*((u16*)(isSdk5(moduleParams) ? 0x02fff850 : 0x027ff850)) = 0x5835;
	
	// Copies of above
	*((u32*)(isSdk5(moduleParams) ? 0x02fffc00 : 0x027ffc00)) = chipID;					// CurrentCardID
	*((u32*)(isSdk5(moduleParams) ? 0x02fffc04 : 0x027ffc04)) = chipID;					// Command10CardID
	*((u16*)(isSdk5(moduleParams) ? 0x02fffc08 : 0x027ffc08)) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)(isSdk5(moduleParams) ? 0x02fffc0a : 0x027ffc0a)) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]

	*((u16*)(isSdk5(moduleParams) ? 0x02fffc10 : 0x027ffc10)) = 0x5835;

	*((u16*)(isSdk5(moduleParams) ? 0x02fffc40 : 0x027ffc40)) = 0x1;						// Boot Indicator (Booted from card for SDK5) -- EXTREMELY IMPORTANT!!! Thanks to cReDiAr
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function

void arm7_main (void) {

	nocashMessage("arm7_main\n");
	// initMBK();

	int errorCode;

	// Wait for ARM9 to at least start
	nocashMessage("wait for arm9\n");
	while (arm9_stateFlag < ARM9_START);
	nocashMessage("waited for arm9\n");
	// debugOutput(123);

	//debugOutput (ERR_STS_CLR_MEM);

	// Get ARM7 to clear RAM
	arm7_resetMemory();	

	//debugOutput (ERR_STS_LOAD_BIN);

	tDSiHeader* dsiHeaderTemp = (tDSiHeader*)0x02FFC000;

	// Load the NDS file
	errorCode = arm7_loadBinary(dsiHeaderTemp);
	if (errorCode) {
		debugOutput(errorCode);
	}

	ndsHeader = loadHeader(dsiHeaderTemp);
	nocashMessage("copied nds header\n");

	bool isDSBrowser = (memcmp(ndsHeader->gameCode, "UBRP", 4) == 0);

	arm9_extendedMemory = (isDSBrowser);
	if (!arm9_extendedMemory) {
		tonccpy((u32*)0x023FF000, (u32*)(isSdk5(moduleParams) ? 0x02FFF000 : 0x027FF000), 0x1000);
	}

	my_readUserSettings(ndsHeader); // Header has to be loaded first

	patchSleepInputWrite(ndsHeader, moduleParams);

	if (memcmp(ndsHeader->gameCode, "NTR", 3) == 0		// Download Play ROMs
	 || memcmp(ndsHeader->gameCode, "ASM", 3) == 0		// Super Mario 64 DS
	 || memcmp(ndsHeader->gameCode, "AMC", 3) == 0		// Mario Kart DS
	 || memcmp(ndsHeader->gameCode, "A2D", 3) == 0		// New Super Mario Bros.
	 || memcmp(ndsHeader->gameCode, "ARZ", 3) == 0		// Rockman ZX/MegaMan ZX
	 || memcmp(ndsHeader->gameCode, "AKW", 3) == 0		// Kirby Squeak Squad/Mouse Attack
	 || memcmp(ndsHeader->gameCode, "YZX", 3) == 0		// Rockman ZX Advent/MegaMan ZX Advent
	 || memcmp(ndsHeader->gameCode, "B6Z", 3) == 0)	// Rockman Zero Collection/MegaMan Zero Collection
	{
		gameSoftReset = true;
	}

	if (memcmp(ndsHeader->gameTitle, "TOP TF/SD DS", 12) == 0) {
		runCardEngine = false;
	}
	
#define CHEAT_CODE_END	0xCF000000
	if(CHEAT_DATA_LOCATION[0] == CHEAT_CODE_END){
		runCardEngine = false;
	}

	if (runCardEngine) {

		errorCode = hookNdsRetail(ndsHeader, (const u32*)CHEAT_DATA_LOCATION, (u32*)ENGINE_LOCATION_ARM7);
		if (errorCode == ERR_NONE) {
			nocashMessage("card hook Sucessfull\n");
		} else {
			nocashMessage("error during card hook\n");
			debugOutput(errorCode);
		}
	}
	toncset ((void*)0x023F0000, 0, 0x8000);		// Clear cheat data from main memory

	nocashMessage("debugOutput (ERR_STS_START\n");
	// debugOutput (ERR_STS_START);

	arm9_boostVram = boostVram;
	arm9_scfgUnlock = false;
	arm9_runCardEngine = runCardEngine;
	
	if(isSdk5(moduleParams)){
		nocashMessage("is sdk5\n");
	}
	else{
		nocashMessage("isn't sdk5\n");
	}

	arm9_stateFlag = ARM9_SETSCFG;
	while (arm9_stateFlag != ARM9_READY);

	setMemoryAddress(ndsHeader);
	nocashMessage("arm7_startBinary\n");

	arm7_startBinary();

	while (1);
}

