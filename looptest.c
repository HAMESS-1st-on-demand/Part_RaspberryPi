#include <stdio.h>
#include <string.h>
#include <wiringPi.h> // delay()
#include <wiringSerial.h>

#define BAUD 9600

int main(void) {
	int fd;
	unsigned char asc, rec;
	
	// 시리얼 오픈
	if((fd = serialOpen("/dev/ttyS0", BAUD)) < 0) {
		printf("Device file open error!! use sudo ...\n");
		return 1;
	}
	
	printf("[UART testing....... loopback]\n");
	
	asc = 65; // 보낼 아스키 코드
	while(1)
	{
		printf("Transmitting ... %d ", asc);
		// 시리얼에 아스키코드를 보낸다.
		serialPutchar(fd, asc);
 		// 딜레이 
		delay(10);
		// 만약 Rx에 신호가 있다면 실행
		if(serialDataAvail(fd))
		{
			// 보낸 아스키코드를 받아 rec에 넣는다.
			rec = serialGetchar(fd);
			// 숫자로 출력, 문자로 출력
			printf("===> Received : %d %c\n", rec, rec);
			// 버퍼한번 비워준다.
			serialFlush(fd);
		}
		// 딜레이
		delay(300);
		asc++;
 }
 return 0;
}