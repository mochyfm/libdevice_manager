#include "device_manager.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <unistd.h>

void (*send_data)(DeviceEvent event) = NULL;

Device devices[MAX_DEVICES];
pthread_mutex_t devices_mutex = PTHREAD_MUTEX_INITIALIZER;

void clean_up_devices() {
    pthread_mutex_lock(&devices_mutex);
    for (int i = 0; i < MAX_DEVICES; ++i) {
        if (devices[i].joystick != NULL) {
            SDL_JoystickClose(devices[i].joystick);
            devices[i].joystick = NULL;
        }
    }
    pthread_mutex_unlock(&devices_mutex);
    SDL_Quit();
    printf("Dispositivos limpiados y SDL cerrada.\n");
}

void* read_device_data(void* /*arg*/) {
    printf("Hilo de lectura de datos de dispositivo iniciado.\n");
    while (1) {
        pthread_mutex_lock(&devices_mutex);
        for (int i = 0; i < MAX_DEVICES; ++i) {
            if (devices[i].joystick != NULL) {
                for (int j = 0; j < SDL_JoystickNumAxes(devices[i].joystick); ++j) {
                    DeviceEvent device_event;
                    memset(&device_event, 0, sizeof(DeviceEvent));
                    device_event.device_id = devices[i].device_index;
                    device_event.vendor_id = devices[i].vendor_id;
                    device_event.product_id = devices[i].product_id;
                    snprintf(device_event.serial_number, sizeof(device_event.serial_number), "%s", devices[i].device_name);
                    snprintf(device_event.event_type, sizeof(device_event.event_type), "Axis %d", j);
                    snprintf(device_event.value, sizeof(device_event.value), "%d", SDL_JoystickGetAxis(devices[i].joystick, j));

                    if (send_data) {
                        send_data(device_event);
                    }
                }

                for (int j = 0; j < SDL_JoystickNumButtons(devices[i].joystick); ++j) {
                    DeviceEvent device_event;
                    memset(&device_event, 0, sizeof(DeviceEvent));
                    device_event.device_id = devices[i].device_index;
                    device_event.vendor_id = devices[i].vendor_id;
                    device_event.product_id = devices[i].product_id;
                    snprintf(device_event.serial_number, sizeof(device_event.serial_number), "%s", devices[i].device_name);
                    snprintf(device_event.event_type, sizeof(device_event.event_type), "Button %d", j);
                    snprintf(device_event.value, sizeof(device_event.value), "%d", SDL_JoystickGetButton(devices[i].joystick, j));

                    if (send_data) {
                        send_data(device_event);
                    }
                }

                for (int j = 0; j < SDL_JoystickNumHats(devices[i].joystick); ++j) {
                    DeviceEvent device_event;
                    memset(&device_event, 0, sizeof(DeviceEvent));
                    device_event.device_id = devices[i].device_index;
                    device_event.vendor_id = devices[i].vendor_id;
                    device_event.product_id = devices[i].product_id;
                    snprintf(device_event.serial_number, sizeof(device_event.serial_number), "%s", devices[i].device_name);
                    snprintf(device_event.event_type, sizeof(device_event.event_type), "Hat %d", j);
                    snprintf(device_event.value, sizeof(device_event.value), "%d", SDL_JoystickGetHat(devices[i].joystick, j));

                    if (send_data) {
                        send_data(device_event);
                    }
                }
            }
        }
        pthread_mutex_unlock(&devices_mutex);
        usleep(10000);  // 10ms
    }
    return NULL;
}

void detect_devices(void (*send_data_func)(DeviceEvent)) {
    send_data = send_data_func;

    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        fprintf(stderr, "No se pudo inicializar SDL: %s\n", SDL_GetError());
        return;
    }

    printf("SDL inicializado para detección de dispositivos.\n");

    // Inicializa el arreglo de dispositivos
    for (int i = 0; i < MAX_DEVICES; ++i) {
        devices[i].device_index = i;
        devices[i].joystick = NULL;
        devices[i].device_name[0] = '\0';
        devices[i].vendor_id = 0;
        devices[i].product_id = 0;
    }

    pthread_t device_thread;
    if (pthread_create(&device_thread, NULL, read_device_data, NULL) != 0) {
        fprintf(stderr, "No se pudo crear el hilo de lectura de datos de dispositivo.\n");
        return;
    }
    pthread_detach(device_thread); // Detach thread to allow resources to be freed upon completion

    printf("Hilo de detección de dispositivos iniciado.\n");
}

const Device* get_device(int index) {
    if (index < 0 || index >= MAX_DEVICES) {
        return NULL;
    }
    return &devices[index];
}

int get_device_count() {
    int count = 0;
    pthread_mutex_lock(&devices_mutex);
    for (int i = 0; i < MAX_DEVICES; ++i) {
        if (devices[i].joystick != NULL) {
            count++;
        }
    }
    pthread_mutex_unlock(&devices_mutex);
    return count;
}
