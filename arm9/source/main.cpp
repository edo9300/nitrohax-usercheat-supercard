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

const char TITLE_STRING[] = "Nitro Hax " VERSION_STRING "\nWritten by Chishm";
const char* defaultFiles[] = {"usrcheat.dat", "/DS/NitroHax/usrcheat.dat", "/NitroHax/usrcheat.dat", "/data/NitroHax/usrcheat.dat", "/usrcheat.dat", "/_nds/usrcheat.dat", "/_nds/TWiLightMenu/extras/usrcheat.dat"};


static inline void ensure (bool condition, const char* errorMsg) {
	if (false == condition) {
		ui.showMessage (errorMsg);
		while(1) swiWaitForVBlank();
	}

	return;
}

static void restore_secure_area_from_sdram() {
	SC_changeMode(SC_MODE_RAM);
	volatile uint32_t* firmwareSecureArea = (vu32*)0x02000000;
	tonccpy(__NDSHeader, (uint8_t*)GBA_BUS, 0x200);
	tonccpy((void*)firmwareSecureArea, ((uint8_t*)GBA_BUS) + 0x200, 0x4000);
}

//---------------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;

	u32 gameid;
	uint32_t headerCRC;
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
	restore_secure_area_from_sdram();

	//arm9executeAddress and cardControl13 are altered by flashme and become unrecoverable
	//use some heuristics to try to guess them back and check for the header crc to match
	ndsHeader.header.arm9executeAddress = ((char*)ndsHeader.header.arm9destination) + 0x800;
	bool crc_matched = false;
	for(auto control : {0x00416657, 0x00586000, 0x00416017}) {
		ndsHeader.header.cardControl13 = control;
		if(crc_matched = ndsHeader.header.headerCRC16 == swiCRC16(0xFFFF, (void*)&ndsHeader, 0x15E); crc_matched) {
			break;
		}
	}
	ensure(crc_matched, "Header crc didn't match\nRestart!");
	
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
	headerCRC = crc32((const char*)&ndsHeader, sizeof(ndsHeader));

	ui.showMessage ("Loading codes");

	CheatCodelist* codelist = new CheatCodelist();
	if(!codelist->load(cheatFile, gameid, headerCRC, doFilter)) {
		union {
			uint32_t bbb;
			struct {
				char a;
				char b;
				char c;
				char d;
			};
		} converter{gameid};
		ui.showMessage ("Can't read cheat list for game id: %c%c%c%c, header CRC: 0x%X\n", converter.a, converter.b, converter.c, converter.d, headerCRC);
		while(1) swiWaitForVBlank();
	}
	// ensure (codelist->load(cheatFile, gameid, headerCRC, doFilter), "Can't read cheat list\n");
	fclose (cheatFile);
	CheatFolder *gameCodes = codelist->getGame (gameid, headerCRC);

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
		if(!codelist->load(cheatFile, gameid, headerCRC, doFilter)) {
			union {
				uint32_t bbb;
				struct {
					char a;
					char b;
					char c;
					char d;
				};
			} converter{gameid};
			ui.showMessage ("Can't read cheat list for game id: %c%c%c%c, header CRC: 0x%X\n", converter.a, converter.b, converter.c, converter.d, headerCRC);
			while(1) swiWaitForVBlank();
		}
		fclose (cheatFile);
		gameCodes = codelist->getGame (gameid, headerCRC);

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
