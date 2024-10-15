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

#include "read_card.h"

#include <nds.h>
#include <nds/arm9/cache.h>
#include <nds/dma.h>
#include <nds/card.h>
#include <string.h>

#include "common/tonccpy.h"
#include "common.h"

extern u32 headerData[0x1000/sizeof(u32)];

static u32 portFlags = 0;

static u32 secureArea[CARD_SECURE_AREA_SIZE/sizeof(u32)];

static void switchToTwlBlowfish(sNDSHeaderExt* ndsHeader) {
	nocashMessage("switching to blowfish, error\n");
}


int cardInit (sNDSHeaderExt* ndsHeader, u32* chipID)
{
	nocashMessage("fake init card\n");
	*chipID = headerData[0x201/4];
	headerData[0x201/4] = 0;
	tonccpy(ndsHeader, headerData, 0x1000);
	nocashMessage(ndsHeader->gameCode);
	nocashMessage("\n");
	vu32* firmwareSecureArea = (vu32*)0x02000000;
	tonccpy(secureArea, firmwareSecureArea, CARD_SECURE_AREA_SIZE);
	portFlags = ndsHeader->cardControl13 & ~CARD_BLK_SIZE(7);
	return ERR_NONE;
}

void cardRead (u32 src, u32* dest, size_t size)
{
	sNDSHeaderExt* ndsHeader = (sNDSHeaderExt*)headerData;

	size_t readSize;

	if (src > ndsHeader->romSize) {
		switchToTwlBlowfish(ndsHeader);
	}

	if (src < CARD_SECURE_AREA_OFFSET) {
		nocashMessage("reading below secure area, abort\n");
		return;
	} else if (src < CARD_DATA_OFFSET) {
		// nocashMessage("reading from secure area\n");
		// Read data from secure area
		readSize = src + size < CARD_DATA_OFFSET ? size : CARD_DATA_OFFSET - src;
		tonccpy (dest, (u8*)secureArea + src - CARD_SECURE_AREA_OFFSET, readSize);
		src += readSize;
		dest += readSize/sizeof(*dest);
		size -= readSize;
	} else if ((ndsHeader->unitCode != 0) && (src >= ndsHeader->arm9iromOffset) && (src < ndsHeader->arm9iromOffset+CARD_SECURE_AREA_SIZE)) {
		nocashMessage("reading from secure area dsi\n");
		// Read data from secure area
		readSize = src + size < (ndsHeader->arm9iromOffset+CARD_SECURE_AREA_SIZE) ? size : (ndsHeader->arm9iromOffset+CARD_SECURE_AREA_SIZE) - src;
		tonccpy (dest, (u8*)secureArea + src - ndsHeader->arm9iromOffset, readSize);
		src += readSize;
		dest += readSize/sizeof(*dest);
		size -= readSize;
	}

	while (size > 0) {
		// nocashMessage("reading in blocks\n");
		readSize = size < CARD_DATA_BLOCK_SIZE ? size : CARD_DATA_BLOCK_SIZE;
		cardParamCommand (CARD_CMD_DATA_READ, src,
			portFlags | CARD_ACTIVATE | CARD_nRESET | CARD_BLK_SIZE(1),
			dest, readSize);
		src += readSize;
		dest += readSize/sizeof(*dest);
		size -= readSize;
	}
}

