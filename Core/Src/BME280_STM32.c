#include "BME280_STM32.h"

extern I2C_HandleTypeDef hi2c2;
#define BME280_I2C &hi2c2

#define SUPPORT_64BIT 1

#define BME280_ADDRESS 0xEC  // SDIO заземлен, иначе 0x76

extern float Temperature, Pressure, Humidity;

uint8_t chipID;

uint8_t TrimParam[36];
int32_t tRaw, pRaw, hRaw;

uint16_t dig_T1,  \
         dig_P1, \
         dig_H1, dig_H3;

int16_t  dig_T2, dig_T3, \
         dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9, \
		 dig_H2,  dig_H4, dig_H5, dig_H6;



// Read the Trimming parameters saved in the NVM ROM of the device
void TrimRead(void)
{
	uint8_t trimdata[32];
	// Чтение NVM от 0x88 до 0xA1
	HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, 0x88, 1, trimdata, 25, HAL_MAX_DELAY);

	// Чтение NVM от 0xE1 до 0xE7
	HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, 0xE1, 1, (uint8_t *)trimdata+25, 7, HAL_MAX_DELAY);

	// Выделя данные согласное даташиту(стр 24.)
	dig_T1 = (trimdata[1]<<8) | trimdata[0];
	dig_T2 = (trimdata[3]<<8) | trimdata[2];
	dig_T3 = (trimdata[5]<<8) | trimdata[4];
	dig_P1 = (trimdata[7]<<8) | trimdata[5];
	dig_P2 = (trimdata[9]<<8) | trimdata[6];
	dig_P3 = (trimdata[11]<<8) | trimdata[10];
	dig_P4 = (trimdata[13]<<8) | trimdata[12];
	dig_P5 = (trimdata[15]<<8) | trimdata[14];
	dig_P6 = (trimdata[17]<<8) | trimdata[16];
	dig_P7 = (trimdata[19]<<8) | trimdata[18];
	dig_P8 = (trimdata[21]<<8) | trimdata[20];
	dig_P9 = (trimdata[23]<<8) | trimdata[22];
	dig_H1 = trimdata[24];
	dig_H2 = (trimdata[26]<<8) | trimdata[25];
	dig_H3 = (trimdata[27]);
	dig_H4 = (trimdata[28]<<4) | (trimdata[29] & 0x0f);
	dig_H5 = (trimdata[30]<<4) | (trimdata[29]>>4);
	dig_H6 = (trimdata[31]);
}

int BME280_Config (uint8_t osrs_t, uint8_t osrs_p, uint8_t osrs_h, uint8_t mode, uint8_t t_sb, uint8_t filter)
{
	// Читаем параметры
	TrimRead();


	uint8_t datatowrite = 0;
	uint8_t datacheck = 0;

	// Сбрасываем устройство
	datatowrite = 0xB6;
	if (HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDRESS, RESET_REG, 1, &datatowrite, 1, 1000) != HAL_OK)
	{
		return -1;
	}

	HAL_Delay (100);


	// записываем влажность oversampling 0xF2
	datatowrite = osrs_h;
	if (HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDRESS, CTRL_HUM_REG, 1, &datatowrite, 1, 1000) != HAL_OK)
	{
		return -1;
	}
	HAL_Delay (100);
	HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, CTRL_HUM_REG, 1, &datacheck, 1, 1000);
	if (datacheck != datatowrite)
	{
		return -1;
	}


	// записываем время ожидания и коэфициент БИК фильтра 0xF5
	datatowrite = (t_sb <<5) |(filter << 2);
	if (HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDRESS, CONFIG_REG, 1, &datatowrite, 1, 1000) != HAL_OK)
	{
		return -1;
	}
	HAL_Delay (100);
	HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, CONFIG_REG, 1, &datacheck, 1, 1000);
	if (datacheck != datatowrite)
	{
		return -1;
	}


	// записываем oversampling давление и температуры согласно моду 0xF4
	datatowrite = (osrs_t <<5) |(osrs_p << 2) | mode;
	if (HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDRESS, CTRL_MEAS_REG, 1, &datatowrite, 1, 1000) != HAL_OK)
	{
		return -1;
	}
	HAL_Delay (100);
	HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, CTRL_MEAS_REG, 1, &datacheck, 1, 1000);
	if (datacheck != datatowrite)
	{
		return -1;
	}

	return 0;
}


int BMEReadRaw(void)
{
	uint8_t RawData[8];

	// Читаем чип перед чтением
	HAL_I2C_Mem_Read(&hi2c2, BME280_ADDRESS, ID_REG, 1, &chipID, 1, 1000);

	if (chipID == 0x60)
	{
		// Читаем регистры с 0xF7 по 0xFE
		HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, PRESS_MSB_REG, 1, RawData, 8, HAL_MAX_DELAY);

		/* Вычисляем сырые данные
		 * Давление и температура в 20 битном формате, влажность в 16 битном формате
		 *
		 */
		pRaw = (RawData[0]<<12)|(RawData[1]<<4)|(RawData[2]>>4);
		tRaw = (RawData[3]<<12)|(RawData[4]<<4)|(RawData[5]>>4);
		hRaw = (RawData[6]<<8)|(RawData[7]);

		return 0;
	}

	else return -1;
}


