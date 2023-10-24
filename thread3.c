#include <stdio.h> // 표준 입출력 함수를 위한 헤더
#include <pthread.h> // pthread.h POSIX 스레드를 위한 헤더
#include <unistd.h> // UNIX 표준 시스템 호출을 위한 헤더
#include <fcntl.h> // 파일 제어 옵션을 위한 헤더
#include <termios.h> // 터미널 I/O 인터페이스를 위한 헤더

void* file_output_thread(void* arg) { 
    // 'file.txt' 파일을 쓰기 모드로 연다
    int fd = open("../file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) { 
        perror("FFailed to open file");
        return NULL;
    }

    for (int i = 0; i < 10; ++i) {
        dprintf(fd, "Writing to file: %d\n", i);
        delay(200); // 1초 지연
    }

    close(fd); // 파일 닫기
    return NULL;
}

void* uart_send_thread(void* arg) {
    int fd = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY); // UART 디바이스를 비동기 모드로 연다
    if (fd == -1) {
        perror("Failed to open UART");
        return NULL;
    }

    struct termios options; // UART 통신을 위한 설정 진행
    tcgetattr(fd, &options);
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(fd, TCIFLUSH); 
    tcsetattr(fd, TCSANOW, &options);

    for (int i = 0; i < 10; ++i) {
        int ret = write(fd, "UART Message", 12);
        if (ret < 0) {
            perror("Failed to write to UART");
            break;
        }
        sleep(1);
    }

    close(fd);
    return NULL;
}

int main(void) {
    pthread_t thread1, thread2;

    // thread 2개 실행하고 정의 
    pthread_create(&thread1, NULL, file_output_thread, NULL);
    pthread_create(&thread2, NULL, uart_send_thread, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    sleep(2);  // 추가적인 딜레이

    return 0;
}
