#include<iostream>
#include <stdio.h>
#include<typeinfo>
#include <stdlib.h>
#include<semaphore.h>
#include<pthread.h>
#include<tmmintrin.h>
#include<xmmintrin.h>
#include<emmintrin.h>
#include<pmmintrin.h>
#include<smmintrin.h>
#include<nmmintrin.h>
#include<immintrin.h>
#include<windows.h>
using namespace std;
#define N 1000

#define NUM_THREADS 15
float** A = NULL;


long long head, tail, freq;

sem_t sem_main;  //信号量
sem_t sem_workstart[NUM_THREADS];
sem_t sem_workend[NUM_THREADS];

sem_t sem_leader;
sem_t sem_Division[NUM_THREADS];
sem_t sem_Elimination[NUM_THREADS];

pthread_barrier_t barrier_Division;
pthread_barrier_t barrier_Elimination;

struct threadParam_t {    //参数数据结构
    int k;
    int t_id;
};

void A_init() {     //未对齐的数组的初始化
    A = new float* [N];
    for (int i = 0; i < N; i++) {
        A[i] = new float[N];
    }
    for (int i = 0; i < N; i++) {
        A[i][i] = 1.0;
        for (int j = i + 1; j < N; j++) {
            A[i][j] = rand() % 1000;
        }

    }
    for (int k = 0; k < N; k++) {
        for (int i = k + 1; i < N; i++) {
            for (int j = 0; j < N; j++) {
                A[i][j] += A[k][j];
                A[i][j] = (int)A[i][j] % 1000;
            }
        }
    }
}

void deleteA() {
    for (int i = 0; i < N; i++) {
        delete[] A[i];
    }
    delete A;
}

void print(float** a) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            cout << a[i][j] << " ";
        }
        cout << endl;
    }
}

void LU() {    //普通消元算法
    for (int k = 0; k < N; k++) {
        for (int j = k + 1; j < N; j++) {
            A[k][j] = A[k][j] / A[k][k];
        }
        A[k][k] = 1.0;

        for (int i = k + 1; i < N; i++) {
            for (int j = k + 1; j < N; j++) {
                A[i][j] = A[i][j] - A[i][k] * A[k][j];
            }
            A[i][k] = 0;
        }
    }
}

