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

#include "cheat.h"
#include <stdio.h>
#include <iterator>
#include <algorithm>
#include <span>

#include "ui.h"

CheatFolder::~CheatFolder()
{
	for (std::vector<CheatBase*>::iterator curItem = contents.begin(); curItem != contents.end(); curItem++) {
		delete (*curItem);
	}
}

void CheatFolder::enableAll (bool enabled)
{
	if (allowOneOnly && enabled) {
		return;
	}
	for (std::vector<CheatBase*>::iterator curItem = contents.begin(); curItem != contents.end(); curItem++) {
		CheatCode* cheatCode = dynamic_cast<CheatCode*>(*curItem);
		if (cheatCode) {
			cheatCode->setEnabled (enabled);
		}
	}
}

void CheatFolder::enablingSubCode (void)
{
	if (allowOneOnly) {
		enableAll (false);
	}
}

std::vector<CheatWord> CheatFolder::getEnabledCodeData (void)
{
	std::vector<CheatWord> codeData;
	CheatCode* cheatCode;

	for (std::vector<CheatBase*>::iterator curItem = contents.begin(); curItem != contents.end(); curItem++) {
		std::vector<CheatWord> curCodeData = (*curItem)->getEnabledCodeData();
		cheatCode = dynamic_cast<CheatCode*>(*curItem);
		if (cheatCode && cheatCode->isMaster()) {
			codeData.insert( codeData.begin(), curCodeData.begin(), curCodeData.end());
		} else {
			codeData.insert( codeData.end(), curCodeData.begin(), curCodeData.end());
		}
	}

	return codeData;
}

std::vector<CheatWord> CheatCode::getEnabledCodeData (void)
{
	std::vector<CheatWord> codeData;
	if (enabled) {
		codeData = cheatData;
	}

	return codeData;
}

static void patchDatelHacks(std::vector<CheatWord>& cheatData)
{
	std::span<uint64_t> u64Data{reinterpret_cast<uint64_t*>(cheatData.data()), cheatData.size() / 2};
	auto nopOut = [](auto& it) {
		// replace with a NOP operation (add 0 to offset)
		*it = 0x00000000D4000000;
		++it;
	};

	static constexpr uint64_t D4_ORR_hack{ 0xE1833004023FE424 };
	static constexpr uint64_t D4_AND_hack{ 0xE0033004023FE424 };
	static constexpr uint64_t D4_ADD_hack{ 0xE0833004023FE424 };
	auto patchD4Hack = [&u64Data, &nopOut](const auto hackPattern, auto patchedInstructionFlag) {
		if(auto start = std::ranges::find(u64Data, hackPattern);
			start != u64Data.end()) {
			nopOut(start);
			auto end = std::find(start, u64Data.end(), D4_ADD_hack);
			if(auto it = std::find_if(start, end, [](auto elem) { return (elem & 0xFFFFFFFF) == 0xD4000000; }); it != end) {
				// Replace 0xD4 code with nitrohax's custom operator code
				*it |= patchedInstructionFlag;
			}
			if(end != u64Data.end()) {
				nopOut(end);
			}
		}
	};

	patchD4Hack(D4_ORR_hack, 0x1);
	patchD4Hack(D4_AND_hack, 0x2);
	patchD4Hack(D4_ADD_hack, 0x0);

	static constexpr uint64_t DB_code_fix_hack{ 0x0A000003023FE4D8 };
	if(auto start = std::ranges::find(u64Data, DB_code_fix_hack);
		start != u64Data.end()) {
		nopOut(start);
	}

	// Make the asm cheats designed for ards work with nitrohax
	// Make 0x0E handler execute the data as arm
	static constexpr uint64_t asm_hack_start{ 0x012FFF11023FE074 };
	// Restore behaviour of 0x0E handler
	static constexpr uint64_t asm_hack_end{ 0xE3520003023FE074 };
	if(auto start = std::ranges::find(u64Data, asm_hack_start);
		start != u64Data.end()) {
		nopOut(start);
		auto end = std::find(start, u64Data.end(), asm_hack_end);
		if(auto it = std::find_if(start, end, [](auto elem) { return (elem & 0xFFFFFFFF) == 0xE0000000; }); it != end) {
			// Replace 0x0E code with nitrohax's 0xC2 in arm mode (0xXXXXXXXXE0000000 - 0xXXXXXXXX1E000000 = 0xXXXXXXXXC2000000)
			*it -= 0x1E000000;
		}
		if(end != u64Data.end()) {
			nopOut(end);
		}
	}
}

void CheatCode::setCodeData (const CheatWord *codeData, int codeLen)
{
	cheatData = std::vector<CheatWord>(codeLen);
	memcpy(cheatData.data(), codeData, codeLen * 4);
	patchDatelHacks(cheatData);
}

void CheatCode::toggleEnabled (void)
{
	if (!enabled && getParent()) {
		getParent()->enablingSubCode();
	}
	if (!always_on) {
		enabled = !enabled;
	}
}


void CheatGame::setGameid (uint32_t id, uint32_t crc)
{
	gameid = id;
	headerCRC = crc;
}


CheatCodelist::~CheatCodelist(void)
{
	for (std::vector<CheatBase*>::iterator curItem = getContents().begin(); curItem != getContents().end(); curItem++) {
		delete (*curItem);
	}
}

struct CheatEntry {
	long offset;
	long size;
	uint32_t crc32;
};

