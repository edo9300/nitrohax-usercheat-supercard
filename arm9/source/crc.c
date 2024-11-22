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

#include "crc.h"

#define CRCPOLY 0xedb88320
uint32_t crc32Partial(const void* pvoid, size_t len, uint32_t crc)
{
	const uint8_t* p = (uint8_t*)pvoid;
	while(len--)
	{
		crc^=*p++;
		for(int ii=0;ii<8;++ii) crc=(crc>>1)^((crc&1)?CRCPOLY:0);
	}
	return crc;
}

uint32_t crc32(const void* pvoid,size_t len)
{
	return crc32Partial(pvoid, len, UINT32_C(~0));
}
