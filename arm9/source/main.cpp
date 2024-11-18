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
#include <stdio.h>
#include <fat.h>
#include <string.h>
#include <malloc.h>
#include <list>

#include "sc_mode.h"
#include "common/tonccpy.h"
#include "cheat.h"
#include "ui.h"
#include "cheat_engine.h"
#include "crc.h"
#include "version.h"

struct ndsHeader {
sNDSHeader header;
char padding[0x200 - sizeof(sNDSHeader)];
};

const char TITLE_STRING[] = "Nitro Hax Supercard " VERSION_STRING "\nWritten by Chishm\nModified by edo9300";
const char* defaultFiles[] = {"usrcheat.dat", "/DS/NitroHax/usrcheat.dat", "/NitroHax/usrcheat.dat", "/data/NitroHax/usrcheat.dat", "/usrcheat.dat", "/_nds/usrcheat.dat", "/_nds/TWiLightMenu/extras/usrcheat.dat"};


static inline void ensure (bool condition, const char* errorMsg) {
	if (false == condition) {
		ui.showMessage (errorMsg);
		while(1) swiWaitForVBlank();
	}

	return;
}
struct SUPERCARD_RAM_DATA {
	uint8_t magicString[8];
	ndsHeader header;
	uint8_t secure_area[0x4000];
	uint32_t chipid;
};
#define CHIPID ((volatile uint32_t*)0x027FF800)
#define FIRMWARE_SECURE_AREA ((volatile uint32_t*)0x02000000)

static bool restore_secure_area_from_sdram() {
	uint8_t magicString[8] = {'S', 'C', 'S', 'F', 'W', 0, 0, 0};
	SC_changeMode(SC_MODE_RAM);
	auto& ram_data = *((SUPERCARD_RAM_DATA*)GBA_BUS);
	if(memcmp(ram_data.magicString, magicString, 8) != 0)
		return false;
	volatile uint32_t* firmwareSecureArea = FIRMWARE_SECURE_AREA;
	tonccpy(__NDSHeader, &ram_data.header, 0x200);
	tonccpy((void*)firmwareSecureArea, ram_data.secure_area, 0x4000);
	*CHIPID = ram_data.chipid;
	return true;
}

static void getHeader (u32* ndsHeader) {
	cardParamCommand (CARD_CMD_DUMMY, 0, 
		CARD_ACTIVATE | CARD_CLK_SLOW | CARD_BLK_SIZE(1) | CARD_DELAY1(0x1FFF) | CARD_DELAY2(0x3F), 
		NULL, 0);

	cardParamCommand(CARD_CMD_HEADER_READ, 0,
		CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW | CARD_BLK_SIZE(1) | CARD_DELAY1(0x1FFF) | CARD_DELAY2(0x3F),
		ndsHeader, 512);

}

//---------------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;

	u32 gameid;
	std::string filename;
	FILE* cheatFile;
	bool doFilter=false;

	ui.showMessage (UserInterface::TEXT_TITLE, TITLE_STRING);

#ifdef DEMO
	ui.demo();
	while(1);
