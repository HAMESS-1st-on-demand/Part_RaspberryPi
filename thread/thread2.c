#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

volatile int thread1_exit_request = 0;
volatile int thread2_exit_request = 0;

void* thread_function1(void* arg) {
    printf("Thread 1 시작\n");
    while(!thread1_exit_request) {
        sleep(1);
    }
    printf("Thread 1 종료\n");
    return NULL;
}

void* thread_function2(void* arg) {
    printf("Thread 2 시작\n");
    while(!thread2_exit_request) {
        sleep(1);
    }
    printf("Thread 2 종료\n");
    return NULL;
}

int main(void) {
    pthread_t thread1, thread2;

    // 스레드 생성
    pthread_create(&thread1, NULL, thread_function1, NULL);
    pthread_create(&thread2, NULL, thread_function2, NULL);

    // 일정 시간 후 스레드 종료 요청
    sleep(3);
    thread1_exit_request = 1;
    thread2_exit_request = 1;

    // 스레드가 종료될 때까지 기다림
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}
