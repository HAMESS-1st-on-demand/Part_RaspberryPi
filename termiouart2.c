#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h> // gettimeofday()를 사용하기 위한 헤더

#define DEVICE "/dev/serial0"
#define BAUDRATE B9600
#define TIMEOUT 1  // 1초
#define MAX_TRIES 5

long get_microseconds() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

int main() {
    int setting = system("sudo chmod 666 /dev/serial0"); // system setting으로 설정

    if(setting == -1)
    {
        perror("Error - Unable to change mode\n");
        return -1;
    }
    int uart_fd, count;
    struct termios options;
    char send_buf[256] = "Hello Arduino!";
    char recv_buf[256];

    uart_fd = open(DEVICE, O_RDWR | O_NOCTTY);
    if (uart_fd == -1) {
        printf("Error opening UART.\n");
        return -1;
    }

    tcgetattr(uart_fd, &options);
    cfsetispeed(&options, BAUDRATE);
    cfsetospeed(&options, BAUDRATE);

    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    tcflush(uart_fd, TCIFLUSH);
    tcsetattr(uart_fd, TCSANOW, &options);

    long start_time = get_microseconds();

    FILE *logFile = fopen("../log.txt", "a");
    for (int i = 0; i < MAX_TRIES; i++) {
        write(uart_fd, send_buf, strlen(send_buf));

        fd_set set;
        struct timeval timeout;
        
        FD_ZERO(&set);
        FD_SET(uart_fd, &set);

        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;

        int rv = select(uart_fd + 1, &set, NULL, NULL, &timeout);
        if (rv == -1) {
            perror("select()");
        } else if (rv == 0) {
            fprintf(logFile, "Timeout occurred! No data after %d seconds.\n", TIMEOUT);
        } else {
            int bytes_read = read(uart_fd, recv_buf, sizeof(recv_buf) - 1);
            if (bytes_read > 0) {
                recv_buf[bytes_read] = '\0';
                if (strstr(recv_buf, "ACK")) {
                    long end_time = get_microseconds();
                    long elapsed_time = end_time - start_time; // microseconds

                    printf("Received ACK from Arduino in %ld microseconds!\n", elapsed_time);
                    fprintf(logFile, "Attempt %d: Sent message and received ACK in %ld microseconds.\n", i + 1, elapsed_time);
                    break;
                }
            }
        }
    }

    fclose(logFile);
    close(uart_fd);
    return 0;
}