#endif

	sysSetCardOwner (BUS_OWNER_ARM9);
	sysSetCartOwner (BUS_OWNER_ARM9);
	
	// reuse ds header parsed by the ds firmware and then altered by flashme
	auto& ndsHeader = *((struct ndsHeader*)__NDSHeader);
	uint32_t guessedCrc32[3]{};
	bool crc_matched = false;
	if(restore_secure_area_from_sdram()) {
		//arm9executeAddress and cardControl13 are altered by flashme and become unrecoverable
		//use some heuristics to try to guess them back and check for the header crc to match
		ndsHeader.header.arm9executeAddress = ((char*)ndsHeader.header.arm9destination) + 0x800;
		for(auto control : {0x00416657, 0x00586000, 0x00416017}) {
			ndsHeader.header.cardControl13 = control;
			if(crc_matched = ndsHeader.header.headerCRC16 == swiCRC16(0xFFFF, (void*)&ndsHeader, 0x15E); crc_matched) {
				break;
			}
		}
		if(crc_matched) {
			char extraBuffer[0xA0]{};
			// non dsi enhanced games produced after the dsi was released, have
			// this extra byte possibly set in their header, try to do another
			// guessing round
			// 1BFh 1    Flags (40h=RSA+TwoHMACs, 60h=RSA+ThreeHMACs)
			auto partialHeaderCRC = crc32((const char*)&ndsHeader, 0x160);
			guessedCrc32[0] = crc32Partial(extraBuffer, 0xA0, partialHeaderCRC);
			extraBuffer[0x1BF - 0x160] = 0x40;
			guessedCrc32[1] = crc32Partial(extraBuffer, 0xA0, partialHeaderCRC);
			extraBuffer[0x1BF - 0x160] = 0x60;
			guessedCrc32[2] = crc32Partial(extraBuffer, 0xA0, partialHeaderCRC);
		}
		
	}
	if(!crc_matched) {
		ui.showMessage ("Header crc didn't match\nRemove your DS Card");
		do {
			swiWaitForVBlank();
			getHeader ((uint32_t*)&ndsHeader);
		} while (*((uint32_t*)&ndsHeader) != 0xffffffff);

		ui.showMessage ("Insert Game");
		do {
			swiWaitForVBlank();
			getHeader ((uint32_t*)&ndsHeader);
		} while (*((uint32_t*)&ndsHeader) == 0xffffffff);
		sysSetCardOwner(BUS_OWNER_ARM7);
		fifoSendValue32(FIFO_USER_01, 1);
		fifoWaitValue32(FIFO_USER_02);
		*CHIPID = fifoGetValue32(FIFO_USER_02);
		sysSetCardOwner(BUS_OWNER_ARM9);
		// not really guessed, we know it's the right one
		guessedCrc32[0] = crc32((const char*)&ndsHeader, sizeof(ndsHeader));
	}

	ensure (fatInitDefault(), "FAT init failed");
	
	// Read cheat file
	for (u32 i = 0; i < sizeof(defaultFiles)/sizeof(const char*); i++) {
		cheatFile = fopen (defaultFiles[i], "rb");
		if (NULL != cheatFile) break;
		doFilter=true;
	}
	if (NULL == cheatFile) {
		filename = ui.fileBrowser ("usrcheat.dat");
		ensure (filename.size() > 0, "No file specified");
		cheatFile = fopen (filename.c_str(), "rb");
		ensure (cheatFile != NULL, "Couldn't load cheats");
	}

	ui.showMessage (UserInterface::TEXT_TITLE, TITLE_STRING);

	gameid = ((int*)&ndsHeader)[3];
	
	ui.showMessage ("Loading codes");

	CheatCodelist* codelist = new CheatCodelist();
	if(!codelist->load(cheatFile, gameid, guessedCrc32, doFilter)) {
		union {
			uint32_t bbb;
			struct {
				char a;
				char b;
				char c;
				char d;
			};
		} converter{gameid};
		ui.showMessage ("Can't read cheat list for game id: %c%c%c%c, header CRC: 0x%X\n", converter.a, converter.b, converter.c, converter.d, guessedCrc32[0]);
		while(1) swiWaitForVBlank();
	}
	// ensure (codelist->load(cheatFile, gameid, headerCRC, doFilter), "Can't read cheat list\n");
	fclose (cheatFile);
	CheatFolder *gameCodes = nullptr;
	if(guessedCrc32[1] == 0 && guessedCrc32[2] == 0) {
		gameCodes = codelist->getGame(gameid, guessedCrc32[0]);
	}	

	if (!gameCodes) {
		gameCodes = codelist;
	}
	
	if(codelist->getContents().empty()) {
		filename = ui.fileBrowser ("usrcheat.dat");
		ensure (filename.size() > 0, "No file specified");
		cheatFile = fopen (filename.c_str(), "rb");
		ensure (cheatFile != NULL, "Couldn't load cheats");

		ui.showMessage ("Loading codes");

		CheatCodelist* codelist = new CheatCodelist();
		// ensure (codelist->load(cheatFile, gameid, headerCRC, doFilter), "Can't read cheat list\n");
		if(!codelist->load(cheatFile, gameid, guessedCrc32, doFilter)) {
			union {
				uint32_t bbb;
				struct {
					char a;
					char b;
					char c;
					char d;
				};
			} converter{gameid};
			ui.showMessage ("Can't read cheat list for game id: %c%c%c%c, header CRC: 0x%X\n", converter.a, converter.b, converter.c, converter.d, guessedCrc32[0]);
			while(1) swiWaitForVBlank();
		}
		fclose (cheatFile);
		if(guessedCrc32[1] == 0 && guessedCrc32[2] == 0) {
			gameCodes = codelist->getGame(gameid, guessedCrc32[0]);
		}	

		if (!gameCodes) {
			gameCodes = codelist;
		}
	}

	ui.cheatMenu (gameCodes, gameCodes);


	auto cheatList = gameCodes->getEnabledCodeData();

	ensure(cheatList.size() <= (CHEAT_MAX_DATA_SIZE / 4), "Too many cheats selected");

	ui.showMessage (UserInterface::TEXT_TITLE, TITLE_STRING);
	ui.showMessage ("Running game");

	runCheatEngine (cheatList.data(), cheatList.size() * sizeof(u32));

	while(1) {

	}

	return 0;
}
