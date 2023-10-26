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
#include "SI.h" // 센서를 실제로 받는 코드 작성

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
    char sbuf;
    char rbuf;
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
    int fd = open("../log_main.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if(fd < 0){
        return;
    }
    dprintf(fd, "%s %lu : %s\n", get_current_time(), pthread_self(), format);
    printf("%s %lu : %s", get_current_time(), pthread_self(), format);
    printf("\n");
    close(fd);
    return;
}

// 센서의 값들을 메시지로 변환
char makeMsg(unsigned char sensorInput) { // unsigned char로 변경
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
    int log_fd = open("../log_thread1.txt", O_WRONLY | O_CREAT | O_APPEND, 0666); // log file을 위한 descriptor
    if (log_fd == -1) { // log file descriptor 못 쓰면 반환한다. 
        log_message("Failed to open file.\n");
        // thread1_alive = 0로 thread 멈췄다고 알려준다.
        thread1_alive = 0;
        log_message("Thread 1 is terminated.\n");
        return NULL;
    }
    dprintf(log_fd, "%s %lu start\n", get_current_time(), pthread_self());

    char sendBuf; // 보내는 값의 buffer : mutex 값 복사 
    // 현재 시간, msg 주기, 센서 주기 값들
    unsigned int now, msgTime, lightTime, rainTime, dustTime, waterTime, temperTime;
    unsigned char flags = 0, buffer = 0; // 센서 판단 저장, sendflag
    // 멈춤 signal이 들어오지 않으면

    // 초기화 및 동기화
    now = lightTime = rainTime = dustTime = waterTime = temperTime = msgTime = millis();
    
    dprintf(log_fd, "%s %lu Before loop %lu\n", get_current_time(), pthread_self(), millis()-now);

    while(!stop_thread1) { 
        now = millis();

        // 수위 감지 센서 추가
        if(now - waterTime>=WATERLEV_PER)
        {
            waterTime = now;
            int waterLev = readWaterLevelSensor();
            printf("[%d] waterLev = %d\n", now/100,waterLev);

            if(readWaterLevelSensor()>WATERLEV_TH){ //판단
                printf("썬루프 열어\n");
                buffer |= 1<<4; //buffer: 10000
            }
        }

        dprintf(log_fd, "%s %lu 수위 센서%lu\n", get_current_time(), pthread_self(), millis()-now);

        // 조도 센서 추가
        if(now - lightTime>=LIGHT_PER)
        {
            lightTime = now;
            int light = readLightSensor();
            printf("[%d] light = %d\n", now/100,light);

            if(readLightSensor()<LIGHT_TH) { //판단
                printf("썬루프 어둡게\n"); // buffer가 아닌 LED 진행
            }
        }

        dprintf(log_fd, "%s %lu 조도 센서 %lu\n", get_current_time(), pthread_self(), millis()-now);

         // rain 센서추가 
        if(now - rainTime>=RAIN_PER)
        {
            rainTime =now;
            int rain = readRainSensor();
            printf("[%d] rain = %d\n", now/100,rain);

            if(readRainSensor()<RAIN_TH){ //판단
                printf("썬루프 닫아\n");
                buffer |= 1<<3; //buffer: 01000
            }
        }

        dprintf(log_fd, "%s %lu rain 센서%lu\n", get_current_time(), pthread_self(), millis()-now);

        // 미세먼지 센서 추가
        if(now - dustTime>=DUST_PER)
        {
            dustTime=now;
            int dust = readDustSensor();
            printf("[%d] dust = %d\n", now/100,dust);
            
            if(readDustSensor()>DUST_TH){ //판단
                printf("썬루프 닫아\n");
                buffer |= 1<<2; //buffer: 00100
            }
        }

        dprintf(log_fd, "%s %lu 미세먼지 센서%lu\n", get_current_time(), pthread_self(), millis() - now);

        // 온습도 센서 추가
        if(now - temperTime>=TEMPER_PER)
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

        dprintf(log_fd, "%s %lu 온습도 센서%lu\n", get_current_time(), pthread_self(), millis() - now);
        // 일정 시간 마다 살아있음을 보여주는 코드 작성
        if(millis() - msgTime >= 700)
        {
            dprintf(log_fd, "%s %lu alive : %d\n", get_current_time(), pthread_self(), msgTime);
            msgTime = millis();

            sendBuf = makeMsg(buffer);
            // 1초마다 진행
            pthread_mutex_lock(&mutex);
            if(0 < sd.changed && sd.changed < 3) // 메시지 안 보낸 것
            {
                ++(sd.changed);
            }
            else{
                sd.changed = 1;
                // update the send data 
                if((int)sendBuf!=0)
                {
                    sd.sbuf = sendBuf;  
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
        dprintf(log_fd, "%s %lu delta Time Between loop-start and loop-end: %lu\n", get_current_time(), pthread_self(), millis()-now);
    }
    close(log_fd);
    thread1_alive = 0;
    log_message("Thread 1 is terminated.\n");
    return NULL;
}

void* func2(void* arg) {
    int log_fd = open("../log_thread2.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
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

    // setWiringPi로 gpio on
    if(setWiringPi()==-1) {
        log_message("Failed to setup WiringPi\n");
        return -1;
    }
    else{
        log_message("Success to setup WiringPi\n");
    }
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