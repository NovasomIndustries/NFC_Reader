/*
 * NFC_EncDec.c
 *
 *  Created on: Apr 28, 2021
 *      Author: fil
 */
#include "main.h"

#define UID_SIZE    4
#define SHIFT_01    7
#define TRY         6

uint8_t host_buff[BUF_SIZE];

#ifdef	AFUNCTIONS
static uint8_t ror(uint8_t value, uint8_t number_of_shifts)
{
uint8_t i;
    for(i=0;i<number_of_shifts;i++)
    {
        if (( value & 0x01 ) == 0x01)
            value = (value >> 1 ) | 0x80;
        else
            value = value >> 1;
    }
    return value;
}

static uint8_t rol(uint8_t value, uint8_t number_of_shifts)
{
uint8_t i;
    for(i=0;i<number_of_shifts;i++)
    {
        if (( value & 0x80 ) == 0x80)
            value = (value << 1 ) | 0x01;
        else
            value = value << 1;
    }
    return value;
}

static 	uint8_t neg(uint8_t value)
{
    return value ^= 0xff;
}
#endif

static void messHostBuffer(void)
{
uint32_t random_number,j;

	for(j=0;j<16;j++)
	{
		HAL_RNG_GenerateRandomNumber(&hrng, &random_number);
		host_buff[j*4] 		= (random_number>>24) & 0xff;
		host_buff[(j*4)+1] 	= (random_number>>16) & 0xff;
		host_buff[(j*4)+2] 	= (random_number>>8)  & 0xff;
		host_buff[(j*4)+3] 	=  random_number      & 0xff;
	}
}

void encode_and_write(uint8_t tries)
{
uint8_t buffer[16];
uint32_t random_number,j;

	for(j=0;j<4;j++)
	{
		HAL_RNG_GenerateRandomNumber(&hrng, &random_number);
		buffer[j*4] 		= (random_number>>24) & 0xff;
		buffer[(j*4)+1] 	= (random_number>>16) & 0xff;
		buffer[(j*4)+2] 	= (random_number>>8)  & 0xff;
		buffer[(j*4)+3] 	=  random_number      & 0xff;
	}
	authenticateAndWrite(CARD1_ID,NFC_1STBUFBLK,buffer);

	for(j=0;j<4;j++)
	{
		HAL_RNG_GenerateRandomNumber(&hrng, &random_number);
		buffer[j*4] 		= (random_number>>24) & 0xff;
		buffer[(j*4)+1] 	= (random_number>>16) & 0xff;
		buffer[(j*4)+2] 	= (random_number>>8)  & 0xff;
		buffer[(j*4)+3] 	=  random_number      & 0xff;
	}
	authenticateAndWrite(CARD1_ID,NFC_2NDBUFBLK,buffer);

	for(j=0;j<4;j++)
	{
		HAL_RNG_GenerateRandomNumber(&hrng, &random_number);
		buffer[j*4] 		= (random_number>>24) & 0xff;
		buffer[(j*4)+1] 	= (random_number>>16) & 0xff;
		buffer[(j*4)+2] 	= (random_number>>8)  & 0xff;
		buffer[(j*4)+3] 	=  random_number      & 0xff;
	}
	buffer[0x0c] = uid[0]+tries;
	buffer[0x08] = uid[1]+tries;
	buffer[0x04] = uid[2]+tries;
	buffer[0x00] = uid[3]+tries;
	authenticateAndWrite(CARD1_ID,NFC_3RDBUFBLK,buffer);
}

uint8_t decode(uint8_t tries)
{
uint8_t buffer[16];

	authenticateAndRead(CARD1_ID,NFC_3RDBUFBLK,buffer);
	messHostBuffer();
	if (( buffer[0x0c] - uid[0] ) == tries)
	{
		if (( buffer[0x08] - uid[1] ) == tries)
		{
			if (( buffer[0x04] - uid[2] ) == tries)
			{
				if (( buffer[0x00] - uid[3] ) == tries)
				{
					authenticateAndRead(CARD1_ID,NFC_0BUFBLK,  &host_buff[0]);
					authenticateAndRead(CARD1_ID,NFC_1STBUFBLK,&host_buff[16]);
					authenticateAndRead(CARD1_ID,NFC_2NDBUFBLK,&host_buff[32]);
					authenticateAndRead(CARD1_ID,NFC_3RDBUFBLK,&host_buff[48]);
			    	return tries;
				}
			}
		}
	}
	return 255;
}
