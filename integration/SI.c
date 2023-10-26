#include <wiringPi.h>
#include <wiringPiSPI.h>

#include <stdint.h>

#include "SI.h"

unsigned int dht_data[5];   // 온습도 센서를 읽을 때 사용되는 데이터 배열
unsigned char buffer[3];    // ADC와 통신하기 위해 사용되는 버퍼

// SPI를 setup
// SPI에 사용되는 CS, LED를 OUTPUT모드로 설정
// 성공시 1, 실패시 -1 반환
int setWiringPi(){
    if(wiringPiSetup() == -1) return -1;
    if(wiringPiSPISetup(0, SPI_SPEED) == -1) return -1;
    pinMode(CS, OUTPUT);
    pinMode(LED, OUTPUT);

    return 1;
}

// ADC 채널에 따른 주소를 buffer에 설정
void setConfigBit(int channel)
{
    buffer[0] = 0b110; // 채널에 따른 시작 바이트 설정
    buffer[1] = channel<<6;
    buffer[2] = 0;
}

// 미세먼지 센서로부터 아날로그 값을 읽어와 반환
int readDustSensor(){
    int adc = 0;

    setConfigBit(ADC_CHANNEL3);

    digitalWrite(LED,LOW);
    delayMicroseconds(SAMPLINGTIME); //미세먼지를 측정하기위한 샘플링 시간
    
    digitalWrite(CS, LOW); // CS/SHDN bit을 0으로 만들어야 값을 얻을 수 있다.
    wiringPiSPIDataRW(0, buffer, 3); // SPI 장치와 데이터 교환
    digitalWrite(CS, HIGH); // CS/SHDN bit : CHANNEL 바뀔 때 1로 setting
    
    delayMicroseconds(DELTA);
    digitalWrite(LED,HIGH);
    adc = ((buffer[1] & 0x0F) << 8) + buffer[2]; // 데이터 교환 후 ADC 값 버퍼 추출, 반환
    return adc;
}

// 수위 센서로부터 값을 읽어와 리턴
int readWaterLevelSensor(){ 
    int adc = 0;

    setConfigBit(ADC_CHANNEL4);
    digitalWrite(CS, LOW); // CS/SHDN bit을 0으로 만들어야 값을 얻을 수 있다.
    wiringPiSPIDataRW(0, buffer, 3); // SPI 장치와 데이터 교환
    digitalWrite(CS, HIGH); // CS/SHDN bit : CHANNEL 바뀔 때 1로 setting

    adc = ((buffer[1] & 0x0F) << 8) + buffer[2]; // 데이터 교환 후 ADC 값 버퍼 추출, 반환
    return adc;
}

// 조도 센서로부터 값을 읽어와 리턴
int readLightSensor(){ // 지정된 SPI 채널에서 ADC 값을 읽습니다.
    int adc = 0;

    setConfigBit(ADC_CHANNEL1);
    digitalWrite(CS, LOW); // CS/SHDN bit을 0으로 만들어야 값을 얻을 수 있다.
    wiringPiSPIDataRW(0, buffer, 3); // SPI 장치와 데이터 교환
    digitalWrite(CS, HIGH); // CS/SHDN bit : CHANNEL 바뀔 때 1로 setting

    adc = ((buffer[1] & 0x0F) << 8) + buffer[2]; // 데이터 교환 후 ADC 값 버퍼 추출, 반환
    return adc;
}

// 빗물감지 센서로부터 값을 읽어와 리턴
int readRainSensor(){ // 지정된 SPI 채널에서 ADC 값을 읽습니다.
    int adc = 0;

    setConfigBit(ADC_CHANNEL2);
    digitalWrite(CS, LOW); // CS/SHDN bit을 0으로 만들어야 값을 얻을 수 있다.
    wiringPiSPIDataRW(0, buffer, 3); // SPI 장치와 데이터 교환
    digitalWrite(CS, HIGH); // CS/SHDN bit : CHANNEL 바뀔 때 1로 setting

    adc = ((buffer[1] & 0x0F) << 8) + buffer[2]; // 데이터 교환 후 ADC 값 버퍼 추출, 반환
    return adc;
}

