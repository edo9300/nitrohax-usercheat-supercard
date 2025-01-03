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

void CheatCode::setCodeData (const CheatWord *codeData, int codeLen)
{
	cheatData = std::vector<CheatWord>(codeLen);
	memcpy(cheatData.data(), codeData, codeLen * 4);

	// const char* codeData = data.c_str();
	// char codeinfo[30];
	// int value;
	// int codeLen = strlen (codeData);
	// int codePos = 0;
	// int readNum = 1;

	// const char ALWAYS_ON[] = "always_on";
	// const char ON[] = "on";
	// const char MASTER[] = "master";

	// if (sscanf (codeData, "%29s", codeinfo) > 0) {
	// 	if (strcmp(codeinfo, ALWAYS_ON) == 0) {
	// 		always_on = true;
	// 		enabled = true;
	// 		codePos += strlen (ALWAYS_ON);
	// 	} else if (strcmp(codeinfo, ON) == 0) {
	// 		enabled = true;
	// 		codePos += strlen (ON);
	// 	} else if (strcmp(codeinfo, MASTER) == 0) {
	// 		master = true;
	// 		codePos += strlen (MASTER);
	// 	}
	// }

	// while ((codePos < codeLen) && (readNum > 0)) {
	// 	// Move onto the next hexadecimal value
	// 	codePos += strcspn (codeData + codePos, HEX_CHARACTERS);
	// 	readNum = sscanf (codeData + codePos, "%x", &value);
	// 	if (readNum > 0) {
	// 		cheatData.push_back (value);
	// 		codePos += CODE_WORD_LEN;
	// 	} else {
	// 		readNum = sscanf (codeData + codePos, "%29s", codeinfo);
	// 		if (readNum > 0) {
	// 			codePos += strlen (codeinfo);
	// 		}
	// 	}
	// }

	// if (master && (cheatData.size() >= 2)) {
	// 	if ((*(cheatData.begin()) & 0xFF000000) == 0xCF000000) {
	// 		// Master code meant for Nitro Hax
	// 		always_on = true;
	// 		enabled = true;
	// 	} else if ((cheatData.size() >= 18) && (*(cheatData.begin()) == 0x00000000)) {
	// 		// Master code meant for the Action Replay
	// 		// Convert it for Nitro Hax
	// 		CheatWord relocDest;
	// 		std::list<CheatWord>::iterator i = cheatData.begin();
	// 		std::advance (i, 13);
	// 		relocDest = *i;
	// 		cheatData.clear();
	// 		cheatData.push_back (CHEAT_ENGINE_RELOCATE);
	// 		cheatData.push_back (relocDest);
	// 		enabled = true;
	// 	}
	// }
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
