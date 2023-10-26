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
    unsigned char changed; 
    unsigned char ArduinoIdle;
    unsigned char SendMsg;
    unsigned char RcvMsg;
    char sbuf[65];
    char rbuf[65];
    unsigned char status; // open/close
};

unsigned char threadEnable = 1; // thread를 살리는지 여부

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
    int log_fd = open("../file1.txt", O_WRONLY | O_CREAT | O_APPEND, 0666); // log file을 위한 descriptor
    if (log_fd == -1) { // log file descriptor 못 쓰면 반환한다. 
        log_message("Failed to open file");
        return NULL;
    }
    // 멈춤 signal이 들어오지 않으면
    while(!stop_thread1) { 
        // 센서 값들을 확인하고 Check

        // mutex 값 확인 
        pthread_mutex_lock(&mutex);
        if(0 < sd.changed && sd.changed < 3) // 메시지 안 보낸 것
        {
            ++(sd.changed);
            dprintf(log_fd, "[%s] [Thread %lu] add changed : %d\n", get_current_time(), pthread_self(), sd.changed);
        }
        else{
            sd.changed = 1;
            // update the send data
            sprintf(sd.sbuf, "Hello, my name is %d", pthread_self());    
            sd.SendMsg = strlen(sd.sbuf) + 1;
            dprintf(log_fd, "[%s] [Thread %lu] send msg : %s\n", get_current_time(), pthread_self(), sd.sbuf);
        }
        pthread_mutex_unlock(&mutex);
        dprintf(log_fd, "[%s] [Thread %lu] default running\n", get_current_time(), pthread_self());
        
        if(stop_thread1)
        {
            dprintf(log_fd, "[%s] [Thread %lu] Take End signal\n", get_current_time(), pthread_self());
            break;
        }
    }
    close(log_fd);
    log_message("File output thread terminated.");
    return NULL;
}

void* func2(void* arg) {
    int log_fd = open("../file2.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (log_fd == -1) {
        log_message("Failed to open file");
        return NULL;
    }

    int uart_fd = serialOpen("/dev/ttyS0", BAUD);
    if(uart_fd == -1)
    {
        log_message("Device file open error!! ");
        return NULL;
    }

    while(!stop_thread2) {
        // 계속해서 값을 받는다. 
        // mutex 잠그고 값 확인
        char buf[65]; 
        pthread_mutex_lock(&mutex);
        if(sd.changed > 0)
        {
            if(sd.SendMsg > 0)strcpy(buf, sd.sbuf);
            dprintf(log_fd, "[%s] [Thread %lu] send msg copy : %d\n", get_current_time(), pthread_self(), sd.changed);
            sd.changed = 0;
            memset(sd.sbuf, '\0', sizeof(sd.sbuf));
        }
        pthread_mutex_unlock(&mutex);

        if(sd.SendMsg > 0)
        {
            for(int i=0;i<sd.SendMsg;++i)serialPutchar(uart_fd, buf[i]);
            sd.SendMsg = 0;
            dprintf(log_fd, "[%s] [Thread %lu] Writing to Arduino: %d\n", get_current_time(), pthread_self());
        }

        delay(10);
        memset(buf, '\0', sizeof(buf));
        int i = 0;
        if(serialDataAvail(uart_fd))
        {
            buf[i] = serialGetchar(uart_fd);
            printf("===> Received : %d %c\n", buf[i], buf[i]);
            serialFlush(uart_fd);
            dprintf(log_fd, "[%s] [Thread %lu] Received Msg trom Arduino : %d\n", get_current_time(), pthread_self(), i);
        }

        if (stop_thread2) {
            dprintf(log_fd, "[%s] [Thread %lu] Take End signal\n", get_current_time(), pthread_self());
            break;
        }
    }
    close(log_fd);
    close(uart_fd);
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
        if(pthread_kill(thread1, 0)!=0)
        {
            log_message("thread1 부활");
            pthread_join(thread1, NULL);
            pthread_create(&thread1, NULL, func1, NULL);
        }
        if(pthread_kill(thread2, 0)!=0)
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

    pthread_mutex_destroy(&mutex); 

    log_message("End");

    return 0;
}