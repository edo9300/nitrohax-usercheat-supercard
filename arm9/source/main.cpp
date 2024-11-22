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
#include <map>

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

static const std::map<uint16_t, uint32_t> dsiEnhancedHeaders{
	{0xC997, 0xCDF54088}, // 1000 Cooking Recipes from Elle a Table (Europe) (En,Fr,De,Es,It)
	{0x7F37, 0x809BB062}, // 2 in 1 - Music for Kids + Englisch Macht Spass - Eine Reise nach London! (Europe) (En,De)
	{0x6184, 0x6E6E5B2A}, // Alice in Wonderland (Europe) (En,Fr,De,Es,It,Nl)
	{0x72DC, 0x1C49016F}, // Alice in Wonderland (USA) (En,Fr,Es)
	{0xD43F, 0x16BEB3A6}, // An M. Night Shyamalan Film - The Last Airbender (Europe) (En,Fr,De,Es,It)
	{0x1450, 0x2F8D6EA5}, // An M. Night Shyamalan Film - The Last Airbender (USA) (En,Fr)
	{0x5E6A, 0x3685D60E}, // Animal Life - Africa (Europe) (En,Fr,De,Es,It)
	{0x2A5B, 0xE3BC4C20}, // Animal Life - Australia (Europe) (En,Fr,De,Es,It)
	{0xF1D6, 0x7F79ACB0}, // Animal Life - Dinosaurs (Europe) (En,Fr,De,Es,It)
	{0x9C30, 0x49866939}, // Animal Life - Eurasia (Europe) (En,Fr,De,Es,It)
	{0x993F, 0x750B0767}, // Animal Life - North America (Europe) (En,Fr,De,Es,It)
	{0x9FAE, 0xF047F13F}, // Anone DS (Japan) (NDSi Enhanced) [b].zip.dat
	{0x595D, 0xCAB6DD6E}, // Are You Smarter than a 5th Grader - Back to School (USA)
	{0xD684, 0xD38C753F}, // Art Academy (Europe) (En,Fr,De,Es,It)
	{0xE860, 0x713BB2A7}, // Art Academy (USA)
	{0x0F7A, 0x4EFEAA9D}, // Art of Murder - FBI Top Secret (Europe) (Fr,De)
	{0x5D23, 0x385435AD}, // Assassin's Creed II - Discovery (Europe) (En,Fr,De,Es,It,Nl,Sv,No,Da)
	{0x0523, 0xE41A0C63}, // Assassin's Creed II - Discovery (USA) (En,Fr,Es)
	{0xF9D8, 0x695AD3FD}, // Baby Fashion Star (Europe) (En,Fr,De,Es,It,Nl,Sv,No,Da)
	{0x5E39, 0xAEA23395}, // Barbie - Jet, Set & Style! (Europe) (De,Es,It) (VMZP)
	{0x49BE, 0x0E0E0584}, // Barbie - Jet, Set & Style! (Europe) (De,Es,It) (VMZX)
	{0xF836, 0xD61374D4}, // Barbie - Jet, Set & Style! (Europe) (En,Fr,Nl)
	{0xF4B6, 0x4C7D9662}, // Barbie - Jet, Set & Style! (USA) (En,Fr)
	{0xCCE7, 0x4985DDE4}, // Bejeweled Twist (Australia) (En,Fr,De,Es,It,Nl)
	{0x60FF, 0xB6DFA36C}, // Bejeweled Twist (Europe) (En,Fr,De,Es,It,Nl) (VBTP)
	{0x409B, 0xA46C6368}, // Bejeweled Twist (Europe) (En,Fr,De,Es,It,Nl) (VBTX)
	{0xF0E0, 0x28179A10}, // Bejeweled Twist (USA)
	{0x4FA3, 0x8AB8A062}, // Biggest Loser USA, The (Europe)
	{0x733F, 0xF614FC76}, // Biggest Loser, The (USA)
	{0x1A83, 0x1DD8BDD8}, // Boshi Techou DS with 'Akachan Massage' (Japan)
	{0x02D3, 0x3A8B3C90}, // Boshi Techou DS with 'Akachan Massage' (Japan) (Rev 1)
	{0x36BD, 0x1CAF4D9C}, // Brainstorm Series - Treasure Chase (USA) (En,Fr,De,Es,It)
	{0x7CC3, 0x296F7D24}, // Bratz - Fashion Boutique (USA)
	{0x813D, 0x1F756EED}, // Bravissi-Mots (France)
	{0xDB75, 0x78149D85}, // CSI - Crime Scene Investigation - Unsolved! (Europe) (En,Fr,De,Es,It)
	{0x8504, 0x34B31464}, // CSI - Crime Scene Investigation - Unsolved! (USA) (En,Fr,Es)
	{0x5747, 0x5FF81C9E}, // Camp Rock - The Final Jam (Europe) (En,Fr,De,Es,It,Nl)
	{0x04CA, 0x9BA781DE}, // Camp Rock - The Final Jam (Europe) (En,Sv,No,Da)
	{0x48A6, 0x4AEA7855}, // Camp Rock - The Final Jam (USA) (En,Fr,Es)
	{0x2D8C, 0x1E03665A}, // Cars 2 (Europe) (En,Es)
	{0x6A30, 0x8D6149B1}, // Cars 2 (Europe) (En,Sv,Fi)
	{0x7B9A, 0x54C3E11F}, // Cars 2 (Europe) (Fr,De,It)
	{0x6F06, 0x22D06968}, // Cars 2 (Europe) (Fr,Nl)
	{0xE6EE, 0x7D63FDDA}, // Cars 2 (Europe) (Sv,No,Da)
	{0x7D05, 0x0B7AA8F7}, // Cars 2 (Japan)
	{0x9EDF, 0x26649D39}, // Cars 2 (USA) (En,Fr,Es)
	{0xC447, 0xEDECF43D}, // Chara-Chenko (Japan)
	{0x4950, 0x51DC3E69}, // Cheer We Go (USA)
	{0x0CFD, 0x06C5DF2F}, // Chronicles of Mystery - The Secret Tree of Life (Europe) (En,Fr,De,Es,It,Nl)
	{0x9512, 0x14C18767}, // Chronicles of Mystery - The Secret Tree of Life (USA) (En,Fr,Es)
	{0xF0F7, 0x7A10EA13}, // Classic Word Games (Europe)
	{0xBA41, 0x2603D8B9}, // Classic Word Games (USA)
	{0x0E0A, 0xA5E3FD2D}, // Cosmetick Paradise - Kirei no Mahou (Japan)
	{0x2D2D, 0x3FE07104}, // Cosmetick Paradise - Princess Life (Japan)
	{0xDCF1, 0x3C67DF7C}, // Crime Lab - Body of Evidence (Europe) (En,Es,It,Nl)
	{0xC50D, 0x6CD93DBC}, // Crime Lab - Body of Evidence (Europe)
	{0x877C, 0x9DB435EB}, // Crime Lab - Body of Evidence (USA) (En,Fr,Es)
	{0x5805, 0xE6B17599}, // DS-Pico Series - Sanrio Puroland - Waku Waku Okaimono - Suteki na Oheya o Tsukuri Masho (Japan)
	{0x5FFB, 0x3BBFF863}, // Dance! - It's Your Stage (Europe) (En,De)
	{0x7883, 0x3D0C5FEE}, // Daniel X - The Ultimate Power (USA) (En,Fr,De,Es,It)
	{0x6ED0, 0x76782A96}, // Dolphin Island - Underwater Adventures (Europe) (En,Fr,De,Es,It,Nl)
	{0xE0A0, 0x02B12EA6}, // Egokoro Kyoushitsu DS (Japan)
	{0x8A20, 0xEB84694D}, // Eigo de Asobou - Mummy Talk DS (Japan)
	{0x6462, 0x70A5583A}, // Elminage II - Sousei no Megami to Unmei no Daichi - DS Remix (Japan) (NDSi Enhanced) [b].zip.dat
	{0x8277, 0xDA409901}, // Emily the Strange - Strangerous (Europe) (En,De)
	{0xCE68, 0x0B365BC3}, // Emily the Strange - Strangerous (Europe) (En,Fr) (VESV)
	{0xA1F4, 0xD3928807}, // Emily the Strange - Strangerous (Europe) (En,Fr) (VESX)
	{0x192E, 0x0EE1D690}, // Emily the Strange - Strangerous (USA)
	{0xA611, 0xBDED439C}, // FIFA 11 (Europe) (En,Fr,De,Es,It)
	{0xFB2E, 0x6CF9E07D}, // FIFA Soccer 11 (USA) (En,Fr,De,Es,It)
	{0xC6D9, 0xB65E73F1}, // Fancy Nancy - Tea Party Time! (USA)
	{0x760B, 0x1C8D9537}, // Fire Emblem - Shin Monshou no Nazo - Hikari to Kage no Eiyuu (Japan) (Rev 1) (NDSi Enhanced) [b].zip.dat
	{0x0FB2, 0xBDA37A20}, // Fluch der Osterinsel, Der (Germany)
	{0x1072, 0xAC26799E}, // Fossil Fighters Champions (USA)
	{0xBA2B, 0x059CD600}, // Fuyu no Sonata DS (Japan)
	{0xC787, 0xD8E8C56F}, // Germany's Next Topmodel - Das Offizielle Spiel zur Staffel 2010 (Germany)
	{0xD771, 0x5FF9F24E}, // Germany's Next Topmodel - Das offizielle Spiel zur Staffel 2011! (Germany)
	{0x9168, 0x1C107637}, // Girls Life - Fashion Addict (Europe) (En,Fr,De,Es,It,Nl)
	{0x4CD9, 0x20CED2C6}, // Girls Life - Jewellery Style (Europe) (En,Fr,De,Es,It,Nl)
	{0xC447, 0xDBF8437F}, // Girls Life - Makeover (Europe) (En,Fr,De,Es,It,Nl)
	{0x77B7, 0xEF17AD57}, // Gokujou!! Mecha Mote Iinchou - MM My Best Friend! (Japan)
	{0x1974, 0xFCD87A76}, // Grease - The Official Video Game (Europe)
	{0x173E, 0x5489C7B6}, // Grease - The Official Video Game (USA)
	{0x1F34, 0xAA959058}, // Home Designer - Perfekt Gestylte Zimmer (Germany)
	{0xCDE3, 0xE764DF02}, // Hot Wheels - Track Attack (Brazil) (En,Es,Pt)
	{0x3120, 0x17216EA9}, // Hot Wheels - Track Attack (Europe) (En,Fr,De,Es,It,Nl)
	{0xF155, 0xE9497B67}, // Hot Wheels - Track Attack (USA) (En,Fr)
	{0xB05D, 0xC991DFE4}, // Idolm@ster, The - Dearly Stars (Japan)
	{0xFC79, 0x6AC8DB18}, // Imagine - Animal Doctor Care Center (USA) (En,Fr,Es)
	{0x5548, 0x6EFF3AA0}, // Imagine - Artist (Europe) (En,Fr,De,Es,It,Nl)
	{0xB544, 0x9EE7BD3D}, // Imagine - Artist (USA) (En,Fr,Es)
	{0x2B2E, 0x2D0C7782}, // Imagine - Babyz Fashion (USA) (En,Fr,Es)
	{0x905B, 0x69335BDB}, // Imagine - Champion Rider (Europe) (En,Fr,De,Es,It)
	{0x21E1, 0x067C6319}, // Imagine - Dream Resort (Europe) (En,Fr,De,Es,It)
	{0x9780, 0x3A961672}, // Imagine - Fashion Designer - World Tour (USA) (En,Fr,Es)
	{0xFE98, 0x0AEE9E30}, // Imagine - Fashion Paradise (Europe) (En,Fr,De,Es,It,Pt)
	{0xC0D0, 0xDAA0ED9F}, // Imagine - Fashion Stylist (USA) (En,Fr,Es)
	{0x71D7, 0xB0538486}, // Imagine - Gymnast (USA) (En,Fr,Es)
	{0xA45F, 0x65111A7C}, // Imagine - Journalist (Europe) (En,Fr,De,Es,It,Nl,Sv,No,Da)
	{0xAD58, 0x18FEB0CE}, // Imagine - Reporter (USA) (En,Fr,Es)
	{0xC246, 0xB2B23846}, // Imagine - Rescue Vet (Europe) (En,Fr,De,Es,It)
	{0x5B6B, 0x871FD7B9}, // Imagine - Resort Owner (USA) (En,Fr,Es)
	{0xD2E9, 0x146CCA57}, // Jam Sessions 2 (USA) (En,Fr,Es)
	{0x3831, 0x0C9A0510}, // James Cameron's Avatar - The Game (Europe) (En,Fr,De,Es,It,Nl)
	{0xE4FC, 0x6AC21BAB}, // James Cameron's Avatar - The Game (Japan)
	{0x8F89, 0x595AA10D}, // James Cameron's Avatar - The Game (USA) (En,Fr,Es)
	{0x4786, 0xC23D057E}, // James Patterson Women's Murder Club - Games of Passion (Europe) (En,Fr,De,Es,It)
	{0x0EC5, 0x041538BE}, // James Patterson Women's Murder Club - Games of Passion (USA) (En,Fr)
	{0x2ED9, 0x2CF67A21}, // Jigapix - Love is (Europe) (En,Fr,De,Es,It,Nl)
	{0xC016, 0xB46F1D1A}, // Jigapix - Pets (Europe) (En,Fr,De,Es,It,Nl)
	{0xE572, 0x120DEC02}, // Jigapix - Pets (USA) (En,Fr,Es)
	{0x97E8, 0x53170A5E}, // Jigapix - Wild World (Europe) (En,Fr,De,Es,It,Nl)
	{0xB6FD, 0x59832336}, // Jigapix - Wild World (USA) (En,Fr,Es)
	{0xF577, 0x8D47FA67}, // Jigapix - Wonderful World (Europe) (En,Fr,De,Es,It,Nl)
	{0x14B9, 0x4A1E311C}, // Jigapix - Wonderful World (USA) (En,Fr,Es)
	{0x07DB, 0xB7878110}, // Just Sing (USA) (En,Fr)
	{0x6AEB, 0xA50B769E}, // Just Sing! (Europe) (En,Fr,De,Nl)
	{0x981D, 0x3F7D9375}, // Just Sing! (Europe) (En,Fr,De,Nl) (Rev 1)
	{0x93C5, 0x77623445}, // Just Sing! - Vol. 2 (Europe) (En,Fr,De) (VJVP)
	{0x7DE7, 0xC4E08389}, // Just Sing! - Vol. 2 (Europe) (En,Fr,De) (VJVV)
	{0xCA87, 0x16ECBBB2}, // Just Sing! - Vol. 3 (Europe) (En,Fr,De)
	{0x646F, 0xACDAB9D8}, // Katekyoo Hitman Reborn! DS - Flame Rumble XX - Chou Kessen! Real 6 Chouka (Japan)
	{0x5C12, 0x2A19561C}, // Katekyoo Hitman Reborn! DS - Ore ga Boss! - Saikyou Family Taisen (Japan)
	{0x6063, 0x4689EFB6}, // Kids Learn Music - A+ Edition (USA)
	{0x9AFB, 0xA6A1F434}, // Kirakira Rhythm Collection (Japan)
	{0x5221, 0x2A72E3FE}, // Know How 2 (Europe) (En,Fr,De,Es,It)
	{0x8527, 0x42A08868}, // Korg DS-10+ Synthesizer (Japan) (NDSi Enhanced) [b].zip.dat
	{0x3974, 0x0A9996E5}, // Korg DS-10+ Synthesizer (USA)
	{0x2906, 0x94AC1C42}, // Korg DS-10+ Synthesizer Limited Edition (Japan) (En,Ja) (NDSi Enhanced) [b].zip.dat
	{0xCAC2, 0x7519A50B}, // Kung Fu Panda 2 (Europe) (En,Fr,De,Es,It,Nl)
	{0xEFA7, 0xB847C331}, // Kung Fu Panda 2 (USA) (En,Fr) (VKUE)
	{0x8B8B, 0xBC64F7C1}, // Kung Fu Panda 2 (USA) (En,Fr) (VKUY)
	{0xF655, 0x40C2221C}, // Licca-chan DS - Motto! Onnanoko Lesson - Oshare, Oshigoto, Otetsudai Daisuki! (Japan)
	{0xA6F4, 0x69ADF69D}, // Lost Identities (Europe) (En,De) (VLIP)
	{0xA827, 0x278E51A5}, // Lost Identities (Europe) (En,De) (VLIV)
	{0xF236, 0xED0FA9D1}, // Lovely Lisa and Friends (USA)
	{0x6A48, 0xB70784FB}, // Make-up and Style (Germany)
	{0xF3C6, 0x3134BA3B}, // Mario vs. Donkey Kong - Mini-Land Mayhem! (Europe) (En,Fr,De,Es,It)
	{0x1B5E, 0xDF6D95F1}, // Mario vs. Donkey Kong - Mini-Land Mayhem! (Europe, Australia) (Demo) (Kiosk)
	{0x689B, 0x63D92FB9}, // Mario vs. Donkey Kong - Mini-Land Mayhem! (USA) (Demo) (Kiosk)
	{0xCC32, 0xBDEC13A0}, // Mario vs. Donkey Kong - Mini-Land Mayhem! (USA) (En,Fr,Es) (Rev 1)
	{0x2447, 0xDE1D32BC}, // Mario vs. Donkey Kong - Mini-Land Mayhem! (USA) (En,Fr,Es) (Rev 2)
	{0x34AC, 0x85CB3BF1}, // Mario vs. Donkey Kong - Totsugeki! Mini-Land (Japan)
	{0x4166, 0x0A633511}, // Mein Koch-Coach - Gesund und Lecker Kochen (Germany)
	{0x4F05, 0x3FAD6AF1}, // Meine Tierarztpraxis - SOS am Ozean (Europe) (En,De)
	{0x0297, 0x9FC22C82}, // Mi Experto en Cocina - Comida Saludable (Spain)
	{0x69DE, 0x65C99474}, // Mio Coach di Cucina, Il - Prepara Cibi Sani e Gustosi (Italy)
	{0xA008, 0x966D4421}, // Mon Coach Personnel - Mes Recettes Plaisir et Ligne (France)
	{0x99DA, 0x0AFA1FCA}, // Monster High - Ghoul Spirit (Europe) (En,Fr,De,Es,It,Nl) (Little Orbit)
	{0x09F0, 0xCE435F4E}, // Monster High - Ghoul Spirit (Europe) (En,Fr,De,Es,It,Nl) (THQ)
	{0xB254, 0x7FE44690}, // Monster High - Ghoul Spirit (USA) (En,Fr)
	{0xF75B, 0x147EBEDC}, // Moxie Girlz (USA)
	{0x0F1E, 0x0E572F81}, // Murder on the Titanic (Europe) (En,Fr,De,Nl)
	{0xF37E, 0x998C34C5}, // Music for Kids (Europe) (En,De)
	{0x1B50, 0x8A8A3994}, // My Cooking Coach - Prepare Healthy Recipes (Europe)
	{0x5CF7, 0x330B3992}, // My Healthy Cooking Coach - Easy Way to Cook Healthy (USA)
	{0x0149, 0xEEB4E1FA}, // New Carnival - Funfair Games (Europe) (En,Fr,De,Es,It,Nl)
	{0xFF65, 0xE4479A43}, // New Carnival Games (USA) (En,Fr,Es)
	{0xFB78, 0xC9ED731E}, // Paws & Claws - Marine Rescue (USA)
	{0x259B, 0x6DB4527A}, // Paws & Claws - Pampered Pets 2 (USA)
	{0xE3AB, 0x861E82AE}, // Penguins of Madagascar, The (Europe) (De,Nl)
	{0xDA09, 0x1250DF47}, // Penguins of Madagascar, The (Europe) (En,Fr)
	{0xE775, 0x7130EE00}, // Penguins of Madagascar, The (Europe) (Es,It)
	{0x71C1, 0x256858F5}, // Penguins of Madagascar, The (USA) (En,Fr)
	{0xDDA6, 0xCFF6382B}, // Penguins of Madagascar, The - Dr. Blowhole Returns Again! (Europe) (De,Nl)
	{0x5494, 0x11E8ED19}, // Penguins of Madagascar, The - Dr. Blowhole Returns Again! (Europe) (En,Fr)
	{0x03B9, 0x52353CE4}, // Penguins of Madagascar, The - Dr. Blowhole Returns Again! (Europe) (Es,It)
	{0x32FE, 0x26110227}, // Penguins of Madagascar, The - Dr. Blowhole Returns Again! (USA) (En,Fr)
	{0xB978, 0xB4F2BE7E}, // Petz - Dolphinz Encounter (USA) (En,Fr,Es)
	{0xD7F0, 0x5D0C1D95}, // Petz - Fantasy (Europe) (En,Fr,De,Es,It,Nl,Sv,No,Da)
	{0x8C50, 0xB196D789}, // Petz - Horsez Family (USA) (En,Fr,Es)
	{0x9AED, 0x4973172D}, // Petz Fantasy - Moonlight Magic (USA) (En,Fr,Es)
	{0xDA34, 0x52F180B3}, // Petz Fantasy - Sunshine Magic (USA) (En,Fr,Es)
	{0xD2C7, 0x8C4DAF32}, // Phineas and Ferb - 2 Disney Games (Europe) (En,Fr,De,Es,It,Nl)
	{0x2C05, 0x99CCE099}, // Phineas and Ferb - Ride Again (Europe) (En,De,Es,It,Nl)
	{0xE0DD, 0x82FBDC95}, // Phineas and Ferb - Ride Again (USA) (En,Es)
	{0xB6D0, 0x59842849}, // Pictionary (Europe) (En,Fr,De,Es,It,Nl)
	{0xFD62, 0xC1DEDBC4}, // Pictionary (USA) (En,Fr,Es)
	{0x7D1E, 0xB39DB08A}, // Pocket Monsters - Black (Japan)
	{0xFAC5, 0x2D0CB8AE}, // Pocket Monsters - Black (Korea)
	{0xB2E8, 0x5A023804}, // Pocket Monsters - Black 2 (Japan) (Rev 1)
	{0x8ED6, 0x1B411333}, // Pocket Monsters - Black 2 (Korea) (NDSi Enhanced) [b].zip.dat
	{0xF191, 0x4F2FDEC2}, // Pocket Monsters - White (Japan)
	{0x3D1C, 0x0F5D67C8}, // Pocket Monsters - White (Korea)
	{0xC7DC, 0x0146C7A8}, // Pocket Monsters - White 2 (Japan) (Rev 1)
	{0x1A52, 0xA64DEB30}, // Pocket Monsters - White 2 (Korea) (NDSi Enhanced) [b].zip.dat
	{0x8485, 0x106820A5}, // Pokemon - Black Version (USA, Europe)
	{0x7680, 0x8E4C1CD6}, // Pokemon - Black Version 2 (USA, Europe)
	{0xC780, 0x11F41913}, // Pokemon - Edicion Blanca (Spain)
	{0xB3F6, 0x5265940B}, // Pokemon - Edicion Blanca 2 (Spain)
	{0x21EB, 0x17487AFA}, // Pokemon - Edicion Negra (Spain)
	{0x46C8, 0xDF767B2D}, // Pokemon - Edicion Negra 2 (Spain)
	{0x2C44, 0x8B409E11}, // Pokemon - Schwarze Edition (Germany)
	{0xDCD0, 0xC4697F15}, // Pokemon - Schwarze Edition 2 (Germany)
	{0xBC1D, 0x031EF208}, // Pokemon - Version Blanche (France)
	{0x234D, 0x6C54D079}, // Pokemon - Version Blanche 2 (France)
	{0x09E4, 0xBA565122}, // Pokemon - Version Noire (France)
	{0x5A30, 0x66465E3D}, // Pokemon - Version Noire 2 (France)
	{0x907B, 0xCC7CAE76}, // Pokemon - Versione Bianca (Italy)
	{0x5985, 0x4CD5F7C6}, // Pokemon - Versione Bianca 2 (Italy)
	{0x5BFF, 0x639C2678}, // Pokemon - Versione Nera (Italy)
	{0x95E8, 0x79F2AC9F}, // Pokemon - Versione Nera 2 (Italy)
	{0xC1DB, 0x12020314}, // Pokemon - Weisse Edition (Germany)
	{0xE2DB, 0x54853727}, // Pokemon - Weisse Edition 2 (Germany)
	{0xF6BF, 0x0F0875FE}, // Pokemon - White Version (USA, Europe)
	{0xD75C, 0x012AF769}, // Pokemon - White Version 2 (USA, Europe)
	{0x4ADA, 0x75449D37}, // Pokemon Conquest (Europe)
	{0xC0C0, 0xD50A86CD}, // Pokemon Conquest (USA, Australia)
	{0x5FCD, 0x3D6F8988}, // Pokemon Conquest (USA, Australia) (Rev 1)
	{0x2FDA, 0x93E9B4A1}, // Pokemon Plus Nobunaga no Yabou (Japan)
	{0x4F60, 0x1BE180E3}, // Popstars (Germany)
	{0x757E, 0x18F5822E}, // Power Pro Kun Pocket 12 (Japan)
	{0x15B5, 0xED6F80BA}, // Power Pro Kun Pocket 13 (Japan)
	{0x2476, 0x34D1FFA5}, // Power Pro Kun Pocket 14 (Japan)
	{0xC645, 0x312DFD57}, // Prince of Persia - The Forgotten Sands (Europe) (En,Fr,De,Es,It,Nl)
	{0x08A6, 0x9801B6E6}, // Prince of Persia - The Forgotten Sands (USA) (En,Fr,De,Es,It,Nl)
	{0x56B0, 0x615E3A11}, // RPG Tkool DS (Japan)
	{0x1AB5, 0x8D8D62DF}, // RPG Tsukuru DS+ - Create The New World (Japan)
	{0x6B5C, 0xE6C5935A}, // Rabbids Go Home (Europe) (En,Fr,De,Es,It,Nl)
	{0xBEF3, 0xC33E9C90}, // Rabbids Go Home - A Comedy Adventure (USA) (En,Fr,Es)
	{0x854D, 0xFD2DE763}, // Rio (Europe) (En,Fr,De,Es,It,Nl)
	{0x85FB, 0xF4CFB927}, // Rio (USA) (En,Fr,De,Es,It,Nl)
	{0x7257, 0x589A5B33}, // Scripps Spelling Bee (USA)
	{0xE065, 0x28B6B8B8}, // Shin 'Noukyou' Iku (Japan)
	{0x90B4, 0x60A56473}, // Shrek - E Vissero Felici e Contenti (Italy)
	{0xD21E, 0x058C52EE}, // Shrek - Forever After (Europe) (De,Es)
	{0x1A26, 0x25B2BC87}, // Shrek - Forever After (Europe) (En,Fr)
	{0xD266, 0x95D9CF32}, // Shrek - Forever After (Europe) (Nl,Sv)
	{0x361E, 0xA62E3242}, // Shrek - Forever After (USA) (En,Fr)
	{0x05D1, 0xD1014ADF}, // Sims 3, The (Europe) (En,Fr,De,Es,It,Nl)
	{0x27DE, 0xE40D2753}, // Sims 3, The (USA) (En,Fr,Es)
	{0x10BE, 0x471E19DD}, // Solatorobo - Red the Hunter (Europe) (En,Fr,De,Es,It)
	{0x95A5, 0x3C1E9F64}, // Solatorobo - Red the Hunter (USA) (En,Fr,De,Es,It)
	{0xD6D9, 0x7A2EF8AD}, // Solatorobo - Sorekara Coda e (Japan)
	{0xA58B, 0x0BAF985D}, // Sonic Classic Collection (Europe) (En,Fr,De,Es,It)
	{0x82A2, 0x58F85490}, // Sonic Classic Collection (USA) (En,Fr,Es)
	{0x9FDF, 0x689654BD}, // Sonny with a Chance (Europe) (En,Fr,De,Es,It)
	{0x7635, 0x01D7F221}, // Sonny with a Chance (USA) (En,Fr,Es)
	{0xA962, 0x9F50D927}, // SpongeBob's Boating Bash (Europe) (En,De)
	{0xCC67, 0x269846AE}, // SpongeBob's Boating Bash (Europe) (En,Es)
	{0x6440, 0x093173E4}, // SpongeBob's Boating Bash (USA)
	{0xA31F, 0xEDEDFB2B}, // Style Lab - Jewelry Design (USA) (En,Fr,Es)
	{0xE2E6, 0xA63D92A2}, // Style Lab - Jewelry Design (USA) (En,Fr,Es) (Rev 1)
	{0xF442, 0x7ED389C2}, // Style Lab - Makeover (USA) (En,Fr,Es)
	{0x9795, 0x42F8BCA8}, // Sumeun Sojireul Kkaeuneun - Geurimgyosil (Korea)
	{0x10A3, 0xF70F170F}, // Super Kaseki Horider (Japan)
	{0x8FAE, 0x1F6A184A}, // Super Kaseki Horider (Japan) (Rev 1)
	{0x0384, 0x0A26F6AB}, // Sushi Go-Round (USA) (En,Fr,De,Es,It)
	{0x5DF7, 0xB01B1FE4}, // TRON - Evolution (Europe) (En,Fr,De,Es,It,Nl)
	{0x8303, 0x1A54BA4C}, // TRON - Evolution (Europe) (En,Sv,No,Da)
	{0x499A, 0x0E6DBDE1}, // TRON - Evolution (USA) (En,Fr,Es)
	{0x5CA0, 0x6CE92E70}, // TouchMaster - Connect (USA) (En,Fr,Es)
	{0x9BF5, 0x5686BAC5}, // TouchMaster 4 - Connect (Europe) (En,Fr,De,Es,It)
	{0xBA9C, 0x42CCE0DD}, // Toy Story 3 (Europe) (En,Fr,De,Es,It,Nl)
	{0x8453, 0x956BBDC0}, // Toy Story 3 (Europe) (En,Sv,No,Da,Fi)
	{0x477A, 0x4E9D5605}, // Toy Story 3 (Japan)
	{0x6E9B, 0x61787E80}, // Toy Story 3 (USA) (En,Fr,Es)
	{0xE306, 0x53EBEC17}, // Toy Story 3 (USA) (En,Fr,Es) (Target Exclusive)
	{0x2163, 0x37BFD22B}, // VIP News (Europe) (En,De)
	{0xBC07, 0x8E8328F6}, // VIPs - Very Important Pets (Europe) (En,De)
	{0x3300, 0x76438A11}, // Vampire Legends - Power of Three (USA)
	{0x8458, 0x0627FAB9}, // Witches & Vampires - Ghost Pirates of Ashburry (Europe) (En,De) (VWVP)
	{0x5766, 0x7604BFE2}, // Witches & Vampires - Ghost Pirates of Ashburry (Europe) (En,De) (VWVV)
	{0x95E7, 0x53019D6F}, // You Don't Know Jack (USA)
	{0x27C9, 0xCE36DB7D}, // Youda Farmer (Europe) (En,Fr,De)
	{0xC42D, 0x484C48F0}, // Youda Farmer (Europe) (En,Fr,De) (Rev 1)
	{0x5857, 0xFB4749C5}, // Youda Farmer (Europe) (Fr,Nl)
	{0xE292, 0xBCFC91AF}, // Youda Farmer (Europe) (Fr,Nl) (Rev 1)
	{0x9A58, 0xAE0A7823}, // Youda Legend - The Curse of the Amsterdam Diamond (Europe) (En,Fr,De)
	{0x7190, 0x35D4F60D}, // Youda Legend - The Curse of the Amsterdam Diamond (Europe) (Fr,Nl)
	{0x934E, 0x0BB91269}, // Youda Legend - The Golden Bird of Paradise (Europe) (En,Fr,De)
	{0xE9D5, 0x50A731CB}, // Zaidan Houjin Nihon Kanji Nouryoku Kentei Kyoukai Kyouryoku - Kanken DS Training (Japan)
	{0xC2F0, 0xF626C9A5}, // Zhu Zhu Pets 2 - Featuring the Wild Bunch (USA) (En,Fr)
	{0xF31F, 0x650672AB}, // Zhu Zhu Pets featuring the Wild Bunch (Europe) (En,Fr,De,Es,It)
	{0x720E, 0xDBA639AB}, // de Blob 2 (Europe) (En,Fr,De,Es,It,Nl)
	{0x023F, 0x0B62BFA3}, // de Blob 2 (USA) (En,Fr)
	{0x4EEE, 0xC3D38286}, // iCarly (Australia)
	{0xCE09, 0xDDD6FDAA}, // iCarly (Europe) (En,Fr,De,Es,It,Nl)
	{0xE515, 0x8AF8371D}, // iCarly (USA) (En,Fr)
	{0x08A3, 0x72B8606B}, // iCarly 2 - iJoin the Click! (Europe) (En,De,Es,It)
	{0x852C, 0x4E595137}, // iCarly 2 - iJoin the Click! (USA)
};

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
			if(auto found = dsiEnhancedHeaders.find(ndsHeader.header.headerCRC16); found != dsiEnhancedHeaders.end()) {
				guessedCrc32[0] = found->second;
			} else {
				uint8_t extraBuffer[0xA0]{};
				// non dsi enhanced games produced after the dsi was released, have
				// this extra byte possibly set in their header, try to do another
				// guessing round
				// 1BFh 1    Flags (40h=RSA+TwoHMACs, 60h=RSA+ThreeHMACs)
				auto partialHeaderCRC = crc32(&ndsHeader, 0x160);
				guessedCrc32[0] = crc32Partial(extraBuffer, 0xA0, partialHeaderCRC);
				extraBuffer[0x1BF - 0x160] = 0x40;
				guessedCrc32[1] = crc32Partial(extraBuffer, 0xA0, partialHeaderCRC);
				extraBuffer[0x1BF - 0x160] = 0x60;
				guessedCrc32[2] = crc32Partial(extraBuffer, 0xA0, partialHeaderCRC);
			}
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
		guessedCrc32[0] = crc32(&ndsHeader, sizeof(ndsHeader));
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
