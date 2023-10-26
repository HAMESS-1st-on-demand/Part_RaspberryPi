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
#include <sys/time.h>

#define BAUD 9600

volatile int running = 1; // 프로그램 실행 상태를 제어
volatile int thread1_alive = 0; // 스레드 1의 상태
volatile int thread2_alive = 0; // 스레드 2의 상태
volatile int stop_thread1 = 0; // kill할 때 보내는 코드
volatile int stop_thread2 = 0; // 

struct sharedData // 공유 자원을 위한 구조체
{
    unsigned char changed; // 보낼 값이 있는지
    unsigned char ArduinoIdle; 
    unsigned char SendMsg;
    unsigned char RcvMsg;
    char sbuf; // charactor 배열이 아닌 값으로 변경 
    char rbuf; // charactor 값으로 변경
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

char* get_current_time() {
    static char buffer[100];
    struct timeval tv;
    struct tm *timeinfo;

    gettimeofday(&tv, NULL);
    timeinfo = localtime(&tv.tv_sec); // 현재 시간을 초 단위로 얻음

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo); // 날짜와 시간 형식화
    snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), " %06ld", tv.tv_usec); // 마이크로초 추가

    return buffer;
}

// log message 남기는 함수 
void log_message(const char *format) {
    int fd = open("../log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if(fd < 0){
        return;
    }
    dprintf(fd, "%s %lu : %s\n", get_current_time(), pthread_self(), format);
    printf("%s %lu : %s", get_current_time(), pthread_self(), format);
    printf("\n");
    close(fd);
    return;
}

// 새로 추가한 값. 
char makeMsg(char sensorInput) {
    char msg = 0;
    if (sensorInput & (1 << 4)) { // 침수
        msg |= 13 << 3;
        msg |= 1 << 2;
    } 
    else if (sensorInput & (1 << 3)) msg |= 12 << 3; // 비
    else if (sensorInput & (1 << 2)) msg |= 11 << 3; // 미세먼지
    else if (sensorInput & (1 << 1)) msg |= 10 << 3; // 온도 -> 열기
    else if (sensorInput & 1) { // 온도 -> 닫기
        msg |= 9 << 3;
        msg |= 1 << 2;
    }
    return msg;
}


void* func1(void* arg) {
    int log_fd = open("../file1.txt", O_WRONLY | O_CREAT | O_APPEND, 0666); // log file을 위한 descriptor
    if (log_fd == -1) { // log file descriptor 못 쓰면 반환한다. 
        log_message("Failed to open file.\n");
        // thread1_alive = 0로 thread 멈췄다고 알려준다.
        thread1_alive = 0;
        log_message("Thread 1 is terminated.\n");
        return NULL;
    }
    dprintf(log_fd, "%s %lu start\n", get_current_time(), pthread_self());

    char buf;
    unsigned char flags = 0;
    int times = 0;
    // 멈춤 signal이 들어오지 않으면
    while(!stop_thread1) { 
        // 센서 값들을 확인하고 Check

        // mutex 값 확인 
        // 일정 시간 마다 살아있음을 보여주는 코드 작성
        if(millis() - times >= 500)
        {
            dprintf(log_fd, "%s %lu alive : %d\n", get_current_time(), pthread_self(), times);
            times = millis();

            buf = makeMsg((char)8);
            // 1초마다 진행
            pthread_mutex_lock(&mutex);
            if(0 < sd.changed && sd.changed < 3) // 메시지 안 보낸 것
            {
                ++(sd.changed);
            }
            else{
                sd.changed = 1;
                // update the send data 
                if((int)buf!=0)
                {
                    sd.sbuf = buf;  
                    sd.SendMsg = 1;
                    dprintf(log_fd, "%s %lu send msg flag up : %d\n", get_current_time(), pthread_self(), (int)sd.sbuf);
                }
            }

            pthread_mutex_unlock(&mutex);
        }
        if(stop_thread1)
        {
            dprintf(log_fd, "%s %lu Take End signal\n", get_current_time(), pthread_self());
            close(log_fd);
            thread1_alive = 0;
            log_message("Thread 1 is terminated.\n");
            break;
        }
    }
    close(log_fd);
    thread1_alive = 0;
    log_message("Thread 1 is terminated.\n");
    return NULL;
}

