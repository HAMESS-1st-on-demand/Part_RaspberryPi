#include <stdio.h>
#include <SI.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>

//센서별 임계값
#define LIGHT_TH 200
#define RAIN_TH 1700
#define WATERLEV_TH 2800
#define DUST_TH 2000



int main(void){
    int now;
    int lightTime, rainTime, dustTime, waterTime, temperTime;
    //센서별 체크 주기
    int lightPeriod = 500;
    int rainPeriod = 1000;
    int dustPeriod = 1500;
    int waterPeriod = 2000;
    int temperPeriod = 3000;

    //wiringPi init함수
    if(setWiringPi()==-1) {
        printf("Failed to setup WiringPi\n");
        return -1;
    }

    //각각 시간을 추척하기위한 초기화
    now = lightTime = rainTime = dustTime = waterTime = temperTime = millis();
    unsigned char buffer = 0;
    while(1){
        now = millis();

        //각 주기 이상의 시간이 되면 센서값 측정 후 비교
        if(now - waterTime>=waterPeriod) //수위 감지센서
        {
            waterTime = now;
            int waterLev = readWaterLevelSensor();
            printf("[%d] waterLev = %d\n", now/100,waterLev);

            if(readWaterLevelSensor()>WATERLEV_TH){ //판단
                printf("썬루프 열어\n");
                buffer |= 1<<4; //buffer: 10000
            }
        }

        if(now - lightTime>=lightPeriod) //조도 센서
        {
            lightTime = now;
            int light = readLightSensor();
            printf("[%d] light = %d\n", now/100,light);

            if(readLightSensor()<LIGHT_TH) { //판단
                printf("썬루프 어둡게\n");
            }
        }

        if(now - rainTime>=rainPeriod) //비 센서
        {
            rainTime =now;
            int rain = readRainSensor();
            printf("[%d] rain = %d\n", now/100,rain);

            if(readRainSensor()<RAIN_TH){ //판단
                printf("썬루프 닫아\n");
                buffer |= 1<<3; //buffer: 01000
            }
        }

        if(now - dustTime>=dustPeriod) //미세먼지센서
        {
            dustTime=now;
            int dust = readDustSensor();
            printf("[%d] dust = %d\n", now/100,dust);
            
            if(readDustSensor()>DUST_TH){ //판단
                printf("썬루프 닫아\n");
                buffer |= 1<<2; //buffer: 00100
            }
        }

        if(now - temperTime>=temperPeriod) //온습도 센서
        {
            temperTime = now;
            int temper1 = readDHTSensor(DHT_PIN1);        // 10번 핀으로부터 데이터를 읽음 -> 실내온도
            int temper2 = readDHTSensor(DHT_PIN2);        // 11번 핀으로부터 데이터를 읽음 -> 실외 온도

            if(temper1 ==-1||temper2 ==-1){
                printf("DHT data not good\n");
                continue;
            }
            printf("[%d] temper1 = %d, temper2 = %d \n", now/10000,temper1,temper2);

            if(temper1<temper2&&temper2>30){ //판단
                printf("썬루프 닫아");
                buffer |= 1<<1; //buffer: 00010
            }
            else if(temper1>temper2&&temper1>40){
                printf("썬루프 열어");
                buffer |= 1; 
            }
        }
    }

    return 0;
}