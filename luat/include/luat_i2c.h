#ifndef LUAT_I2C_H
#define LUAT_I2C_H

#include "luat_base.h"

int luat_i2c_exist(int id);
int luat_i2c_setup(int id, int speed);
int luat_i2c_close(int id);
int luat_i2c_send(int id, int addr, void* buff, size_t len, uint8_t stop);
int luat_i2c_recv(int id, int addr, void* buff, size_t len);

int luat_i2c_write_reg(int id, int addr, int reg, uint16_t value, uint8_t stop);
int luat_i2c_read_reg(int id, int addr, int reg, uint16_t* value);

int luat_i2c_transfer(int id, int addr, uint8_t *reg, size_t reg_len, uint8_t *buff, size_t len);
int luat_i2c_no_block_transfer(int id, int addr, uint8_t is_read, uint8_t *reg, size_t reg_len, uint8_t *buff, size_t len, uint16_t Toms, void *CB, void *pParam);

#endif
