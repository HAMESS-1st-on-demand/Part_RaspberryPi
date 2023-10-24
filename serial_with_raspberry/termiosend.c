#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

int main() {
    // chmod를 변경하는 코드
    int setting = system("sudo chmod 666 /dev/ttyAMA0");

    if(setting == -1)
    {
        perror("Error - Unable to change mode\n");
        return 1;
    }
    int uart_fd = -1; // uart filedescriptor 설정 
    struct termios options; 

    // UART 포트 열기
    uart_fd = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY); // | O_NDELAY); // O_RDWR : 읽고 쓰기, O_NOCTTY : 키보드 중단 신호 영향, O_NDELAY : 수신 대기 차단 
    if (uart_fd == -1) {
        perror("Error - Unable to open UART");
        return 1;
    }

    // 통신 설정 가져오기
    tcgetattr(uart_fd, &options);
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD; // Baud rate 설정: 9600, 5-bit, 지역 연결, 데이터 수신 허용 (bit 5 ~ 8)
    options.c_iflag = IGNPAR; // Parity 오류 무시
    options.c_oflag = 0;
    options.c_lflag = 0;

    // 블로킹 동작 설정
    options.c_cc[VMIN] = 5; // 5개의 문자를 수신해야 반환
    options.c_cc[VTIME] = 0; // No Timeout

    // 통신 설정 적용하기
    tcflush(uart_fd, TCIFLUSH);
    tcsetattr(uart_fd, TCSANOW, &options);

    int vv = 0;
    // 데이터 송신
    while(1)
    {
        char tx_buffer[256] = {0};
        sprintf(tx_buffer, "Heeee owow %d!\n", vv);
        printf("%s\n", tx_buffer);
        if (write(uart_fd, tx_buffer, strlen(tx_buffer)) < 0) {
            perror("UART TX error");
        }
        if(++vv == 300)break;
    }

    // UART 포트 닫기
    close(uart_fd);

    printf("End of transmit!\n");

    return 0;
}