/*void* LU_threadFunc(void* param) {
    threadParam_t* p = (threadParam_t*)param;
    int k = p->k;           //消去的轮次
    int t_id = p->t_id;     //线程
    int i = k + t_id + 1;   //获取任务

    for (int j = k + 1; j < N; j++) {
        A[i][j] = A[i][j] - A[i][k] * A[k][j];
    }
    A[i][k] = 0;
    pthread_exit(NULL);
    return NULL;
}


void LU_pthread_dynamic() {    //串行，动态分配线程
    for (int k = 0; k < N; k++) {
        for (int j = k + 1; j < N; j++) {
            A[k][j] = A[k][j] / A[k][k];
        }
        A[k][k] = 1.0;

        int thread_cnt = N - 1 - k;
        pthread_t* handle = (pthread_t*)malloc(thread_cnt * sizeof(pthread_t));
        threadParam_t* param = (threadParam_t*)malloc(thread_cnt * sizeof(threadParam_t));

        for (int t_id = 0; t_id < thread_cnt; t_id++) {//分配任务
            param[t_id].k = k;
            param[t_id].t_id = t_id;
        }

        for (int t_id = 0; t_id < thread_cnt; t_id++) {
            pthread_create(&handle[t_id], NULL, LU_threadFunc, &param[t_id]);//
        }

        for (int t_id = 0; t_id < thread_cnt; t_id++) {
            pthread_join(handle[t_id], NULL);
        }
        free(handle);
        free(param);
    }

}
*/
void avx_optimized() {            //avx优化算法
    for (int k = 0; k < N; k++) {
        __m256 vt = _mm256_set1_ps(A[k][k]);
        int j = 0;
        for (j = k + 1; j + 8 <= N; j += 8) {
            __m256 va = _mm256_loadu_ps(&A[k][j]);
            va = _mm256_div_ps(va, vt);
            _mm256_storeu_ps(&A[k][j], va);
        }
        for (; j < N; j++) {
            A[k][j] = A[k][j] / A[k][k];
        }
        A[k][k] = 1.0;
        for (int i = k + 1; i < N; i++) {
            __m256 vaik = _mm256_set1_ps(A[i][k]);
            for (j = k + 1; j + 8 <= N; j += 8) {
                __m256 vakj = _mm256_loadu_ps(&A[k][j]);
                __m256 vaij = _mm256_loadu_ps(&A[i][j]);
                __m256 vx = _mm256_mul_ps(vakj, vaik);
                vaij = _mm256_sub_ps(vaij, vx);
                _mm256_storeu_ps(&A[i][j], vaij);
            }
            for (; j < N; j++) {
                A[i][j] = A[i][j] - A[i][k] * A[k][j];
            }
            A[i][k] = 0;

        }
    }
}
/*

void* avx_threadFunc(void* param) {
    threadParam_t* p = (threadParam_t*)param;
    int k = p->k;           //消去的轮次
    int t_id = p->t_id;     //线程
    int i = k + t_id + 1;   //获取任务

    __m256 vaik = _mm256_set1_ps(A[i][k]);
    int j;
    for (j = k + 1; j + 8 <= N; j += 8) {
        __m256 vakj = _mm256_loadu_ps(&A[k][j]);
        __m256 vaij = _mm256_loadu_ps(&A[i][j]);
        __m256 vx = _mm256_mul_ps(vakj, vaik);
        vaij = _mm256_sub_ps(vaij, vx);
        _mm256_storeu_ps(&A[i][j], vaij);
    }
    for (; j < N; j++) {
        A[i][j] = A[i][j] - A[i][k] * A[k][j];
    }
    A[i][k] = 0;
    pthread_exit(NULL);
    return NULL;
}

void avx_dynamic() {            //avx优化算法,动态分配
    for (int k = 0; k < N; k++) {
        __m256 vt = _mm256_set1_ps(A[k][k]);
        int j = 0;
        for (j = k + 1; j + 8 <= N; j += 8) {
            __m256 va = _mm256_loadu_ps(&A[k][j]);
            va = _mm256_div_ps(va, vt);
            _mm256_storeu_ps(&A[k][j], va);
        }
        for (; j < N; j++) {
            A[k][j] = A[k][j] / A[k][k];
        }
        A[k][k] = 1.0;

        int thread_cnt = N - 1 - k;
        pthread_t* handle = (pthread_t*)malloc(thread_cnt * sizeof(pthread_t));
        threadParam_t* param = (threadParam_t*)malloc(thread_cnt * sizeof(threadParam_t));

        for (int t_id = 0; t_id < thread_cnt; t_id++) {//分配任务
            param[t_id].k = k;
            param[t_id].t_id = t_id;
        }

        for (int t_id = 0; t_id < thread_cnt; t_id++) {
            pthread_create(&handle[t_id], NULL, avx_threadFunc, &param[t_id]);
        }

        for (int t_id = 0; t_id < thread_cnt; t_id++) {
            pthread_join(handle[t_id], NULL);
        }
        free(handle);
        free(param);
    }


}

void* LU_threadFunc_NUM_THREADS(void* param) {
    threadParam_t* p = (threadParam_t*)param;
    int k = p->k;           //消去的轮次
    int t_id = p->t_id;     //线程
    int i = k + t_id + 1;   //获取任务
    for (; i < N; i += NUM_THREADS) {
        for (int j = k + 1; j < N; j++) {
            A[i][j] = A[i][j] - A[i][k] * A[k][j];
        }
        A[i][k] = 0;
    }
    pthread_exit(NULL);
}

void LU_pthread_dynamic_NUM_THREADS() {    //串行，动态分配8线程
    for (int k = 0; k < N; k++) {
        for (int j = k + 1; j < N; j++) {
            A[k][j] = A[k][j] / A[k][k];
        }
        A[k][k] = 1.0;

        int thread_cnt = NUM_THREADS;
        pthread_t* handle = (pthread_t*)malloc(thread_cnt * sizeof(pthread_t));
        threadParam_t* param = (threadParam_t*)malloc(thread_cnt * sizeof(threadParam_t));

        for (int t_id = 0; t_id < thread_cnt; t_id++) {//分配任务
            param[t_id].k = k;
            param[t_id].t_id = t_id;
        }

        for (int t_id = 0; t_id < thread_cnt; t_id++) {
            pthread_create(&handle[t_id], NULL, LU_threadFunc_NUM_THREADS, &param[t_id]);
        }

        for (int t_id = 0; t_id < thread_cnt; t_id++) {
            pthread_join(handle[t_id], NULL);
        }
        free(handle);
        free(param);
    }

}

void* avx_threadFunc_NUM_THREADS(void* param) {  //avx动态分配8线程
    threadParam_t* p = (threadParam_t*)param;
    int k = p->k;           //消去的轮次
    int t_id = p->t_id;     //线程
    int i = k + t_id + 1;   //获取任务

    for (; i < N; i += NUM_THREADS) {
        __m256 vaik = _mm256_set1_ps(A[i][k]);
        int j;
        for (j = k + 1; j + 8 <= N; j += 8) {
            __m256 vakj = _mm256_loadu_ps(&A[k][j]);
            __m256 vaij = _mm256_loadu_ps(&A[i][j]);
            __m256 vx = _mm256_mul_ps(vakj, vaik);
            vaij = _mm256_sub_ps(vaij, vx);
            _mm256_storeu_ps(&A[i][j], vaij);
        }
        for (; j < N; j++) {
            A[i][j] = A[i][j] - A[i][k] * A[k][j];
        }
        A[i][k] = 0;
    }
    pthread_exit(NULL);
    return NULL;
}

void avx_dynamic_NUM_THREADS() {            //avx,动态分配8线程
    for (int k = 0; k < N; k++) {
        __m256 vt = _mm256_set1_ps(A[k][k]);
        int j = 0;
        for (j = k + 1; j + 8 <= N; j += 8) {
            __m256 va = _mm256_loadu_ps(&A[k][j]);
            va = _mm256_div_ps(va, vt);
            _mm256_storeu_ps(&A[k][j], va);
        }
        for (; j < N; j++) {
            A[k][j] = A[k][j] / A[k][k];
        }
        A[k][k] = 1.0;

        int thread_cnt = NUM_THREADS;
        pthread_t* handle = (pthread_t*)malloc(thread_cnt * sizeof(pthread_t));
        threadParam_t* param = (threadParam_t*)malloc(thread_cnt * sizeof(threadParam_t));

        for (int t_id = 0; t_id < thread_cnt; t_id++) {//分配任务
            param[t_id].k = k;
            param[t_id].t_id = t_id;
        }

        for (int t_id = 0; t_id < thread_cnt; t_id++) {
            pthread_create(&handle[t_id], NULL, avx_threadFunc_NUM_THREADS, &param[t_id]);
        }

        for (int t_id = 0; t_id < thread_cnt; t_id++) {
            pthread_join(handle[t_id], NULL);
        }
        free(handle);
        free(param);
    }
}

void* LU_sem_threadFunc(void* param) {  //平凡，动态8线程+信号量
    threadParam_t* p = (threadParam_t*)param;
    int t_id = p->t_id;

    for (int k = 0; k < N; k++) {
        sem_wait(&sem_workstart[t_id]);//阻塞，等待主线程除法完成

        for (int i = k + 1 + t_id; i < N; i += NUM_THREADS) {
            for (int j = k + 1; j < N; j++) {
                A[i][j] = A[i][j] - A[i][k] * A[k][j];
            }
            A[i][k] = 0;
        }

        sem_post(&sem_main);        //唤醒主线程
        sem_wait(&sem_workend[t_id]);  //阻塞，等待主线程唤醒进入下一轮

    }
    pthread_exit(NULL);
    return NULL;
}

void LU_sem_static() {
    sem_init(&sem_main, 0, 0); //初始化信号量
    for (int i = 0; i < NUM_THREADS; i++) {
        sem_init(&sem_workend[i], 0, 0);
        sem_init(&sem_workstart[i], 0, 0);
    }
    pthread_t* handle = (pthread_t*)malloc(NUM_THREADS * sizeof(pthread_t));
    threadParam_t* param = (threadParam_t*)malloc(NUM_THREADS * sizeof(threadParam_t));
    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        param[t_id].t_id = t_id;
        param[t_id].k = 0;
        pthread_create(&handle[t_id], NULL, LU_sem_threadFunc, &param[t_id]);

    }

    for (int k = 0; k < N; k++) {

        for (int j = k + 1; j < N; j++) {
            A[k][j] = A[k][j] / A[k][k];
        }
        A[k][k] = 1.0;

        for (int t_id = 0; t_id < NUM_THREADS; t_id++) {  //唤起子线程
            sem_post(&sem_workstart[t_id]);
        }

        for (int t_id = 0; t_id < NUM_THREADS; t_id++) {  //主线程睡眠
            sem_wait(&sem_main);
        }

        for (int t_id = 0; t_id < NUM_THREADS; t_id++) {  //再次唤起工作线程，进入下一轮消去
            sem_post(&sem_workend[t_id]);
        }

    }
    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        pthread_join(handle[t_id], NULL);
    }
    sem_destroy(&sem_main);    //销毁线程
    for (int t_id = 0; t_id < NUM_THREADS; t_id++)
        sem_destroy(&sem_workstart[t_id]);
    for (int t_id = 0; t_id < NUM_THREADS; t_id++)
        sem_destroy(&sem_workend[t_id]);

    free(handle);
    free(param);
}

void* sem_threadFunc(void* param) {    //静态线程+信号量
    threadParam_t* p = (threadParam_t*)param;
    int t_id = p->t_id;

    for (int k = 0; k < N; k++) {
        sem_wait(&sem_workstart[t_id]);//阻塞，等待主线程除法完成

        for (int i = k + 1 + t_id; i < N; i += NUM_THREADS) {
            __m256 vaik = _mm256_set1_ps(A[i][k]);
            int j;
            for (j = k + 1; j + 8 <= N; j += 8) {
                __m256 vakj = _mm256_loadu_ps(&A[k][j]);
                __m256 vaij = _mm256_loadu_ps(&A[i][j]);
                __m256 vx = _mm256_mul_ps(vakj, vaik);
                vaij = _mm256_sub_ps(vaij, vx);
                _mm256_storeu_ps(&A[i][j], vaij);
            }
            for (; j < N; j++) {
                A[i][j] = A[i][j] - A[i][k] * A[k][j];
            }
            A[i][k] = 0.0;
        }

        sem_post(&sem_main);        //唤醒主线程
        sem_wait(&sem_workend[t_id]);  //阻塞，等待主线程唤醒进入下一轮

    }
    pthread_exit(NULL);
    return NULL;
}

void sem_static() {
    sem_init(&sem_main, 0, 0); //初始化信号量
    for (int i = 0; i < NUM_THREADS; i++) {
        sem_init(&sem_workend[i], 0, 0);
        sem_init(&sem_workstart[i], 0, 0);
    }
    pthread_t* handle = (pthread_t*)malloc(NUM_THREADS * sizeof(pthread_t));
    threadParam_t* param = (threadParam_t*)malloc(NUM_THREADS * sizeof(threadParam_t));
    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        param[t_id].t_id = t_id;
        param[t_id].k = 0;
        pthread_create(&handle[t_id], NULL, sem_threadFunc, &param[t_id]);

    }

    for (int k = 0; k < N; k++) {

        __m256 vt = _mm256_set1_ps(A[k][k]);
        int j = 0;
        for (j = k + 1; j + 8 <= N; j += 8) {
            __m256 va = _mm256_loadu_ps(&A[k][j]);
            va = _mm256_div_ps(va, vt);
            _mm256_storeu_ps(&A[k][j], va);
        }
        for (; j < N; j++) {
            A[k][j] = A[k][j] / A[k][k];
        }
        A[k][k] = 1.0;

        for (int t_id = 0; t_id < NUM_THREADS; t_id++) {  //唤起子线程
            sem_post(&sem_workstart[t_id]);
        }

        for (int t_id = 0; t_id < NUM_THREADS; t_id++) {  //主线程睡眠
            sem_wait(&sem_main);
        }

        for (int t_id = 0; t_id < NUM_THREADS; t_id++) {  //再次唤起工作线程，进入下一轮消去
            sem_post(&sem_workend[t_id]);
        }

    }
    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        pthread_join(handle[t_id], NULL);
    }
    sem_destroy(&sem_main);    //销毁线程
    for (int t_id = 0; t_id < NUM_THREADS; t_id++)
        sem_destroy(&sem_workstart[t_id]);
    for (int t_id = 0; t_id < NUM_THREADS; t_id++)
        sem_destroy(&sem_workend[t_id]);

    free(handle);
    free(param);

}

void* LU_sem_tri_thread(void* param) {
    threadParam_t* p = (threadParam_t*)param;
    int t_id = p->t_id;

    for (int k = 0; k < N; k++) { //0号线程做除法，其余等待

        if (t_id == 0) {
            for (int j = k + 1; j < N; j++) {
                A[k][j] = A[k][j] / A[k][k];
            }
            A[k][k] = 1.0;
        }
        else
            sem_wait(&sem_Division[t_id - 1]);

        if (t_id == 0) {
            for (int i = 0; i < NUM_THREADS - 1; i++) {   //主线程唤醒其余线程
                sem_post(&sem_Division[i]);
            }
        }

        for (int i = k + 1 + t_id; i < N; i += NUM_THREADS) {
            for (int j = k + 1; j < N; j++) {
                A[i][j] = A[i][j] - A[i][k] * A[k][j];
            }
            A[i][k] = 0;
        }

        if (t_id == 0) {
            for (int i = 0; i < NUM_THREADS - 1; i++) {
                sem_wait(&sem_leader);
            }
            for (int i = 0; i < NUM_THREADS - 1; i++) {
                sem_post(&sem_Elimination[i]);
            }
        }
        else {
            sem_post(&sem_leader);
            sem_wait(&sem_Elimination[t_id - 1]);
        }

    }

    pthread_exit(NULL);
    return NULL;
}

void LU_sem_tri() {
    sem_init(&sem_leader, 0, 0);
    for (int i = 0; i < NUM_THREADS; i++) {
        sem_init(&sem_Division[i], 0, 0);
        sem_init(&sem_Elimination[i], 0, 0);
    }
    pthread_t* handle = (pthread_t*)malloc(NUM_THREADS * sizeof(pthread_t));
    threadParam_t* param = (threadParam_t*)malloc(NUM_THREADS * sizeof(threadParam_t));

    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        param[t_id].t_id = t_id;
        param[t_id].k = 0;
        pthread_create(&handle[t_id], NULL, LU_sem_tri_thread, &param[t_id]);

    }
    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        pthread_join(handle[t_id], NULL);
    }
    sem_destroy(&sem_main);    //销毁线程
    for (int t_id = 0; t_id < NUM_THREADS; t_id++)
        sem_destroy(&sem_workstart[t_id]);
    for (int t_id = 0; t_id < NUM_THREADS; t_id++)
        sem_destroy(&sem_workend[t_id]);

    free(handle);
    free(param);
}

void* sem_triplecircle_thread(void* param) { //静态线程+信号量+三重循环
    threadParam_t* p = (threadParam_t*)param;
    int t_id = p->t_id;

    for (int k = 0; k < N; k++) { //0号线程做除法，其余等待

        if (t_id == 0) {
            __m256 vt = _mm256_set1_ps(A[k][k]);
            int j = 0;
            for (j = k + 1; j + 8 <= N; j += 8) {
                __m256 va = _mm256_loadu_ps(&A[k][j]);
                va = _mm256_div_ps(va, vt);
                _mm256_storeu_ps(&A[k][j], va);
            }
            for (; j < N; j++) {
                A[k][j] = A[k][j] / A[k][k];
            }
            A[k][k] = 1.0;
        }
        else
            sem_wait(&sem_Division[t_id - 1]);

        if (t_id == 0) {
            for (int i = 0; i < NUM_THREADS - 1; i++) {   //主线程唤醒其余线程
                sem_post(&sem_Division[i]);
            }
        }

        for (int i = k + 1 + t_id; i < N; i += NUM_THREADS) {
            __m256 vaik = _mm256_set1_ps(A[i][k]);
            int j;
            for (j = k + 1; j + 8 <= N; j += 8) {
                __m256 vakj = _mm256_loadu_ps(&A[k][j]);
                __m256 vaij = _mm256_loadu_ps(&A[i][j]);
                __m256 vx = _mm256_mul_ps(vakj, vaik);
                vaij = _mm256_sub_ps(vaij, vx);
                _mm256_storeu_ps(&A[i][j], vaij);
            }
            for (; j < N; j++) {
                A[i][j] = A[i][j] - A[i][k] * A[k][j];
            }
            A[i][k] = 0.0;
        }

        if (t_id == 0) {
            for (int i = 0; i < NUM_THREADS - 1; i++) {
                sem_wait(&sem_leader);
            }
            for (int i = 0; i < NUM_THREADS - 1; i++) {
                sem_post(&sem_Elimination[i]);
            }
        }
        else {
            sem_post(&sem_leader);
            sem_wait(&sem_Elimination[t_id - 1]);
        }

    }

    pthread_exit(NULL);
    return NULL;
}

void sem_triplecircle() {
    sem_init(&sem_leader, 0, 0);
    for (int i = 0; i < NUM_THREADS; i++) {
        sem_init(&sem_Division[i], 0, 0);
        sem_init(&sem_Elimination[i], 0, 0);
    }
    pthread_t* handle = (pthread_t*)malloc(NUM_THREADS * sizeof(pthread_t));
    threadParam_t* param = (threadParam_t*)malloc(NUM_THREADS * sizeof(threadParam_t));

    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        param[t_id].t_id = t_id;
        param[t_id].k = 0;
        pthread_create(&handle[t_id], NULL, sem_triplecircle_thread, &param[t_id]);

    }
    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        pthread_join(handle[t_id], NULL);
    }
    sem_destroy(&sem_main);    //销毁线程
    for (int t_id = 0; t_id < NUM_THREADS; t_id++)
        sem_destroy(&sem_workstart[t_id]);
    for (int t_id = 0; t_id < NUM_THREADS; t_id++)
        sem_destroy(&sem_workend[t_id]);

    free(handle);
    free(param);
}
*/
void* LU_barrier_thread(void* param) {
    threadParam_t* p = (threadParam_t*)param;
    int t_id = p->t_id;

    for (int k = 0; k < N; k++) { //0号线程做除法
        if (t_id == 0) {
            for (int j = k + 1; j < N; j++) {
                A[k][j] = A[k][j] / A[k][k];
            }
            A[k][k] = 1.0;
        }

        pthread_barrier_wait(&barrier_Division);//第一个同步点

        for (int i = k + 1 + t_id; i < N; i += NUM_THREADS) {
            for (int j = k + 1; j < N; j++) {
                A[i][j] = A[i][j] - A[i][k] * A[k][j];
            }
            A[i][k] = 0;
        }

        pthread_barrier_wait(&barrier_Elimination);//第二个同步点


    }
    pthread_exit(NULL);
    return NULL;
}

