#include "../drivers/BSP/DHT/DHT.h"

#define lineDown() 		HAL_GPIO_WritePin(sensor->DHT_Port, sensor->DHT_Pin, GPIO_PIN_RESET)
#define lineUp()		HAL_GPIO_WritePin(sensor->DHT_Port, sensor->DHT_Pin, GPIO_PIN_SET)
#define getLine()		(HAL_GPIO_ReadPin(sensor->DHT_Port, sensor->DHT_Pin) == GPIO_PIN_SET)
#define Delay(d)		HAL_Delay(d)

static void goToOutput(DHT_sensor *sensor) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

  //§±§à §å§Þ§à§Ý§é§Ñ§ß§Ú§ð §ß§Ñ §Ý§Ú§ß§Ú§Ú §Ó§í§ã§à§Ü§Ú§Û §å§â§à§Ó§Ö§ß§î
  lineUp();

  //§¯§Ñ§ã§ä§â§à§Û§Ü§Ñ §á§à§â§ä§Ñ §ß§Ñ §Ó§í§ç§à§Õ 
  GPIO_InitStruct.Pin = sensor->DHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; 	//§°§ä§Ü§â§í§ä§í§Û §ã§ä§à§Ü
  GPIO_InitStruct.Pull = sensor->pullUp;		//§±§à§Õ§ä§ñ§Ø§Ü§Ñ §Ü §á§Ú§ä§Ñ§ß§Ú§ð

  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; //§£§í§ã§à§Ü§Ñ§ñ §ã§Ü§à§â§à§ã§ä§î §â§Ñ§Ò§à§ä§í §á§à§â§ä§Ñ
  HAL_GPIO_Init(sensor->DHT_Port, &GPIO_InitStruct);
}

static void goToInput(DHT_sensor *sensor) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  //§¯§Ñ§ã§ä§â§à§Û§Ü§Ñ §á§à§â§ä§Ñ §ß§Ñ §Ó§ç§à§Õ 
  GPIO_InitStruct.Pin = sensor->DHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = sensor->pullUp;		//§±§à§Õ§ä§ñ§Ø§Ü§Ñ §Ü §á§Ú§ä§Ñ§ß§Ú§ð
  HAL_GPIO_Init(sensor->DHT_Port, &GPIO_InitStruct);
}

