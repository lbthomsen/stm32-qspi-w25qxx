/*
 * w25.c
 *
 *  Created on: May 17, 2021
 *      Author: Lars Boegild Thomsen <lbthomsen@gmail.com>
 */

#include <stdio.h>

#include "main.h"
#include "w25.h"
#include "stm32l4xx_hal_qspi.h"

// The hqspi is declared and initialized in main
extern QSPI_HandleTypeDef hqspi;

// We use the same one everywhere
QSPI_CommandTypeDef sCommand;

W25_result_t w25init() {

	uint8_t rx_buffer[0x10];

	//DBG("ws25init");

	/***** Read ID operation*****/
	sCommand.Instruction = W25_READ_ID; //READ ID command code
	sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE; //Command line width
	sCommand.AddressMode = QSPI_ADDRESS_NONE; //Address line width. No address phase
	sCommand.DataMode = QSPI_DATA_1_LINE; //Data line width
	sCommand.NbData = 4; //Read the data length. ID length is 3 bytes
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; //No multiplexing byte stage
	sCommand.DummyCycles = 0; //No Dummy phase
	sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
	sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	//Configuration command (when there is data stage, the command will be sent in the subsequent sending/receiving API call)
	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)
			!= HAL_OK) {
		Error_Handler();
	}
	//Execute QSPI reception
	if (HAL_QSPI_Receive(&hqspi, rx_buffer, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)
			!= HAL_OK) {
		Error_Handler();
	}

	return W25_Ok;

}

W25_result_t w25_read(uint32_t address, uint8_t *dat, uint32_t len) {

	sCommand.Instruction = W25_QUAD_READ; //Quick read command code with four lines
	sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE; //Command line width
	sCommand.AddressMode = QSPI_ADDRESS_1_LINE; //Address line width
	sCommand.AddressSize = QSPI_ADDRESS_24_BITS; //Address length
	sCommand.Address = address; //Start address
	sCommand.DataMode = QSPI_DATA_4_LINES; //Data line width
	sCommand.NbData = len; //Read data length
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; //No multiplexing byte stage
	sCommand.DummyCycles = 8; //Dummy phase. w25q128

	//Configuration command (when there is data stage, the command will be sent in the subsequent sending/receiving API call)
	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE)
			!= HAL_OK) {
		Error_Handler();
	}
	//Execute QSPI reception
	if (HAL_QSPI_Receive(&hqspi, dat, HAL_QSPI_TIMEOUT_DEFAULT_VALUE)
			!= HAL_OK) {
		Error_Handler();
	}

	return W25_Ok;
}

W25_result_t w25_read_decrypt(uint32_t address, uint8_t *key, uint8_t *dat, uint32_t len) {

	W25_result_t return_value;

	return_value = w25_read(address, dat, len);

	uint8_t *key_idx = key;

	for (uint32_t i = 0; i < len; ++i) {
		dat[i] = dat[i] ^ *key_idx;
		key_idx++;
	}

	return return_value;
}

W25_result_t w25_write(uint32_t address, uint8_t *dat, uint32_t len) {

	uint32_t start_page = address / W25_PAGE_SIZE;
	uint32_t end_page = (address + len - 1) / W25_PAGE_SIZE;
	uint32_t start_address, number;

	for (uint32_t page = start_page; page <= end_page; page++) {

		uint32_t page_start = page * W25_PAGE_SIZE;
		uint32_t page_end = page_start + W25_PAGE_SIZE;

		if (page == start_page) { // First page
			start_address = page * W25_PAGE_SIZE + (address - page * W25_PAGE_SIZE);
			if (start_address + len < page_end) {
				number = len;
			} else {
				number = page_end - start_address;
			}
		} else if (page == end_page) { // Last page
			start_address = page * W25_PAGE_SIZE;
			number = len - (page * W25_PAGE_SIZE - address);
		} else { // All other pages
			start_address = page_start;
			number = W25_PAGE_SIZE;
		}

		uint32_t data_start = start_address - address;

		w25_write_enable();

		/***** Four-wire fast write operation*****/
		sCommand.Instruction = W25_QUAD_WRITE; //Quick write command code with four lines
		sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE; //Command line width
		sCommand.AddressMode = QSPI_ADDRESS_1_LINE; //Address line width
		sCommand.AddressSize = QSPI_ADDRESS_24_BITS; //Address length
		sCommand.Address = start_address; //Write the starting address
		sCommand.DataMode = QSPI_DATA_4_LINES; //Data line width
		sCommand.NbData = number; //write data length
		sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; //No multiplexing byte stage
		sCommand.DummyCycles = 0; //No Dummy phase
		//Configuration command (when there is data stage, the command will be sent in the subsequent sending/receiving API call)
		if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE)
				!= HAL_OK) {
			Error_Handler();
		}
		//Execute QSPI transmission
		if (HAL_QSPI_Transmit(&hqspi, &dat[data_start], HAL_QSPI_TIMEOUT_DEFAULT_VALUE)
				!= HAL_OK) {
			Error_Handler();
		}

		// Wait until no longer busy
		while (w25_get_status() && 0x01 == 0x01) {
		};

	}

	return W25_Ok;
}

