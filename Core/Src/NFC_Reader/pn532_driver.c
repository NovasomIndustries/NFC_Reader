/*
 * pn532_driver.c
 *
 *  Created on: May 3, 2021
 *      Author: fil
 */

#include "main.h"
#include <pn532_driver.h>
#include <string.h>

uint8_t	tx_buff[BUF_SIZE],rx_buff[BUF_SIZE],uid[16],uidLength;
const uint8_t pn532ack[]  = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
const uint8_t pn532nack[] = {0x00, 0x00, 0xFF, 0x01, 0xFF, 0x7F};
const uint8_t pn532response_firmwarevers[] = {0x00, 0x00, 0xFF, 0x06, 0xFA, 0xD5, 0x03};
const uint8_t authenticate_A[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const uint8_t authenticated_buf[] = {PN532_PN532TOHOST,PN532_RESPONSE_INDATAEXCHANGE,MIFARE_RESPONSE_AUTHORIZED};

void  readData( uint8_t byte_count)
{
	tx_buff[0] = PN532_SPI_DATAREAD;
	HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_SPI_TransmitReceive(&hspi1, tx_buff, rx_buff, byte_count, SPI_TIMEOUT);
	//HAL_Delay(1);
    HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_SET);
}

uint8_t	wait_irq(uint32_t timeout)
{
	while (HAL_GPIO_ReadPin(NFC_IRQ_GPIO_Port,NFC_IRQ_Pin) == GPIO_PIN_SET)
	{
		if ( timeout == 0 )
			return 1;
		timeout --;
		HAL_Delay(1);
	}
	return 0;
}

uint8_t writeCommand(uint8_t* cmd, uint8_t cmdlen, uint8_t timeout)
{
uint8_t checksum,i;

	tx_buff[0] = PN532_SPI_DATAWRITE;
	tx_buff[1] = PN532_PREAMBLE;
	tx_buff[2] = PN532_STARTCODE1;
	tx_buff[3] = PN532_STARTCODE2;
	tx_buff[4] = cmdlen + 1;
	tx_buff[5] = ~tx_buff[4] + 1;
	checksum = tx_buff[6] = PN532_HOSTTOPN532;
	for (i=0; i<cmdlen; i++)							// Send data to PN532
	{
		tx_buff[7+i] = cmd[i];
		checksum += tx_buff[7+i];
	}
	tx_buff[7+i] = ~checksum + 1;
	tx_buff[8+i] = PN532_POSTAMBLE;
	HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_SPI_TransmitReceive(&hspi1, tx_buff, rx_buff, cmdlen+9, SPI_TIMEOUT);
	//HAL_Delay(1);
    HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_SET);
    wait_irq(timeout);
    readData(7);
	if ( strncmp((char *)&rx_buff[1], (char *)pn532ack, 6) == 1)
		return 1;
    wait_irq(timeout);
	readData(32);
	return 0;

}

uint8_t getFirmwareVersion(uint8_t *ic,uint8_t *ver,uint8_t *rev,uint8_t *support)
{
uint8_t	tbuff[1];

	tbuff[0] = PN532_COMMAND_GETFIRMWAREVERSION;
	if ( writeCommand(tbuff, 1,SPI_TIMEOUT))
		return 1;
	if ( rx_buff[4] != 0x06 )
		return 1;
	*ic  = rx_buff[8]<<24;
	*ver = rx_buff[9]<<16;
	*rev = rx_buff[10]<<8;
	*support = rx_buff[11];
	return 0;
}

uint8_t setPassiveActivationRetries(uint8_t maxRetries)
{
uint8_t	tbuff[5];

	tbuff[0] = PN532_COMMAND_RFCONFIGURATION;
	tbuff[1] = 5;    // Config item 5 (MaxRetries)
	tbuff[2] = 0xFF; // MxRtyATR (default = 0xFF)
	tbuff[3] = 0x01; // MxRtyPSL (default = 0x01)
	tbuff[4] = maxRetries;
	// send configuration message to PN532
	return writeCommand(tbuff, 5 , 10);
}

uint8_t SAMConfig(void)
{
uint8_t	tbuff[4];

	tbuff[0] = PN532_COMMAND_SAMCONFIGURATION;
	tbuff[1] = 0x01; // normal mode;
	tbuff[2] = 0x14; // timeout 50ms * 20 = 1 second
	tbuff[3] = 0x01; // use IRQ pin!
	if ( writeCommand(tbuff, 4,SPI_TIMEOUT))
		return 1;
	if (rx_buff[7] == 0x15)
		return 0;
	return 1;
}