auto CheatCodelist::searchCheatData(FILE* aDat, uint32_t gamecode) -> std::vector<CheatEntry>
{
	const char* KHeader="R4 CheatCode";
	char header[12];
	fread(header,12,1,aDat);
	if(strncmp(KHeader,header,12) != 0) return {};

	sDatIndex idx,nidx;

	fseek(aDat,0,SEEK_END);
	long fileSize=ftell(aDat);

	fseek(aDat,0x100,SEEK_SET);
	fread(&nidx,sizeof(nidx),1,aDat);

	std::vector<CheatEntry> res;
	while(nidx._offset != 0)
	{
		memcpy(&idx,&nidx,sizeof(idx));
		fread(&nidx,sizeof(nidx),1,aDat);
		if(gamecode==idx._gameCode)
		{
			res.emplace_back(long(idx._offset),long(((nidx._offset)?nidx._offset:fileSize)-idx._offset), idx._crc32);
		}
	}

	return res;
}

bool CheatCodelist::load(FILE* fp, uint32_t gameid, uint32_t* headerCRCs, bool filter)
{
	(void)filter;

	auto cheats = searchCheatData(fp, gameid);
	if(cheats.empty())
		return false;
	
	if(auto found = std::find_if(cheats.begin(), cheats.end(), [&](const auto& cheatEntry){
		auto crc32 = cheatEntry.crc32;
		return crc32 == headerCRCs[0] || crc32 == headerCRCs[1] || crc32 == headerCRCs[2];
	}); found != cheats.end()) {
		headerCRCs[0] = found->crc32;
		headerCRCs[1] = 0;
		headerCRCs[2] = 0;
		auto off = *found;
		std::vector{off}.swap(cheats);
	} else if(cheats.size() == 1) {
		headerCRCs[0] = cheats.front().crc32;
		headerCRCs[1] = 0;
		headerCRCs[2] = 0;
	}
	
	//we want an exact match, since in this case the header is 100% sure
	if(cheats.size() > 1 && headerCRCs[1] != 0)
		return false;
	
	for(const auto& [dataPos, dataSize, crc] : cheats) {
		CheatBase* newItem;
		fseek(fp, dataPos, SEEK_SET);		
		std::vector<uint8_t> buffer;
		buffer.resize(dataSize);

		fread(buffer.data(), dataSize, 1, fp);
		// this is so much cursed, but there's not much that can be done with it unfortunately
		char* gameTitle = (char*)buffer.data();

		auto* cheatGame = new CheatGame (this);
		cheatGame->setGameid(gameid, crc);
		CheatBase* curItem = cheatGame;
		curItem->name = gameTitle;

		uint32_t* ccode = (uint32_t*)(((uint32_t)gameTitle + strlen(gameTitle) + 4) & ~3);
		uint32_t cheatCount = *ccode;
		cheatCount &= 0x0fffffff;
		ccode += 9;

		uint32_t cc = 0;
		while(cc < cheatCount)
		{
			uint32_t folderCount = 1;
			char* folderName = NULL;
			char* folderNote = NULL;
			bool oneOnly = false, inFolder = false;
			if((*ccode >> 28) & 1)
			{
				inFolder = true;
				auto* cheatFolder = dynamic_cast<CheatFolder*>(curItem);
				if (cheatFolder) {
					newItem = new CheatFolder (cheatFolder);
					cheatFolder->addItem (newItem);
					curItem = newItem;
				}
				oneOnly = (*ccode >> 24) == 0x11;
				dynamic_cast<CheatFolder*>(curItem)->setAllowOneOnly(oneOnly);
				folderCount = *ccode & 0x00ffffff;
				folderName = (char*)((u32)ccode + 4);
				folderNote = (char*)((u32)folderName + strlen(folderName) + 1);
				curItem->name = folderName;
				curItem->note = folderNote;
				cc++;
				ccode = (uint32_t*)(((uint32_t)folderName+strlen(folderName)+1+strlen(folderNote)+1+3)&~3);
			}

			bool selectValue = true;
			for(size_t ii=0; ii < folderCount; ++ii)
			{
				auto* cheatFolder = dynamic_cast<CheatFolder*>(curItem);
				if (cheatFolder) {
					newItem = new CheatCode (cheatFolder);
					cheatFolder->addItem (newItem);
					curItem = newItem;
				}
				char* cheatName = (char*)((u32)ccode + 4);
				char* cheatNote = (char*)((u32)cheatName + strlen(cheatName) + 1);
				curItem->name = cheatName;
				curItem->note = cheatNote;
				CheatWord* cheatData = (CheatWord*)(((uint32_t)cheatNote+strlen(cheatNote)+1+3)&~3);
				uint32_t cheatDataLen = *cheatData++;

				if(cheatDataLen)
				{
					auto* cheatCode = dynamic_cast<CheatCode*>(curItem);
					cheatCode->setCodeData (cheatData, cheatDataLen);
					cheatCode->setEnabled (((*ccode & 0xff000000) ? selectValue : 0));
					if((*ccode & 0xff000000) && oneOnly)
						selectValue = false;
				}

				cc++;
				ccode=(uint32_t*)((uint32_t)ccode+(((*ccode&0x00ffffff)+1)*4));
				newItem = curItem->getParent();
				if (newItem) {
					curItem = newItem;
				}
			}

			if(inFolder) {
				newItem = curItem->getParent();
				if (newItem) {
					curItem = newItem;
				}
			}
		}
		this->addItem(curItem);
	}

	return true;
}

CheatGame* CheatCodelist::getGame (uint32_t gameid, uint32_t headerCRC)
{
	for (std::vector<CheatBase*>::iterator curItem = contents.begin(); curItem != contents.end(); curItem++) {
		CheatGame* game = dynamic_cast<CheatGame*>(*curItem);
		if (game && game->checkGameid(gameid, headerCRC)) {
			return game;
		}
	}

	return nullptr;
}