void BME280_WakeUP(void)
{
	uint8_t datatowrite = 0;

	// Читаем первый регистр
	HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, CTRL_MEAS_REG, 1, &datatowrite, 1, 1000);

	// Изменяем данные в force моде
	datatowrite = datatowrite | MODE_FORCED;

	// Записываем новые данные в регистр
	HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDRESS, CTRL_MEAS_REG, 1, &datatowrite, 1, 1000);

	HAL_Delay (100);
}

// Вычисления согласно даташиту на датчик
int32_t t_fine;
int32_t BME280_compensate_T_int32(int32_t adc_T)
{
	int32_t var1, var2, T;
	var1 = ((((adc_T>>3) - ((int32_t)dig_T1<<1))) * ((int32_t)dig_T2)) >> 11;
	var2 = (((((adc_T>>4) - ((int32_t)dig_T1)) * ((adc_T>>4) - ((int32_t)dig_T1)))>> 12) *((int32_t)dig_T3)) >> 14;
	t_fine = var1 + var2;
	T = (t_fine * 5 + 128) >> 8;
	return T;
}


#if SUPPORT_64BIT
// Возвращает давление в int формате
uint32_t BME280_compensate_P_int64(int32_t adc_P)
{
	int64_t var1, var2, p;
	var1 = ((int64_t)t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)dig_P6;
	var2 = var2 + ((var1*(int64_t)dig_P5)<<17);
	var2 = var2 + (((int64_t)dig_P4)<<35);
	var1 = ((var1 * var1 * (int64_t)dig_P3)>>8) + ((var1 * (int64_t)dig_P2)<<12);
	var1 = (((((int64_t)1)<<47)+var1))*((int64_t)dig_P1)>>33;
	if (var1 == 0)
	{
		return 0; // избегаем деление на нуль
	}
	p = 1048576-adc_P;
	p = (((p<<31)-var2)*3125)/var1;
	var1 = (((int64_t)dig_P9) * (p>>13) * (p>>13)) >> 25;
	var2 = (((int64_t)dig_P8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7)<<4);
	return (uint32_t)p;
}

#elif SUPPORT_32BIT
// Возвращает давление в 4 байтном формате
uint32_t BME280_compensate_P_int32(int32_t adc_P)
{
	int32_t var1, var2;
	uint32_t p;
	var1 = (((int32_t)t_fine)>>1) - (int32_t)64000;
	var2 = (((var1>>2) * (var1>>2)) >> 11 ) * ((int32_t)dig_P6);
	var2 = var2 + ((var1*((int32_t)dig_P5))<<1);
	var2 = (var2>>2)+(((int32_t)dig_P4)<<16);
	var1 = (((dig_P3 * (((var1>>2) * (var1>>2)) >> 13 )) >> 3) + ((((int32_t)dig_P2) *var1)>>1))>>18;
	var1 =((((32768+var1))*((int32_t)dig_P1))>>15);
	if (var1 == 0)
	{
		return 0; // избегаем деление на нуль
	}
	p = (((uint32_t)(((int32_t)1048576)-adc_P)-(var2>>12)))*3125;
	if (p < 0x80000000)
	{
		p = (p << 1) / ((uint32_t)var1);
	}
	else
	{
		p = (p / (uint32_t)var1) * 2;
	}
	var1 = (((int32_t)dig_P9) * ((int32_t)(((p>>3) * (p>>3))>>13)))>>12;
	var2 = (((int32_t)(p>>2)) * ((int32_t)dig_P8))>>13;
	p = (uint32_t)((int32_t)p + ((var1 + var2 + dig_P7) >> 4));
	return p;
}
#endif

// Возвращает влажность в 4 байтном формате
uint32_t bme280_compensate_H_int32(int32_t adc_H)
{
	int32_t v_x1_u32r;
	v_x1_u32r = (t_fine - ((int32_t)76800));
	v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) *\
			v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r *\
					((int32_t)dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) +\
							((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)dig_H2) +\
					8192) >> 14));
	v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *\
			((int32_t)dig_H1)) >> 4));
	v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
	v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
	return (uint32_t)(v_x1_u32r>>12);
}
/*********************************************************************************************************/


// Измеряет параметры среды
void BME280_Measure (void)
{
	if (BMEReadRaw() == 0)
	{
		  if (tRaw == 0x800000) Temperature = 0; // значение если данные о температуры не доступны
		  else
		  {
			  Temperature = (BME280_compensate_T_int32 (tRaw))/100.0;  // согласно даташиту делим на 100
		  }

		  if (pRaw == 0x800000) Pressure = 0; // значение если данные о давлении не доступны
		  else
		  {
#if SUPPORT_64BIT
			  Pressure = (BME280_compensate_P_int64 (pRaw))/256.0;  // согласно даташиту делим на 256

#elif SUPPORT_32BIT
			  Pressure = (BME280_compensate_P_int32 (pRaw));

#endif
		  }

		  if (hRaw == 0x8000) Humidity = 0; // значение если данные о влажности не доступны
		  else
		  {
			  Humidity = (bme280_compensate_H_int32 (hRaw))/1024.0;  // согласно даташиту делим на 1024
		  }
	}


	// если датчик отключен
	else
	{
		Temperature = Pressure = Humidity = 0;
	}
}
