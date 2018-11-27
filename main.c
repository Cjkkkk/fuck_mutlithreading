#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#define max_thread_number 100
#define true 1
#define false 0
typedef int bool;
//初始化互斥锁和条件变量
struct car{
    int car_number;
    char direction;
};
typedef struct car car;

pthread_cond_t queueNorth = PTHREAD_COND_INITIALIZER;
pthread_cond_t queueEast = PTHREAD_COND_INITIALIZER;
pthread_cond_t queueSouth = PTHREAD_COND_INITIALIZER;
pthread_cond_t queueWest = PTHREAD_COND_INITIALIZER;

pthread_cond_t firstNorth = PTHREAD_COND_INITIALIZER;
pthread_cond_t firstEast = PTHREAD_COND_INITIALIZER;
pthread_cond_t firstSouth = PTHREAD_COND_INITIALIZER;
pthread_cond_t firstWest = PTHREAD_COND_INITIALIZER;

pthread_cond_t dead_lock_detection = PTHREAD_COND_INITIALIZER;
pthread_mutex_t detect_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t deadlock_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t north_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t east_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t south_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t west_mutex = PTHREAD_MUTEX_INITIALIZER;

char request_direction;
int number;
bool is_deadlock;
int north_queue_number = 0;
int south_queue_number = 0;
int east_queue_number = 0;
int west_queue_number = 0;

int* number_list[4] = {&north_queue_number, &east_queue_number, &south_queue_number, &west_queue_number};
pthread_cond_t* queue_cond_list[4] = {&queueNorth, &queueEast, &queueSouth, &queueWest};
pthread_cond_t* first_cond_list[4] = {&firstNorth, &firstEast, &firstSouth, &firstWest};
pthread_mutex_t* mutex_list[4] = {&north_mutex, &east_mutex, &south_mutex, &west_mutex};

int get_index(char direction){
    if(direction == 'n')return 0;
    if(direction == 'e')return 1;
    if(direction == 's')return 2;
    if(direction == 'w')return 3;
    return -1;
}

void *deadlock_detect_thread(void *x_void_ptr){
    int i = 0;
    pthread_mutex_lock( & detect_thread_mutex ); //不允许更改车队数目
    while(i < 1000){
        pthread_cond_wait( & dead_lock_detection, & detect_thread_mutex );
        pthread_mutex_lock( & north_mutex );
        pthread_mutex_lock( & east_mutex );
        pthread_mutex_lock( & south_mutex );
        pthread_mutex_lock( & west_mutex );
        //printf("begin:%d\n", number);
        if(north_queue_number > 0 && south_queue_number > 0 && east_queue_number >0 && west_queue_number > 0){
            is_deadlock = true;
            printf("DEADLOCK: car jam detected, signalling %c to go\n", request_direction);
        }else is_deadlock = false;
        i ++;
        int index = get_index(request_direction);
//        printf("end:%d\n", number);
        pthread_mutex_unlock( & north_mutex );
        pthread_mutex_unlock( & east_mutex );
        pthread_mutex_unlock( & south_mutex );
        pthread_mutex_unlock( & west_mutex );
        pthread_cond_signal( first_cond_list[index] );
    }
    pthread_mutex_unlock( & detect_thread_mutex ); //不允许更改车队数目
    pthread_exit(NULL);//离开线程
}
// 死锁检测的逻辑
void *car_arrive(void *x_void_ptr){
    car new_car = *(car*)x_void_ptr;
    char direction = new_car.direction;
    int index = get_index(direction);
    int right_index = (index + 3) % 4;
    int left_index = (index + 1) % 4;
    pthread_mutex_lock( mutex_list[index] );
    *(number_list[index]) = *(number_list[index]) + 1;
    if(* number_list[index] > 1) {
        pthread_cond_wait( queue_cond_list[index], mutex_list[index] );
    }
    //达到路口
    printf("car %d from %c arrives at crossing\n", new_car.car_number, new_car.direction);   //输出到达路口的信息
    pthread_mutex_unlock( mutex_list[index] );

    pthread_mutex_lock( & deadlock_mutex );
    //开始检测死锁
    request_direction = direction;
    number = new_car.car_number;
    pthread_cond_signal( & dead_lock_detection ); //检测死锁
    pthread_cond_wait( first_cond_list[index], mutex_list[index]);
    pthread_mutex_unlock( & deadlock_mutex );
    //printf("got signal, is deadlock %d %d\n",new_car.car_number, is_deadlock);
    if(!is_deadlock) {
        int right_queue_number = * number_list[right_index];
        if( right_queue_number > 0 ) pthread_cond_wait( first_cond_list[index], mutex_list[index] );
    }
    // 开始放行
    printf("car %d from %c leaving at crossing\n", new_car.car_number, new_car.direction);   //输出离开路口的信息
    *(number_list[index]) = *(number_list[index]) - 1;
    pthread_mutex_unlock( mutex_list[index] );
    // 通知左边的队列
    pthread_cond_signal( first_cond_list[left_index] );
    sleep(1);
    pthread_cond_signal( queue_cond_list[index] );
    pthread_exit(NULL);//离开线程
}

int main(int argc, char** argv){
    if(argc != 2) return 0;
    char* cars = argv[1];
    pthread_t thread_list[max_thread_number];
    pthread_t deadlock_detect;
    car resource[max_thread_number];
    int count = 0;
    if(pthread_create(&(deadlock_detect), NULL, deadlock_detect_thread, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    for(count = 0 ; count < max_thread_number ; count++){
        if(cars[count]){
            resource[count].car_number = count;
            resource[count].direction = cars[count];
            if(pthread_create(&(thread_list[count]), NULL, car_arrive, &(resource[count]))) {
                fprintf(stderr, "Error creating thread\n");
                return 1;
            }
        }else break;
    }
    // 等待创建的线程结束
    for (int i = 0; i < count; ++i) if (pthread_join(thread_list[i], NULL) != 0) fprintf(stderr, "error: Cannot join thread # %d\n", i);
}