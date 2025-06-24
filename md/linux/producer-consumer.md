

```
#include <pthread.h>

#include <stdio.h>

#include <stdlib.h>

  

#define BUFFER_SIZE 5

  

int buffer[BUFFER_SIZE];

int count = 0;

int in = 0;

int out = 0;

  

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

  

void* producer(void* arg) {

    for (int i = 0; i < 10; i++) {

        pthread_mutex_lock(&mutex);

        while (count == BUFFER_SIZE) {

            pthread_cond_wait(&not_full, &mutex);

        }

        buffer[in] = i;

        in = (in + 1) % BUFFER_SIZE;

        count++;

        pthread_cond_signal(&not_empty);

        pthread_mutex_unlock(&mutex);

    }

    return NULL;

}

  

void* consumer(void* arg) {

    for (int i = 0; i < 10; i++) {

        pthread_mutex_lock(&mutex);

        while (count == 0) {

            pthread_cond_wait(&not_empty, &mutex);

        }

        int item = buffer[out];

        out = (out + 1) % BUFFER_SIZE;

        count--;

        pthread_cond_signal(&not_full);

        pthread_mutex_unlock(&mutex);

        printf("Consumed: %d\n", item);

    }

    return NULL;

}

  

int main() {

    pthread_t producer_t, consumer_t;

    pthread_create(&producer_t, NULL, producer, NULL);

    pthread_create(&consumer_t, NULL, consumer, NULL);

    pthread_join(producer_t, NULL);

    pthread_join(consumer_t, NULL);

    return 0;

}
```