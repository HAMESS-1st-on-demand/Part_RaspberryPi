// ADC analog input 채널
#define ADC_CHANNEL1 0      // 조도센서 채널
#define ADC_CHANNEL2 1      // 빗물감지 센서 채널
#define ADC_CHANNEL3 2      // 미세먼지 센서 채널
#define ADC_CHANNEL4 3      // 수위 센서 채널

#define CS 1            // ADC CS 핀

#define SPI_SPEED 1000000 // 1Mhz

#define DHT_PIN1    10      // 온습도 센서1 10번 pin
#define DHT_PIN2    11      // 온습도 센서2 11번 pin

//For DustSensor
#define SAMPLINGTIME 280
#define LED 0 //미세먼지 측정LED
#define DELTA 40

#define MAX_TIMINGS	85        // 최대 신호 추출 개수
       
// SPI를 setup
// SPI에 사용되는 CS, LED를 OUTPUT모드로 설정
// 성공시 1, 실패시 -1 반환
int setWiringPi();

// ADC 채널에 따른 주소를 buffer에 설정
void setConfigBit(int channel);

// 미세먼지 센서로부터 아날로그 값을 읽어와 반환
int readDustSensor();

// 수위 센서로부터 값을 읽어와 리턴
int readWaterLevelSensor();

// 조도 센서로부터 값을 읽어와 리턴
int readLightSensor();

// 빗물감지 센서로부터 값을 읽어와 리턴
int readRainSensor();

// 온습도센서로부터 온도 정수값만 리턴
int readDHTSensor(unsigned char gpio_pin);