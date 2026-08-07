#include <pthread.h>

static pthread_mutex_t spiffs_mutex;

void spiffs_mutex_lock() {
	pthread_mutex_lock(&spiffs_mutex);
}

void spiffs_mutex_unlock() {
	pthread_mutex_unlock(&spiffs_mutex);
}

void spiffs_mutex_init(void) {
	pthread_mutex_init(&spiffs_mutex, NULL);
}
