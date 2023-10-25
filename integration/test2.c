#include <stdio.h>
#include <string.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <time.h>

#define BAUD 9600

volatile int stop_thread1 = 0;
volatile int stop_thread2 = 0;
volatile int stop_thread3 = 0;

struct sharedData // 공유 자원을 위한 구조체
{
    unsigned int ArduinoIdle;
    unsigned int SendMsg;
    unsigned int RcvMsg;
    char sbuf[256];
    char rbuf[256];
    char priority;
    char intFlag;
};

struct sharedData sd; // 공유 자원 정의

pthread_mutex_t mutex; // mutex 객체

void signal_handler(int signum) {
    if (signum == SIGUSR1) {
        stop_thread1 = 1;
    } else if (signum == SIGUSR2) {
        stop_thread2 = 1;
    }
    return;
}

// 시간을 재는 함수
// utility로 옮길 예정
char* get_current_time() {
    static char buffer[100];
    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    return buffer;
}

// log message 남기는 함수 
// 동적 argument가 통하는지 모르겠다
void log_message(const char *format) {
    int fd = open("../log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    dprintf(fd, "[%s] [Thread %lu] ", get_current_time(), pthread_self());
    printf("[%s] [Thread %lu] ", get_current_time(), pthread_self());
    printf("\n");
    close(fd);
    return;
}

void* func1(void* arg) {
    int fd = open("../file1.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd == -1) {
        log_message("Failed to open file");
        return NULL;
    }

    while(!stop_thread1) {
        for (int i = 0; i < 10; ++i) {
            dprintf(fd, "[%s] [Thread %lu] Writing to file: %d\n", get_current_time(), pthread_self(), i);
            delay(431);
            if (stop_thread1) {
                break;
            }
        }
    }
    close(fd);
    log_message("File output thread terminated.");
    return NULL;
}

void* func2(void* arg) {
    int fd = open("../file2.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd == -1) {
        log_message("Failed to open file");
        return NULL;
    }

    while(!stop_thread2) {
        for (int i = 0; i < 10; ++i) {
            dprintf(fd, "[%s] [Thread %lu] Writing to file: %d\n", get_current_time(), pthread_self(), i);
            delay(499);
            if (stop_thread2) {
                break;
            }
        }
    }
    close(fd);
    log_message("File output thread terminated.");
    return NULL;
}

int main()
{
    pthread_t thread1, thread2;

    pthread_mutex_init(&mutex, NULL); // 뮤텍스 초기화
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    pthread_create(&thread1, NULL, func1, NULL);
    pthread_create(&thread2, NULL, func2, NULL);

    while(1)
    {
        // thread들이 살아있는지 주기적으로 메시지를 보낸다.
        if(pthread_kill(thread1, 0)==1)
        {
            log_message("thread1 부활");
            pthread_join(thread1, NULL);
            pthread_create(&thread1, NULL, func1, NULL);
        }
        if(pthread_kill(thread2, 0)==1)
        {
            log_message("thread2 부활");
            pthread_join(thread2, NULL);
            pthread_create(&thread2, NULL, func2, NULL);
        }
    }

    pthread_kill(thread1, SIGUSR1);
    pthread_kill(thread2, SIGUSR2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    log_message("End");

    return 0;
}