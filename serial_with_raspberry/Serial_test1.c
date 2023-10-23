#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringSerial.h>

// Find Serial Device on Raspberry with $ ls /dev/tty*
// ARDUINO_UNO /dev/ttyACMO
// FTDI_PROGRAMMER "/dev/ttyUSB0"
// HARDWARE_UART = '/dev/ttyAMA0'

char device[] = "/dev/ttyS0";

int fd; // filedescripter
unsigned long baud = 9600;
unsigned long time = 0;

void setup()
{
    printf("%s\n", "Raspberry Start!");
    fflush(stdout);

    int setting = system("sudo chmod 666 /dev/ttyS0");

    if(setting == -1)
    {
        perror("Error - Unable to change mode\n");
        return 1;
    }

    // get fd
    if((fd = serialOpen(device, baud)) < 0){
        fprintf(stderr, "Unable to open serial device : %s\n", strerror(errno));
        exit(1);
    }

    // setup GPIO in wiringPi Mode
    if(wiringPiSetup() == -1){
        fprintf(stdout, "Unable to start wiringPi: %s\n", strerror(errno));
        exit(1);
    }
    return;
}

void loop()
{
    // 현재 시간 (millis) 에서 이전의 time을 뺀 값
    // if(millis()-time >= 1000)
    // {
    //     printf("Hello!%d\n", millis());
    //     serialPuts(fd, "Pong\n");
    //     serialPutchar(fd, 65);
    //     time = millis();
    // }

    if(serialDataAvail(fd)){ // 시리얼 포트에 사용 가능한 데이터가 있는지 확인
        printf("Hi!%d\n", millis());
        char newChar = serialGetchar(fd);
        printf("%c", newChar);
        fflush(stdout);
    }
}

int main()
{
    setup();
    while(1)loop();
    return 0;
}