void hard_reset(void)
{
	HAL_GPIO_WritePin(PN532_RST_GPIO_Port, PN532_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(PN532_RST_GPIO_Port, PN532_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(PN532_RST_GPIO_Port, PN532_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(1);

	HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_RESET);
	HAL_Delay(2);
    HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_SET);
}

uint8_t initializePN532(void)
{
uint8_t retries = 4;
uint32_t version=0;
uint8_t ic,ver,rev,support;

	hard_reset();
	logUsartInit(&huart2);
	logUsart("Initialization Started\r\n");

	while (retries != 0)
	{
		if ( (version = getFirmwareVersion(&ic,&ver,&rev,&support)) == 0 )
		{
			if (setPassiveActivationRetries(0xFF) == 0 )
			{
				if ( SAMConfig() == 0 )
				{
					logUsart("Found PN5%02x V%d R%d\r\n");
					if ( support & 0x01 )
						logUsart("Support for ISO/IEC 14443 TypeA\r\n");
					if ( support & 0x02 )
						logUsart("Support for ISO/IEC 14443 TypeB\r\n");
					if ( support & 0x04 )
						logUsart("Support for ISO18092\r\n");
					return 0;
				}
			}
		}
		retries--;		// Initialization failed, increment retries
	}
	return 1;
}

uint8_t readPassiveTargetID( uint8_t * uid, uint8_t * uidLength)
{
uint8_t	len,i;
uint8_t	tbuff[3];

	tbuff[0] = PN532_COMMAND_INLISTPASSIVETARGET;
	tbuff[1] = 1;
	tbuff[2] = PN532_MIFARE_ISO14443A;
	if (writeCommand(tbuff, 3, SPI_TIMEOUT))
		return 1;  // no cards read

	/* ISO14443A card response should be in the following format:

	    byte            Description
	    -------------   ------------------------------------------
	    b0	            SPI Garbage
	    b1..7           Frame header and preamble
	    b8              Tags Found
	    b9              Tag Number (only one used in this example)
	    b10..11         SENS_RES
	    b12             SEL_RES
	    b13             NFCID Length
	    b14..NFCIDLen   NFCID                                      */

	if (rx_buff[8] != 1)
		return rx_buff[8];	//  Tag seems not a Mifare Classic or too many tags in the field
	len = rx_buff[13];	// set value of NFCID length to uidLength
	for (i=0; i < len; i++)
		uid[i] = rx_buff[14+i];
	*uidLength = len;
	return 0;
}

uint8_t checkCardPresence(void)
{
	return readPassiveTargetID( &uid[0], &uidLength);
}

uint8_t authenticate(uint8_t card_number, uint8_t page)
{
uint8_t	tbuff[32],i;

	if ( uidLength != MIFARE_UID_LEN)
		return MIFARE_AUTHENTICATED;
	tbuff[0]  = PN532_COMMAND_INDATAEXCHANGE;
	tbuff[1]  = card_number;	// card number
	tbuff[2]  = MIFARE_CMD_AUTH_A;
	tbuff[3]  = page;	// page
	tbuff[4]  = authenticate_A[0];
	tbuff[5]  = authenticate_A[1];
	tbuff[6]  = authenticate_A[2];
	tbuff[7]  = authenticate_A[3];
	tbuff[8]  = authenticate_A[4];
	tbuff[9]  = authenticate_A[5];
	for(i=0;i<uidLength;i++)
		tbuff[10+i] = uid[i];
	if (writeCommand(tbuff, 10+uidLength, SPI_TIMEOUT))
		return MIFARE_NOT_AUTHENTICATED;  // no cards read
	return MIFARE_AUTHENTICATED;
}

uint8_t authenticateAndRead(uint8_t card_number, uint8_t page, uint8_t *buffer)
{
uint8_t	tbuff[32];
uint8_t	i;

	if ( uidLength == MIFARE_UID_LEN)
	{
		if ( authenticate(card_number , page) )
			return TAG_INVALID;
		if ( strncmp((char *)authenticated_buf,(char *)&rx_buff[6],3) == 0 )
		{
			tbuff[0]  = PN532_COMMAND_INDATAEXCHANGE;
			tbuff[1]  = card_number;	// card number
			tbuff[2]  = MIFARE_CMD_READ;
			tbuff[3]  = page;	// page
			if (writeCommand(tbuff, 4, SPI_TIMEOUT))
				return TAG_ERROR;
			for(i=0;i<MIFARE_CLASSIC_PAGE_SIZE;i++)
				buffer[i] = rx_buff[i+9];
			return TAG_ACCESS_OK;
		}
	}
	if ( uidLength == MIFARE_ULTRALIGHT_UID_LEN)
	{
		tbuff[0]  = PN532_COMMAND_INDATAEXCHANGE;
		tbuff[1]  = card_number;	// card number
		tbuff[2]  = MIFARE_ULTRALIGHT_CMD_READ;
		tbuff[3]  = page*4;	// page
		if (writeCommand(tbuff, 4, SPI_TIMEOUT))
			return TAG_ERROR;
		for(i=0;i<MIFARE_CLASSIC_PAGE_SIZE;i++)
			buffer[i] = rx_buff[i+9];
		return TAG_ACCESS_OK;
	}
	return TAG_AUTHENTICATION_FAILED;
}

uint8_t authenticateAndWrite(uint8_t card_number, uint8_t page, uint8_t *buffer)
{
uint8_t	tbuff[32],i, ipage=0;


	if ( uidLength == MIFARE_UID_LEN)
	{
		if ( authenticate(card_number , page) )
			return TAG_INVALID;
		if ( strncmp((char *)authenticated_buf,(char *)&rx_buff[6],3) == 0 )
		{
			tbuff[0]  = PN532_COMMAND_INDATAEXCHANGE;
			tbuff[1]  = card_number;
			tbuff[2]  = MIFARE_CMD_WRITE;
			tbuff[3]  = page;
			for ( i=0;i<MIFARE_CLASSIC_PAGE_SIZE;i++)
				tbuff[i+4] = buffer[i];
			if (writeCommand(tbuff, MIFARE_CLASSIC_PAGE_SIZE+4, SPI_TIMEOUT))
				return TAG_ERROR;
			return TAG_ACCESS_OK;
		}
	}
	if ( uidLength == MIFARE_ULTRALIGHT_UID_LEN)
	{
		for(ipage=0;ipage<4;ipage++)
		{
			tbuff[0]  = PN532_COMMAND_INDATAEXCHANGE;
			tbuff[1]  = card_number;
			tbuff[2]  = MIFARE_ULTRALIGHT_CMD_WRITE;
			tbuff[3]  = (page*4)+ipage;
			for ( i=0;i<MIFARE_ULTRALIGHT_PAGE_SIZE;i++)
				tbuff[i+4] = buffer[i];
			if (writeCommand(tbuff, MIFARE_ULTRALIGHT_PAGE_SIZE+4, SPI_TIMEOUT))
				return TAG_ERROR;
		}
		return TAG_ACCESS_OK;
	}
	return TAG_AUTHENTICATION_FAILED;
}

void dump_TAGinfo(uint8_t card_number)
{
	if ( uidLength == MIFARE_UID_LEN)
	{
		logUsart("MiFare Classic TAG : %d\r\n",card_number);
		logUsart("UID : 0x%02x 0x%02x 0x%02x 0x%02x\r\n",uid[0],uid[1],uid[2],uid[3]);
	}
	if ( uidLength == MIFARE_ULTRALIGHT_UID_LEN)
	{
		logUsart("MiFare UltraLight TAG : %d\r\n",card_number);
		logUsart("UID : 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\r\n",uid[0],uid[1],uid[2],uid[3],uid[4],uid[5],uid[6]);
	}
}

void dump_buffer(uint8_t page,uint8_t *buffer)
{
uint8_t	i;
	logUsart("%02x : ", page);
	for(i=0;i<MIFARE_CLASSIC_PAGE_SIZE;i++)
	{
		logUsart("%02x ", buffer[i]);
	}
	logUsart("\r\n");
}

void dump_card(void)
{
uint8_t page,buffer[16];

	for(page=0;page<16;page++)
	{
		authenticateAndRead(CARD1_ID, page, buffer);
		dump_buffer(page,buffer);
	}

}

void dump_0456_card(void)
{
uint8_t buffer[16];
	authenticateAndRead(CARD1_ID, NFC_0BUFBLK, buffer);
	dump_buffer(NFC_0BUFBLK,buffer);
	authenticateAndRead(CARD1_ID, NFC_1STBUFBLK, buffer);
	dump_buffer(NFC_1STBUFBLK,buffer);
	authenticateAndRead(CARD1_ID, NFC_2NDBUFBLK, buffer);
	dump_buffer(NFC_2NDBUFBLK,buffer);
	authenticateAndRead(CARD1_ID, NFC_3RDBUFBLK, buffer);
	dump_buffer(NFC_3RDBUFBLK,buffer);
}

