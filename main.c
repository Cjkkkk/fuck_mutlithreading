#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#define max_thread_number 100 // 最大线程数目
#define true 1 //设置bool类型
#define false 0 //设置bool类型
typedef int bool; //设置bool类型
//初始化互斥锁和条件变量
struct car{
    int car_number;
    char direction;
};  // 定义小车的数据结构
typedef struct car car;// typedef
pthread_cond_t queueNorth = PTHREAD_COND_INITIALIZER; //北边队列中的车需要等待的条件变量
pthread_cond_t queueEast = PTHREAD_COND_INITIALIZER; //东边队列中的车需要等待的条件变量
pthread_cond_t queueSouth = PTHREAD_COND_INITIALIZER; //南边队列中的车需要等待的条件变量
pthread_cond_t queueWest = PTHREAD_COND_INITIALIZER; //西边队列中的车需要等待的条件变量
pthread_cond_t firstNorth = PTHREAD_COND_INITIALIZER; //北边在十字路口的车需要等待的条件变量
pthread_cond_t firstEast = PTHREAD_COND_INITIALIZER;//东边在十字路口的车需要等待的条件变量
pthread_cond_t firstSouth = PTHREAD_COND_INITIALIZER;//南边在十字路口的车需要等待的条件变量
pthread_cond_t firstWest = PTHREAD_COND_INITIALIZER;//西边在十字路口的车需要等待的条件变量
pthread_cond_t dead_lock_detection = PTHREAD_COND_INITIALIZER; //死锁线程等待的条件变量

pthread_mutex_t detect_thread_mutex = PTHREAD_MUTEX_INITIALIZER; // 与dead_lock_detection 一起使用的锁
pthread_mutex_t deadlock_mutex = PTHREAD_MUTEX_INITIALIZER; //保证同时只会有一个方向的一个车申请死锁检测的锁
pthread_mutex_t north_mutex = PTHREAD_MUTEX_INITIALIZER; //保证北方车数目正确的锁
pthread_mutex_t east_mutex = PTHREAD_MUTEX_INITIALIZER; //保证东方车数目正确的锁
pthread_mutex_t south_mutex = PTHREAD_MUTEX_INITIALIZER; //保证南方车数目正确的锁
pthread_mutex_t west_mutex = PTHREAD_MUTEX_INITIALIZER; //保证西方车数目正确的锁

char request_direction; //请求检测思索的方向
bool is_deadlock; //是否发生了死锁
int north_queue_number = 0; //北方的车的总数
int south_queue_number = 0; //南方的车的总数
int east_queue_number = 0; //东方的车的总数
int west_queue_number = 0; //西方的车的总数

int* number_list[4] = {&north_queue_number, &east_queue_number, &south_queue_number, &west_queue_number}; //用于快速获取车队数目的数组
pthread_cond_t* queue_cond_list[4] = {&queueNorth, &queueEast, &queueSouth, &queueWest}; //用于快速获取车队条件变量的数组
pthread_cond_t* first_cond_list[4] = {&firstNorth, &firstEast, &firstSouth, &firstWest}; //用于快速获取车队条件变量的数组
pthread_mutex_t* mutex_list[4] = {&north_mutex, &east_mutex, &south_mutex, &west_mutex}; //用于快速获取车队锁的数组

// 方向与index的映射
int get_index(char direction){
    if(direction == 'n')return 0;
    if(direction == 'e')return 1;
    if(direction == 's')return 2;
    if(direction == 'w')return 3;
    return -1;
}

