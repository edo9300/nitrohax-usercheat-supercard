#ifndef SC_MODE_H
#define SC_MODE_H

#define SC_MODE_RAM 0x5

static void SC_changeMode(unsigned char mode) {
	volatile unsigned short* unlockAddress = (volatile unsigned short*)0x09FFFFFE;
	*unlockAddress = 0xA55A;
	*unlockAddress = 0xA55A;
	*unlockAddress = mode;
	*unlockAddress = mode;
}

#endif