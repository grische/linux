#ifndef _I2C_H_
#define _I2C_H_

/*-------------------------------------------------------------------------------------*\
    Global
\*-------------------------------------------------------------------------------------*/
void I2C_Init(unsigned int sdata, unsigned int scl);
void I2C_WriteBuffer(unsigned char DeviceAddr, unsigned char addr, unsigned char Write_Bytes, unsigned char *Buffer);
unsigned int I2C_ReadBuffer(unsigned char DeviceAddr, unsigned char addr, unsigned char *Buffer, unsigned char maxbuffersize);

unsigned char I2C_ReadByte(unsigned char DeviceAddr, unsigned char addr);
void I2C_WriteByte(unsigned char DeviceAddr, unsigned char addr, unsigned char Byte);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#define I2C_SDA         1
#define I2C_SCK         2

#define CDCEL949_DEVICE_ID           0xD8

#define CDCEL949_GENERAL_ADDR        0x0
#define CDCEL949_PLL1_ADDR           0x10
#define CDCEL949_BUFCOUNT_ADDR       0x6
#define CDCEL949_PLL1_MUX_ADDR       0x14
#define CDCEL949_PLL1_PDIV2_ADDR     0x16
#define CDCEL949_PLL1_0xADDR         0x18
#define CDCEL949_PLL1_1xADDR         0x1C
#define CDCEL949_PLL1_xx_LEN         0x4


#endif /*--- #ifndef _I2C_H_ ---*/