DHT_data DHT_getData(DHT_sensor *sensor) {
	DHT_data data = {-128.0f, -128.0f};
	
	#if DHT_POLLING_CONTROL == 1
	/* §°§Ô§â§Ñ§ß§Ú§é§Ö§ß§Ú§Ö §á§à §é§Ñ§ã§ä§à§ä§Ö §à§á§â§à§ã§Ñ §Õ§Ñ§ä§é§Ú§Ü§Ñ */
	//§°§á§â§Ö§Õ§Ö§Ý§Ö§ß§Ú§Ö §Ú§ß§ä§Ö§â§Ó§Ñ§Ý§Ñ §à§á§â§à§ã§Ñ §Ó §Ù§Ñ§Ó§Ú§ã§Ú§Þ§à§ã§ä§Ú §à§ä §Õ§Ñ§ä§é§Ú§Ü§Ñ
	uint16_t pollingInterval;
	if (sensor->type == DHT11) {
		pollingInterval = DHT_POLLING_INTERVAL_DHT11;
	} else {
		pollingInterval = DHT_POLLING_INTERVAL_DHT22;
	}

	//§¦§ã§Ý§Ú §Ú§ß§ä§Ö§â§Ó§Ñ§Ý §Þ§Ñ§Ý§Ö§ß§î§Ü§Ú§Û, §ä§à §Ó§à§Ù§Ó§â§Ñ§ä §á§à§ã§Ý§Ö§Õ§ß§Ö§Ô§à §å§Õ§Ñ§é§ß§à§Ô§à §Ù§ß§Ñ§é§Ö§ß§Ú§ñ
	if ((HAL_GetTick() - sensor->lastPollingTime < pollingInterval) && sensor->lastPollingTime != 0) {
		data.hum = sensor->lastHum;
		data.temp = sensor->lastTemp;
		return data;
	}
	sensor->lastPollingTime = HAL_GetTick()+1;
	#endif

	/* §©§Ñ§á§â§à§ã §Õ§Ñ§ß§ß§í§ç §å §Õ§Ñ§ä§é§Ú§Ü§Ñ */
	//§±§Ö§â§Ö§Ó§à§Õ §á§Ú§ß§Ñ "§ß§Ñ §Ó§í§ç§à§Õ"
	goToOutput(sensor);
	//§°§á§å§ã§Ü§Ñ§ß§Ú§Ö §Ý§Ú§ß§Ú§Ú §Õ§Ñ§ß§ß§í§ç §ß§Ñ 18 §Þ§ã
	lineDown();
	Delay(18);
	//§±§à§Õ§ì§×§Þ §Ý§Ú§ß§Ú§Ú, §á§Ö§â§Ö§Ó§à§Õ §á§à§â§ä§Ñ "§ß§Ñ §Ó§ç§à§Õ"
	lineUp();
	goToInput(sensor);


	#ifdef DHT_IRQ_CONTROL
	//§£§í§Ü§Ý§ð§é§Ö§ß§Ú§Ö §á§â§Ö§â§í§Ó§Ñ§ß§Ú§Û, §é§ä§à§Ò§í §ß§Ú§é§ä§à §ß§Ö §Þ§Ö§ê§Ñ§Ý§à §à§Ò§â§Ñ§Ò§à§ä§Ü§Ö §Õ§Ñ§ß§ß§í§ç
	__disable_irq();
	#endif
	/* §°§Ø§Ú§Õ§Ñ§ß§Ú§Ö §à§ä§Ó§Ö§ä§Ñ §à§ä §Õ§Ñ§ä§é§Ú§Ü§Ñ */
	uint16_t timeout = 0;
	//§°§Ø§Ú§Õ§Ñ§ß§Ú§Ö §ã§á§Ñ§Õ§Ñ
	while(getLine()) {
		timeout++;
		if (timeout > DHT_TIMEOUT) {
			#ifdef DHT_IRQ_CONTROL
			__enable_irq();
			#endif
			//§¦§ã§Ý§Ú §Õ§Ñ§ä§é§Ú§Ü §ß§Ö §à§ä§à§Ù§Ó§Ñ§Ý§ã§ñ, §Ù§ß§Ñ§é§Ú§ä §Ö§Ô§à §ä§à§é§ß§à §ß§Ö§ä
			//§°§Ò§ß§å§Ý§Ö§ß§Ú§Ö §á§à§ã§Ý§Ö§Õ§ß§Ö§Ô§à §å§Õ§Ñ§é§ß§à§Ô§à §Ù§ß§Ñ§é§Ö§ß§Ú§ñ, §é§ä§à§Ò§í
			//§ß§Ö §á§à§Ý§å§é§Ñ§ä§î §æ§Ñ§ß§ä§à§Þ§ß§í§Ö §Ù§ß§Ñ§é§Ö§ß§Ú§ñ
			sensor->lastHum = -128.0f;
			sensor->lastTemp = -128.0f;

			return data;
		}
	}
	timeout = 0;
	//§°§Ø§Ú§Õ§Ñ§ß§Ú§Ö §á§à§Õ§ì§×§Þ§Ñ
	while(!getLine()) {
		timeout++;
		if (timeout > DHT_TIMEOUT) {
			#ifdef DHT_IRQ_CONTROL
			__enable_irq();
			#endif
			//§¦§ã§Ý§Ú §Õ§Ñ§ä§é§Ú§Ü §ß§Ö §à§ä§à§Ù§Ó§Ñ§Ý§ã§ñ, §Ù§ß§Ñ§é§Ú§ä §Ö§Ô§à §ä§à§é§ß§à §ß§Ö§ä
			//§°§Ò§ß§å§Ý§Ö§ß§Ú§Ö §á§à§ã§Ý§Ö§Õ§ß§Ö§Ô§à §å§Õ§Ñ§é§ß§à§Ô§à §Ù§ß§Ñ§é§Ö§ß§Ú§ñ, §é§ä§à§Ò§í
			//§ß§Ö §á§à§Ý§å§é§Ñ§ä§î §æ§Ñ§ß§ä§à§Þ§ß§í§Ö §Ù§ß§Ñ§é§Ö§ß§Ú§ñ
			sensor->lastHum = -128.0f;
			sensor->lastTemp = -128.0f;

			return data;
		}
	}
	timeout = 0;
	//§°§Ø§Ú§Õ§Ñ§ß§Ú§Ö §ã§á§Ñ§Õ§Ñ
	while(getLine()) {
		timeout++;
		if (timeout > DHT_TIMEOUT) {
			#ifdef DHT_IRQ_CONTROL
			__enable_irq();
			#endif
			return data;
		}
	}
	
	/* §¹§ä§Ö§ß§Ú§Ö §à§ä§Ó§Ö§ä§Ñ §à§ä §Õ§Ñ§ä§é§Ú§Ü§Ñ */
	uint8_t rawData[5] = {0,0,0,0,0};
	for(uint8_t a = 0; a < 5; a++) {
		for(uint8_t b = 7; b != 255; b--) {
			uint16_t hT = 0, lT = 0;
			//§±§à§Ü§Ñ §Ý§Ú§ß§Ú§ñ §Ó §ß§Ú§Ù§Ü§à§Þ §å§â§à§Ó§ß§Ö, §Ú§ß§Ü§â§Ö§Þ§Ö§ß§ä §á§Ö§â§Ö§Þ§Ö§ß§ß§à§Û lT
			while(!getLine() && lT != 65535) lT++;
			//§±§à§Ü§Ñ §Ý§Ú§ß§Ú§ñ §Ó §Ó§í§ã§à§Ü§à§Þ §å§â§à§Ó§ß§Ö, §Ú§ß§Ü§â§Ö§Þ§Ö§ß§ä §á§Ö§â§Ö§Þ§Ö§ß§ß§à§Û hT
			timeout = 0;
			while(getLine()&& hT != 65535) hT++;
			//§¦§ã§Ý§Ú hT §Ò§à§Ý§î§ê§Ö lT, §ä§à §á§â§Ú§ê§Ý§Ñ §Ö§Õ§Ú§ß§Ú§è§Ñ
			if(hT > lT) rawData[a] |= (1<<b);
		}
	}

    #ifdef DHT_IRQ_CONTROL
	//§£§Ü§Ý§ð§é§Ö§ß§Ú§Ö §á§â§Ö§â§í§Ó§Ñ§ß§Ú§Û §á§à§ã§Ý§Ö §á§â§Ú§×§Þ§Ñ §Õ§Ñ§ß§ß§í§ç
	__enable_irq();
    #endif

	/* §±§â§à§Ó§Ö§â§Ü§Ñ §è§Ö§Ý§à§ã§ä§ß§à§ã§ä§Ú §Õ§Ñ§ß§ß§í§ç */
	if((uint8_t)(rawData[0] + rawData[1] + rawData[2] + rawData[3]) == rawData[4]) {
		//§¦§ã§Ý§Ú §Ü§à§ß§ä§â§à§Ý§î§ß§Ñ§ñ §ã§å§Þ§Þ§Ñ §ã§à§Ó§á§Ñ§Õ§Ñ§Ö§ä, §ä§à §Ü§à§ß§Ó§Ö§â§ä§Ñ§è§Ú§ñ §Ú §Ó§à§Ù§Ó§â§Ñ§ä §á§à§Ý§å§é§Ö§ß§ß§í§ç §Ù§ß§Ñ§é§Ö§ß§Ú§Û
		if (sensor->type == DHT22) {
			data.hum = (float)(((uint16_t)rawData[0]<<8) | rawData[1])*0.1f;
			//§±§â§à§Ó§Ö§â§Ü§Ñ §ß§Ñ §à§ä§â§Ú§è§Ñ§ä§Ö§Ý§î§ß§à§ã§ä§î §ä§Ö§Þ§á§Ö§â§Ñ§ä§å§â§í
			if(!(rawData[2] & (1<<7))) {
				data.temp = (float)(((uint16_t)rawData[2]<<8) | rawData[3])*0.1f;
			}	else {
				rawData[2] &= ~(1<<7);
				data.temp = (float)(((uint16_t)rawData[2]<<8) | rawData[3])*-0.1f;
			}
		}
		if (sensor->type == DHT11) {
			data.hum = (float)rawData[0];
			data.temp = (float)rawData[2];
		}
	}
	
	#if DHT_POLLING_CONTROL == 1
	sensor->lastHum = data.hum;
	sensor->lastTemp = data.temp;
	#endif

	return data;	
}

