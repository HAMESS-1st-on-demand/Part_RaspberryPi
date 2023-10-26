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
#include "SI.h"

#define BAUD 9600

volatile int running = 1; // 프로그램 실행 상태를 제어
volatile int thread1_alive = 0; // 스레드 1의 상태
volatile int thread2_alive = 0; // 스레드 2의 상태
volatile int stop_thread1 = 0; // kill할 때 보내는 코드
volatile int stop_thread2 = 0;

struct sharedData // 공유 자원을 위한 구조체
{
    unsigned char changed; // 보낼 값이 있는지
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


unsigned char ssrStateFlag; //스마트 썬루프 On/Off 상태 변수
unsigned char tintingFlag; //Tinting 상태 변수

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
// 동적 argument가 통하는지 모르겠다
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

void ssrButtonInterrupt(void) {
    if (digitalRead(SSR_STATE_BUTT_PIN) == LOW) {
        ssrStateFlag = !ssrStateFlag;
        digitalWrite(SSR_STATE_LED_PIN,ssrStateFlag);
    }
}

void tintingButtonInterrupt(void) {
    if (digitalRead(TINTING_BUTT_PIN) == LOW) {
        tintingFlag = !tintingFlag;
        digitalWrite(TINTING_LED_PIN,tintingFlag);
    }
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

    unsigned int now; //현재시간 저장변수
    unsigned int msgTime; //msg주기 체크변수
    unsigned int lightTime, rainTime, dustTime, waterTime, temperTime; //센서 주기 체크변수

    //센서 판단 저장 버퍼
    unsigned char buffer = 0;

    //각각 시간을 추척하기위한 초기화
    now = lightTime = rainTime = dustTime = waterTime = temperTime = msgTime = millis();
    
    // 멈춤 signal이 들어오지 않으면
    while(!stop_thread1) { 
        // 센서 값들을 확인하고 Check
        now = millis();
        //각 주기 이상의 시간이 되면 센서값 측정 후 비교
        if(now - waterTime>=WATERLEV_PER) //수위 감지센서
        {
            waterTime = now;
            int waterLev = readWaterLevelSensor();
            printf("[%d] waterLev = %d\n", now/100,waterLev);

            if(readWaterLevelSensor()>WATERLEV_TH){ //판단
                printf("썬루프 열어\n");
                buffer |= 1<<4; //buffer: 10000
            }
        }

        if(ssrStateFlag){ //SSR 기능이 켜져있을때만 센서 값 검사
            if(now - lightTime>=LIGHT_PER) //조도 센서
            {
                lightTime = now;
                int light = readLightSensor();
                printf("[%d] light = %d\n", now/100,light);

                if(readLightSensor()<LIGHT_TH) { //판단
                    printf("썬루프 어둡게\n");
                    digitalWrite(TINTING_LED_PIN,HIGH);
                    tintingFlag=1;
                }
            }

            if(now - rainTime>=RAIN_PER) //비 센서
            {
                rainTime =now;
                int rain = readRainSensor();
                printf("[%d] rain = %d\n", now/100,rain);

                if(readRainSensor()<RAIN_TH){ //판단
                    printf("썬루프 닫아\n");
                    buffer |= 1<<3; //buffer: 01000
                }
            }

            if(now - dustTime>=DUST_PER) //미세먼지센서
            {
                dustTime=now;
                int dust = readDustSensor();
                printf("[%d] dust = %d\n", now/100,dust);

                if(readDustSensor()>DUST_TH){ //판단
                    printf("썬루프 닫아\n");
                    buffer |= 1<<2; //buffer: 00100
                }
            }

            if(now - temperTime>=TEMPER_PER) //온습도 센서
            {
                temperTime = now;
                int temper1 = readDHTSensor(DHT_PIN1);        // 10번 핀으로부터 데이터를 읽음 -> 실내온도
                int temper2 = readDHTSensor(DHT_PIN2);        // 11번 핀으로부터 데이터를 읽음 -> 실외 온도

                if(temper1 ==-1||temper2 ==-1){
                    printf("DHT data not good\n");
                    continue;
                }
                printf("[%d] temper1 = %d, temper2 = %d \n", now/10000,temper1,temper2);

                if(temper1<temper2&&temper2>TEMPER_TH1){ //판단
                    printf("썬루프 닫아");
                    buffer |= 1<<1; //buffer: 00010
                }
                else if(temper1>temper2&&temper1>TEMPER_TH2){
                    printf("썬루프 열어");
                    buffer |= 1; 
                }
            }
        }


        // mutex 값 확인 
        // 일정 시간 마다 살아있음을 보여주는 코드 작성
        if(now - msgTime >= 1000)
        {
            dprintf(log_fd, "%s %lu alive : %d\n", get_current_time(), pthread_self(), msgTime);
            msgTime = now;

            // 1초마다 진행
            pthread_mutex_lock(&mutex);
            if(0 < sd.changed && sd.changed < 3) // 메시지 안 보낸 것
            {
                ++(sd.changed);
            }
            else{
                sd.changed = 1;
                // update the send data 
                sprintf(sd.sbuf, "%s", "d\0");    
                sd.SendMsg = strlen(sd.sbuf) + 1;
                dprintf(log_fd, "%s %lu send msg flag up : %s\n", get_current_time(), pthread_self(), sd.sbuf);
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
        printf("delta Time Between loop-start and loop-end: %d\n", millis()-now);
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
        char buf[65];
        unsigned len = 0; 
        // 살아있다는 값 계속 전달 
        if(millis() - times >= 1000)
        {
            dprintf(log_fd, "%s %lu alive : %d\n", get_current_time(), pthread_self(), times);
            times = millis();
        }
        pthread_mutex_lock(&mutex);
        if(sd.changed > 0 && sd.SendMsg > 0)
        {
            strcpy(buf, sd.sbuf);
            len = sd.SendMsg;
            dprintf(log_fd, "%s %lu send msg copy : %d\n", get_current_time(), pthread_self(), len);
            sd.changed = 0;
            sd.SendMsg = 0;
            memset(sd.sbuf, '\0', sizeof(sd.sbuf));
        }
        pthread_mutex_unlock(&mutex);

        if(len > 0) // sd.SendMsg => len 공유 자원에 접근하지 않도록 수정
        {
            for(int i=0;i<len;++i)serialPutchar(uart_fd, buf[i]);
            dprintf(log_fd, "%s %lu Writing to Arduino: %d\n", get_current_time(), pthread_self(), buf[0]);
        }

        delay(10);
        memset(buf, '\0', sizeof(buf));
        int i = 0;
        if(serialDataAvail(uart_fd))
        {
            buf[i] = serialGetchar(uart_fd);
            printf("===> Received : %d %c\n", buf[i], buf[i]);
            serialFlush(uart_fd);
            dprintf(log_fd, "%s %lu Received Msg trom Arduino : %c\n", get_current_time(), pthread_self(), buf[0]);
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

    //스레드1에서 사용하는 핀에 대한 wiringPi init함수 (SI.h, SI.c에 정의되어있음)
    if(setWiringPi()==-1) {
        printf("Failed to setup WiringPi\n");
        return -1;
    }
    //스레드 1에서 사용할 버튼 인터럽트 설정
    if (wiringPiISR(SSR_STATE_BUTT_PIN, INT_EDGE_FALLING, &ssrButtonInterrupt) < 0) {
        fprintf(stderr, "인터럽트 핸들러 설정 실패\n");
        return 1;
    }
    if (wiringPiISR(TINTING_BUTT_PIN, INT_EDGE_FALLING, &tintingButtonInterrupt) < 0) {
        fprintf(stderr, "인터럽트 핸들러 설정 실패\n");
        return 1;
    }

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