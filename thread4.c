#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>

// 전역 변수:
// stop_thread1와 stop_thread2: 각각의 스레드를 종료하기 위한 플래그로 사용됩니다.
volatile int stop_thread1 = 0;  // Thread 1을 종료시키기 위한 변수
volatile int stop_thread2 = 0;  // Thread 2를 종료시키기 위한 변수

//signal_handler 함수:
// 시그널 핸들러로, SIGUSR1 및 SIGUSR2 시그널을 처리합니다.
// SIGUSR1을 받으면 stop_thread1을 1로 설정하고, SIGUSR2를 받으면 stop_thread2를 1로 설정합니다.
void signal_handler(int signum) {
    if (signum == SIGUSR1) {
        stop_thread1 = 1;
    } else if (signum == SIGUSR2) {
        stop_thread2 = 1;
    }
}
// file_output_thread 함수:

// file.txt 파일을 쓰기 모드로 연다. 파일이 있으면 내용에 이어서 쓰고, 없으면 새로 만든다.
// 파일을 열지 못하면 에러 메시지를 출력한다.
// 무한 루프에서 "Writing to file: [숫자]" 라는 메시지를 파일에 계속 쓴다.
// 종료 시점에 파일을 닫고 종료 메시지를 출력한다.
// 주의: 현재 이 스레드는 무한 루프를 탈출할 수 있는 로직이 없습니다.
void* file_output_thread(void* arg) {
    int fd = open("../file.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd == -1) {
        perror("Failed to open file");
        return NULL;
    }

    while(1) {  // 무한 루프
        for (int i = 0; i < 10; ++i) {
            dprintf(fd, "Writing to file: %d\n", i);
            sleep(1);
        }
    }
    close(fd);
    printf("File output thread terminated.\n");
    return NULL;
}

// uart_send_thread 함수:

// /dev/serial0 UART 디바이스를 비동기 모드로 연다.
// 디바이스를 열지 못하면 에러 메시지를 출력한다.
// UART 통신을 위한 설정을 한다.
// 무한 루프에서 "UART Message"라는 메시지를 UART로 계속 보낸다.
// 종료 시점에 디바이스를 닫고 종료 메시지를 출력한다.
// 주의: 현재 이 스레드도 무한 루프를 탈출할 수 있는 로직이 없습니다.

void* uart_send_thread(void* arg) {
    int fd = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY); 
    if (fd == -1) {
        perror("Failed to open UART");
        return NULL;
    }

    struct termios options;
    tcgetattr(fd, &options);
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &options);

    while(1) {  // 무한 루프
        for (int i = 0; i < 10; ++i) {
            int ret = write(fd, "UART Message", 12);
            if (ret < 0) {
                perror("Failed to write to UART");
                break;
            }
            sleep(1);
        }
    }
    close(fd);
    printf("File output thread terminated.\n");
    return NULL;
}

int main(void) {

    // main 함수:

    // 두 개의 스레드 (thread1과 thread2)를 정의한다.
    // 시그널 핸들러를 설정하여 SIGUSR1 및 SIGUSR2 시그널을 signal_handler로 처리하도록 한다.
    // 두 개의 스레드를 생성하여 실행한다.
    // 1초 동안 메인 스레드를 잠재웁니다.
    // 메인 스레드에서 메시지를 출력한다.
    // 각각의 스레드에 SIGUSR1 및 SIGUSR2 시그널을 전송하여 해당 스레드를 종료하려고 시도한다.
    // 종료 메시지를 출력하고 프로그램을 종료한다.
    pthread_t thread1, thread2;

    // 시그널 핸들러 설정
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    pthread_create(&thread1, NULL, file_output_thread, NULL);
    pthread_create(&thread2, NULL, uart_send_thread, NULL);

    sleep(1);  // 예제를 위해 10초 동안 메인 스레드를 잠재웁니다.

    printf("main thread\n");
    // 스레드에 시그널 보내기
    pthread_kill(thread1, SIGUSR1);
    pthread_kill(thread2, SIGUSR2);
    printf("End\n");

    // pthread_join(thread1, NULL);
    // pthread_join(thread2, NULL);
    printf("End\n");

    return 0;
}