void LU_barrier() {
    pthread_barrier_init(&barrier_Division, NULL, NUM_THREADS);
    pthread_barrier_init(&barrier_Elimination, NULL, NUM_THREADS);

    pthread_t* handle = (pthread_t*)malloc(NUM_THREADS * sizeof(pthread_t));
    threadParam_t* param = (threadParam_t*)malloc(NUM_THREADS * sizeof(threadParam_t));

    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        param[t_id].t_id = t_id;
        param[t_id].k = 0;
        pthread_create(&handle[t_id], NULL, LU_barrier_thread, &param[t_id]);

    }
    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        pthread_join(handle[t_id], NULL);
    }

    pthread_barrier_destroy(&barrier_Division);
    pthread_barrier_destroy(&barrier_Elimination);

    free(handle);
    free(param);
}

void* barrier_threadFunc(void* param) {
    threadParam_t* p = (threadParam_t*)param;
    int t_id = p->t_id;

    for (int k = 0; k < N; k++) { //0号线程做除法
        if (t_id == 0) {
            __m256 vt = _mm256_set1_ps(A[k][k]);
            int j = 0;
            for (j = k + 1; j + 8 <= N; j += 8) {
                __m256 va = _mm256_loadu_ps(&A[k][j]);
                va = _mm256_div_ps(va, vt);
                _mm256_storeu_ps(&A[k][j], va);
            }
            for (; j < N; j++) {
                A[k][j] = A[k][j] / A[k][k];
            }
            A[k][k] = 1.0;
        }

        pthread_barrier_wait(&barrier_Division);//第一个同步点

        for (int i = k + 1 + t_id; i < N; i += NUM_THREADS) {
            __m256 vaik = _mm256_set1_ps(A[i][k]);
            int j;
            for (j = k + 1; j + 8 <= N; j += 8) {
                __m256 vakj = _mm256_loadu_ps(&A[k][j]);
                __m256 vaij = _mm256_loadu_ps(&A[i][j]);
                __m256 vx = _mm256_mul_ps(vakj, vaik);
                vaij = _mm256_sub_ps(vaij, vx);
                _mm256_storeu_ps(&A[i][j], vaij);
            }
            for (; j < N; j++) {
                A[i][j] = A[i][j] - A[i][k] * A[k][j];
            }
            A[i][k] = 0.0;
        }

        pthread_barrier_wait(&barrier_Elimination);//第二个同步点


    }
    pthread_exit(NULL);
    return NULL;
}