// 死锁检测的逻辑
void *deadlock_detect_thread(void *x_void_ptr){
    int loop = 0;
    pthread_mutex_lock( & detect_thread_mutex );
    while(loop < max_thread_number){ //最多被唤醒max_thread_number次
        pthread_cond_wait( & dead_lock_detection, & detect_thread_mutex ); //等待被唤醒
        for(int i = 0 ; i < 4 ; i++) pthread_mutex_lock( mutex_list[i] ); //不允许更改车队数目了
        if(north_queue_number > 0 && south_queue_number > 0 && east_queue_number >0 && west_queue_number > 0){
            // 四个方向都有车说明发生了死锁
            is_deadlock = true;
            printf("DEADLOCK: car jam detected, signalling %c to go\n", request_direction);
        }
        else is_deadlock = false;
        for(int i = 0 ; i < 4 ; i++) pthread_mutex_unlock( mutex_list[i] ); // 重新允许更改车队数目
        pthread_cond_signal( first_cond_list[get_index(request_direction)] ); // 通知申请检测的车
        loop ++;
    }
    pthread_mutex_unlock( & detect_thread_mutex );
    pthread_exit(NULL);//离开线程
}

//新的车辆来临
void *car_arrive(void *x_void_ptr){
    car new_car = *(car*)x_void_ptr; // 获取来的车的信息
    char direction = new_car.direction; //获取方向
    int index = get_index(direction); //获取方向的index映射
    int right_index = (index + 3) % 4; //获取右边队列的index
    int left_index = (index + 1) % 4; //获取左边队列的index
    pthread_mutex_lock( mutex_list[index] ); //申请获取锁
    *(number_list[index]) = *(number_list[index]) + 1; // 修改车队数目
    if(* number_list[index] > 1) pthread_cond_wait( queue_cond_list[index], mutex_list[index] ); // 该方向路口已经存在车在等待

    //被唤醒后达到路口
    printf("car %d from %c arrives at crossing\n", new_car.car_number, new_car.direction);   //输出到达路口的信息
    pthread_mutex_unlock( mutex_list[index] );

    pthread_mutex_lock( & deadlock_mutex ); // 申请死锁检测
    //开始检测死锁
    request_direction = direction; // 修改申请方向
    pthread_cond_signal( & dead_lock_detection ); //唤醒检测死锁的线程
    pthread_cond_wait( first_cond_list[index], mutex_list[index]); //等到死锁检测结束
    pthread_mutex_unlock( & deadlock_mutex );
    if(!is_deadlock) { //假如没有发生死锁
        int right_queue_number = * number_list[right_index]; //查看右边队列是否有车
        if( right_queue_number > 0 ) pthread_cond_wait( first_cond_list[index], mutex_list[index] ); //有车则开始等待
    }
    // 被其他车唤醒,开始放行
    printf("car %d from %c leaving at crossing\n", new_car.car_number, new_car.direction);   //输出离开路口的信息
    *(number_list[index]) = *(number_list[index]) - 1; //该方向车数目减少1
    pthread_mutex_unlock( mutex_list[index] );
    // 通知左边的队列
    sleep(1);
    pthread_cond_signal( first_cond_list[left_index] ); // 通知左边的车避免饥饿
//    sleep(1);
    pthread_cond_signal( queue_cond_list[index] ); // 通知这个方向的正在等待的车到路口
    pthread_exit(NULL);//离开线程
}

int main(int argc, char** argv){
    if(argc != 2) return 0; // 没有提供车的信息
    char* cars = argv[1];
    pthread_t thread_list[max_thread_number];
    pthread_t deadlock_detect;
    car resource[max_thread_number];
    int count = 0;
    // 创建死锁检测线程
    if(pthread_create(&(deadlock_detect), NULL, deadlock_detect_thread, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    // 对输入的每一辆车,创建车辆线程
    for(count = 0 ; count < max_thread_number ; count++){
        if(cars[count]){
            car new_car = { count, cars[count]}; // 创建车辆信息
            resource[count] = new_car;
            if(pthread_create(&(thread_list[count]), NULL, car_arrive, &(resource[count]))) {
                fprintf(stderr, "Error creating thread\n");
                return 1;
            }
        }else break;
    }
    // 等待创建的线程结束
    for (int i = 0; i < count; ++i) if (pthread_join(thread_list[i], NULL) != 0) fprintf(stderr, "error: Cannot join thread # %d\n", i);
}