void* func2(void* arg) {
    int log_fd = open("../file2.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (log_fd == -1) {
        log_message("Failed to open file\n");
        // thread2가 죽었다는 flag 작성
        thread2_alive = 0;
        log_message("Thread2 is terminated.\n");
        return NULL;
    }
    dprintf(log_fd, "%s %lu start\n", get_current_time(), pthread_self());
    
    // ttyS0 이 아닌 ttyUSB0 <= dmesg | tail로 확인
    int uart_fd = serialOpen("/dev/ttyUSB0", BAUD);
    if(uart_fd == -1)
    {
        log_message("Device file open error!!\n");
        // thread2 정리
        close(log_fd);
        thread2_alive = 0;
        log_message("Thread2 is terminated.\n");
        return NULL;
    }
    
    int times = millis();
    while(!stop_thread2) {
        // 계속해서 값을 받는다. 
        // mutex 잠그고 값 확인
        unsigned char sendflag;
        char buf; 
        // 살아있다는 값 계속 전달 
        if(millis() - times >= 1000)
        {
            dprintf(log_fd, "%s %lu alive : %d\n", get_current_time(), pthread_self(), times);
            times = millis();
        }
        pthread_mutex_lock(&mutex);
        if(sd.changed > 0 && sd.SendMsg > 0)
        {
            // strcpy(buf, sd.sbuf);
            sendflag = 1;
            buf = sd.sbuf;
            dprintf(log_fd, "%s %lu send msg copy : %d\n", get_current_time(), pthread_self(), (int)buf);
            sd.changed = 0;
            sd.SendMsg = 0;
            sd.sbuf = '\0';
        }
        pthread_mutex_unlock(&mutex);

        if(sendflag == 1)
        {
            serialPutchar(uart_fd, buf);
            dprintf(log_fd, "%s %lu Writing to Arduino: %d\n", get_current_time(), pthread_self(), (int)buf);
            sendflag = 0;
            buf = '\0';
        }

        int i = 0;
        if(serialDataAvail(uart_fd))
        {
            buf = serialGetchar(uart_fd);
            printf("===> Received : %d\n", (int)buf);
            serialFlush(uart_fd);
            dprintf(log_fd, "%s %lu Received Msg trom Arduino : %d\n", get_current_time(), pthread_self(), (int)buf);
        }

        if (stop_thread2) {
            dprintf(log_fd, "%s %lu Take End signal\n", get_current_time(), pthread_self());
            serialClose(uart_fd);
            close(log_fd);
            thread2_alive = 0;
            log_message("Thread2 is terminated.\n");
            break;
        }
    }
    serialClose(uart_fd);
    close(log_fd);
    thread2_alive = 0;
    log_message("Thread2 is terminated.\n");
    return NULL;
}

int main()
{
    log_message("Start\n");
    pthread_t thread1, thread2;

    pthread_mutex_init(&mutex, NULL); // 뮤텍스 초기화
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    if (pthread_create(&thread1, NULL, &func1, NULL)) {
        log_message("Error creating thread 1\n");
        return 1;
    }
    else{
        thread1_alive = 1;
    }
    if (pthread_create(&thread2, NULL, &func2, NULL)) {
        log_message("Error creating thread 2\n");
        return 1;
    }else{
        thread2_alive = 1;
    }

    while(1)
    {
        if(!thread1_alive)
        {
            log_message("Thread1 is down. Restarting...\n");
            pthread_join(thread1, NULL);
            pthread_create(&thread1, NULL, &func1, NULL);
            thread1_alive = 1;
        }
        if(!thread2_alive)
        {
            log_message("Thread2 is down. Restarting...\n");
            pthread_join(thread2, NULL);
            pthread_create(&thread2, NULL, &func2, NULL);
            thread2_alive = 1;
        }

        sleep(1);
    }

    pthread_kill(thread1, SIGUSR1);
    pthread_kill(thread2, SIGUSR2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_mutex_destroy(&mutex); 

    log_message("End\n");

    return 0;
}