#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>

int main() {

    int setting = system("sudo chmod 666 /dev/ttyS0");

    if(setting == -1)
    {
        perror("Error - Unable to change mode\n");
        return 1;
    }
    int uart_fd = -1;
    struct termios options;

    // UART 포트 열기
    uart_fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_fd == -1) {
        perror("Error - Unable to open UART");
        return 1;
    }

    // 통신 설정 가져오기
    tcgetattr(uart_fd, &options);
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD; // Baud rate 설정: 115200, 8-bit, 지역 연결, 데이터 수신 허용
    options.c_iflag = IGNPAR; // Parity 오류 무시
    options.c_oflag = 0;
    options.c_lflag = 0;

    // 블로킹 동작 설정
    // options.c_cc[VMIN] = 0; // 5개의 문자를 수신해야 반환
    // options.c_cc[VTIME] = 3; // No Timeout

    // 통신 설정 적용하기
    tcflush(uart_fd, TCIFLUSH);
    tcsetattr(uart_fd, TCSANOW, &options);

    while(1)
    {
        // 데이터 수신
        char rx_buffer[256] = {0};
        int rx_length = read(uart_fd, rx_buffer, sizeof(rx_buffer) - 1); // -1을 해줘서 NULL 종료 문자 공간 확보
        // if (rx_length < 0) {
        // perror("UART RX error");
        // } else if (rx_length == 0) {
        // printf("No data received.\n");
        // } else {
        if(rx_length > 0){
            rx_buffer[rx_length] = '\0'; // 문자열 종료
            printf("Received %d bytes: %s\n", rx_length, rx_buffer);
        }
        // printf("Run rx_length : %d , bytes : %s\n", rx_length, rx_buffer);
    }

    // UART 포트 닫기
    close(uart_fd);
    printf("End of Receive!\n");
    return 0;
}
