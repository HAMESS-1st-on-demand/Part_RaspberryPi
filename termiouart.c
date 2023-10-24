#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

#define DEVICE "/dev/serial0"  // 이 부분은 라즈베리파이에서 사용하는 UART 디바이스에 따라 변경될 수 있습니다.
#define BAUDRATE B9600
#define TIMEOUT 3
#define MAX_TRIES 10

int main() {
    int uart_fd, count; // uart 진행할 file descriptor
    struct termios options;
    char send_buf[256] = "Hello Arduino!";
    char recv_buf[256]; 

    int setting = system("sudo chmod 666 /dev/serial0"); // system setting으로 설정

    if(setting == -1)
    {
        perror("Error - Unable to change mode\n");
        return -1;
    }
    printf("hi\n");
    uart_fd = open(DEVICE, O_RDWR | O_NOCTTY);
    if (uart_fd == -1) {
        printf("Error opening UART.\n");
        return -1;
    }
    printf("hello\n");
    tcgetattr(uart_fd, &options);
    cfsetispeed(&options, BAUDRATE);
    cfsetospeed(&options, BAUDRATE);

    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cc[VTIME] = 2;

    tcflush(uart_fd, TCIFLUSH);
    tcsetattr(uart_fd, TCSANOW, &options);

    printf("hhi\n");
    FILE *logFile = fopen("../log.txt", "a");
    for (int i = 0; i < MAX_TRIES; i++) {
        write(uart_fd, send_buf, strlen(send_buf));
        printf("Attemp %d : Sent message to Arduino. \n", i+1);
        fprintf(logFile, "Attempt %d: Sent message to Arduino.\n", i + 1);

        sleep(TIMEOUT);  // 여기서 TIMEOUT은 초 단위로 대기하는 시간입니다. 필요에 따라 조정하세요.
        
        int bytes_read = read(uart_fd, recv_buf, sizeof(recv_buf) - 1);
        if (bytes_read > 0) {
            recv_buf[bytes_read] = '\0';
            fprintf(logFile, "Received: %s\n", recv_buf);
            if (strstr(recv_buf, "ACK")) {
                printf("Received ACK from Arduino!\n");
                break;
            }
        } else {
            printf("%s\n", "No response!");
            fprintf(logFile, "No response from Arduino.\n");
        }
    }

    fclose(logFile);
    close(uart_fd);
    return 0;
}