void barrier_static()
{
    pthread_barrier_init(&barrier_Division, NULL, NUM_THREADS);
    pthread_barrier_init(&barrier_Elimination, NULL, NUM_THREADS);

    pthread_t* handle = (pthread_t*)malloc(NUM_THREADS * sizeof(pthread_t));
    threadParam_t* param = (threadParam_t*)malloc(NUM_THREADS * sizeof(threadParam_t));

    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        param[t_id].t_id = t_id;
        param[t_id].k = 0;
        pthread_create(&handle[t_id], NULL, barrier_threadFunc, &param[t_id]);

    }
    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        pthread_join(handle[t_id], NULL);
    }

    pthread_barrier_destroy(&barrier_Division);
    pthread_barrier_destroy(&barrier_Elimination);

    free(handle);
    free(param);
}

void cal(void(*func)()) {
    A_init();
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    func();
    QueryPerformanceCounter((LARGE_INTEGER*)&tail);

}

int main() {

    for(int i=0;i<10;i++){

      cout<<i+1<<endl;

    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);

    cal(LU);
    cout << "平凡算法串行耗时：" << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();

    cal(avx_optimized);
    cout << "avx优化串行耗时： " << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();
/*
    cal(LU_pthread_dynamic);
    cout << "平凡pthread耗时：" << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();

    cal(avx_dynamic);
    cout << "avx优化pthread耗时：" << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();

    cal(LU_pthread_dynamic_NUM_THREADS);
    cout << "平凡算法pthread动态8线程： " << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();

    cal(avx_dynamic_NUM_THREADS);
    cout << "avx优化pthread动态8线程： " << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();

    cal(LU_sem_static);
    cout << "平凡算法静态8线程+信号量： " << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();

    cal(sem_static);
    cout << "avx静态8线程+信号量： " << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();

    cal(LU_sem_tri);
    cout << "平凡算法静态8线程+信号量三重循环： " << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();

    cal(sem_triplecircle);
    cout << "avx静态8线程+信号量三重循环： " << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();
*/
    cal(LU_barrier);
    cout << "平凡算法静态barrier： " << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();

    cal(barrier_static);
    cout << "avx静态barrier：" << (tail - head) * 1000 / freq << "ms" << endl;
    deleteA();



    }
}

