/*
 * w25.h
 *
 *  Created on: May 17, 2021
 *      Author: lth
 */

#ifndef W25_H_
#define W25_H_

#define W25_READ 0x03
#define W25_READ_STATUS_1 0x05
#define W25_WRITE_ENABLE 0x06
#define W25_QUAD_WRITE 0x32
#define W25_QUAD_READ 0x6B
#define W25_READ_ID 0x9f
#define W25_SECTOR_ERASE 0x20
#define W25_DEVICE_ERASE 0xC7

#define W25_PAGE_SIZE 0x100 // 256 B
#define W25_SECTOR_SIZE 0x1000 // 4kB

typedef enum {
	W25_Err,
	W25_Ok
} W25_result_t;

W25_result_t w25init();

W25_result_t w25_read(uint32_t address, uint8_t *dat, uint32_t len);
W25_result_t w25_read_decrypt(uint32_t address, uint8_t *key, uint8_t *dat, uint32_t len);

W25_result_t w25_write(uint32_t address, uint8_t *dat, uint32_t len);
W25_result_t w25_write_encrypt(uint32_t address, uint8_t *key, uint8_t *dat, uint32_t len);

W25_result_t w25_erase(uint32_t address, uint32_t len);

W25_result_t w25_device_erase();

void w25_dump(uint32_t address, uint32_t len);
void w25_dump_decrypt(uint32_t address, uint8_t *key, uint32_t len);

uint8_t w25_get_status();

void w25_write_enable();

#endif /* W25_H_ */