// 온습도센서로부터 온도 정수값만 리턴
int readDHTSensor(unsigned char gpio_pin)                    // dht데이터 읽기 함수
{
	uint8_t laststate	= HIGH;          // DHT핀의 상태 저장용 변수(현재 신호가 HIGH인지 LOW인지 확인하기 위한 용도)
	uint8_t counter		= 0;             // 신호의 길이를 측정하기 위한 카운터 변수
	uint8_t j			= 0, i;          // 40개의 신호 인덱스 용 변수

	dht_data[0] = dht_data[1] = dht_data[2] = dht_data[3] = dht_data[4] = 0;    //초기 데이터 값은 0으로 지정

	/* DHT11센서와의 통신을 개시하기 위해 DATA핀을 18ms동안 LOW로 출력 */
	pinMode( gpio_pin, OUTPUT );
	digitalWrite( gpio_pin, LOW );
	delay( 18 );

	/* 핀을 입력모드로 설정해서 DHT11로 부터 응답을 기다림 */
	pinMode( gpio_pin, INPUT );

	/* DHT11에서 오는 신호 검출 및 데이터비트 추출 */
	for ( i = 0; i < MAX_TIMINGS; i++ )       // 총 85번 동안 신호를 확인
	{
		counter = 0;                           // 초기 카운터는 0
		while ( digitalRead( gpio_pin ) == laststate ) //DHT핀의 신호를 읽어서 현재 지정한 DATA핀 신호와 같은 동안==즉 신호의 변환이 없는 동안
		{
			counter++;                              // 카운터 변수 1 증가
			delayMicroseconds( 1 );                 // 1uS(마이크로초) 동안 대기
			if ( counter == 255 )                  // 카운터가 255까지 도달하면, 즉 너무 오래 동안 대기하면==오류가 생겼다는 의미 임
			{
				break;                              // 카운터 세기 중지
			}
		}
		laststate = digitalRead( gpio_pin );       // 현재 핀 상태 저장

		if ( counter == 255 )                     // 카운터가 255이상 도달했다면, 데이터비트 수신 중지== for문 밖으로 나가서 처음부터 새로 받겠다는 의미임
			break;

		/* 첫번째 3개의 신호는 무시하고 짝수번째에 해당하는 신호길이를 읽어 0인지 1인지에 따라 온습도 변수에 저장
           첫번째 3개의 신호는 DHT11의 응답 용 신호로 실제 데이터 길이를 통해 정보를 수신하는 값이 아니므로 무시함.
           짝수만 추출하는 이유는 짝수 번째의 신호 길이에 따라 해당 신호가 0을 의미하는지 1을 의미하는지를 나타냄. 
         */     
		if ( (i >= 4) && (i % 2 == 0) )   // 4번째 이후의 신호이면서 짝수번째 신호라면 
		{
			/* 가각의 데이터 비트를 온도 및 습도 변수에 하나씩 넣어줌 */
			dht_data[j / 8] <<= 1;                    // 이진수의 자리수를 하나씩 옆으로 이동시킴
			if ( counter > 40 )                    // 카운터의 값이 16보다 크다면, 즉 신호의 길이가 길어서 비트 1로 인식된다면
				dht_data[j / 8] |= 1;                  // 해당 비트는 1을 넣어줌
			j++;                                 // 다음 데이터를 위해 인덱스 값을 하나 증가 시킴
		}
	}

	/*
	 * 40비트를 다 확인했다면 (8비트 x 5 ) 체크섬 데이터와 오류체크
	 * 오류가 없으면 0을 리턴
     */
	if ( (j >= 40) && (dht_data[4] == ( (dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3]) & 0xFF) ))
        return dht_data[2];

    // 오류가 있다면 -1 리턴
    return -1;
}