W25_result_t w25_write_encrypt(uint32_t address, uint8_t *key, uint8_t *dat, uint32_t len) {

	uint8_t *key_idx = key;

	for (uint32_t i = 0; i < len; ++i) {
		dat[i] = dat[i] ^ *key_idx;
		key_idx++;
	}

	return w25_write(address, dat, len);

}

W25_result_t w25_erase(uint32_t address, uint32_t len) {

	// First let's find the start sector
	uint32_t start_sector = address / W25_SECTOR_SIZE;
	uint32_t end_sector = (address + len) / W25_SECTOR_SIZE;

	//DBG("w25_erase s=0x%08x e=0x%08x", start_sector, end_sector);

	for (uint32_t i = start_sector; i <= end_sector; i++) { // Repeat for each sector

		//DBG("w25_erase a=0x%08x", i * W25_SECTOR_SIZE);

		w25_write_enable();

		/***** Block erase operation*****/
		sCommand.Instruction = W25_SECTOR_ERASE; //Sector erase command code
		sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE; //Command line width
		sCommand.AddressMode = QSPI_ADDRESS_1_LINE; //Address line width. No address phase
		sCommand.AddressSize = QSPI_ADDRESS_24_BITS; //Address length
		sCommand.Address = i * W25_SECTOR_SIZE; //Any address in the sector to be erased.
		sCommand.DataMode = QSPI_DATA_NONE; //Data line width. No data stage
		sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; //No multiplexing byte stage
		sCommand.DummyCycles = 0; //No Dummy phase
		//Configure sending command
		if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)
				!= HAL_OK) {
			Error_Handler();
		}

		// Wait until no longer busy
		while (w25_get_status() && 0x01 == 0x01) {
		};

	}

	//DBG("w25_erase success");

	return W25_Ok;
}

W25_result_t w25_device_erase() {

	w25_write_enable();

	/***** Block erase operation*****/
	sCommand.Instruction = W25_DEVICE_ERASE; //Sector erase command code
	sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE; //Command line width
	sCommand.AddressMode = QSPI_ADDRESS_NONE; //Address line width. No address phase
	sCommand.DataMode = QSPI_DATA_NONE; //Data line width. No data stage
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; //No multiplexing byte stage
	sCommand.DummyCycles = 0; //No Dummy phase
	//Configure sending command
	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)
			!= HAL_OK) {
		Error_Handler();
	}

	// Wait until no longer busy
	while (w25_get_status() && 0x01 == 0x01) {};

	return W25_Ok;

}

void w25_dump(uint32_t address, uint32_t len) {

	uint8_t buf[len];

	w25_read(address, buf, len);

	uint32_t line_length = 16;
	uint32_t lines = len / line_length;

	for (uint32_t line = 0; line < lines; ++line) {
		printf("0x%08lx: ", address + line * line_length);
		for (uint32_t p = 0; p < line_length; p++) {
			if (line * line_length + p < len) {
				printf("%02x ", buf[line * line_length + p]);
			}
		}
		printf("\n");
		HAL_Delay(1);
	}

}

void w25_dump_decrypt(uint32_t address, uint8_t *key, uint32_t len) {

	uint8_t buf[len];

	w25_read_decrypt(address, key, buf, len);

	uint32_t line_length = 16;
	uint32_t lines = len / line_length;

	for (uint32_t line = 0; line < lines; ++line) {
		printf("0x%08lx: ", address + line * line_length);
		for (uint32_t p = 0; p < line_length; p++) {
			if (line * line_length + p < len) {
				printf("%02x ", buf[line * line_length + p]);
			}
		}
		printf("\n");
		HAL_Delay(1);
	}
}

// Read status register
uint8_t w25_get_status() {

	uint8_t result;

	/***** Read ID operation*****/
	sCommand.Instruction = W25_READ_STATUS_1; //READ ID command code
	sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE; //Command line width
	sCommand.AddressMode = QSPI_ADDRESS_NONE; //Address line width. No address phase
	sCommand.DataMode = QSPI_DATA_1_LINE; //Data line width
	sCommand.NbData = 1; //Read the data length. ID length is 17 bytes
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; //No multiplexing byte stage
	sCommand.DummyCycles = 0; //No Dummy phase
	sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
	sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	//Configuration command (when there is data stage, the command will be sent in the subsequent sending/receiving API call)
	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)
			!= HAL_OK) {
		Error_Handler();
	}
	//Execute QSPI reception
	if (HAL_QSPI_Receive(&hqspi, &result, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)
			!= HAL_OK) {
		Error_Handler();
	}

	return result;

}

void w25_write_enable() {
	/***** Write enable operation (need to make the external memory in the write enable state before block erasing) *****/
	sCommand.Instruction = W25_WRITE_ENABLE; //Write enable command code
	sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE; //Command line width
	sCommand.AddressMode = QSPI_ADDRESS_NONE; //Address line width. No address phase
	sCommand.DataMode = QSPI_DATA_NONE; //Data line width. No data stage
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; //No multiplexing byte stage
	sCommand.DummyCycles = 0; //No Dummy phase
	sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;
	sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	//Configure sending command
	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE)
			!= HAL_OK) {
		Error_Handler();
	}
}

