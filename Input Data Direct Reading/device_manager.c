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
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            pthread_testcancel(); // Verificar si se solicitó la cancelación del hilo
            DeviceEvent device_event;
            memset(&device_event, 0, sizeof(DeviceEvent));

            if (event.type == SDL_JOYAXISMOTION || event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP || event.type == SDL_JOYHATMOTION) {
                pthread_mutex_lock(&devices_mutex);
                int instance_id = event.jaxis.which;
                for (int i = 0; i < MAX_DEVICES; ++i) {
                    if (devices[i].joystick != NULL && SDL_JoystickInstanceID(devices[i].joystick) == instance_id) {
                        device_event.device_id = devices[i].device_index;
                        device_event.vendor_id = devices[i].vendor_id;
                        device_event.product_id = devices[i].product_id;
                        snprintf(device_event.serial_number, sizeof(device_event.serial_number), "%s", devices[i].device_name);

                        switch (event.type) {
                            case SDL_JOYAXISMOTION:
                                snprintf(device_event.event_type, sizeof(device_event.event_type), "Axis %d", event.jaxis.axis);
                                snprintf(device_event.value, sizeof(device_event.value), "%d", event.jaxis.value);
                                break;
                            case SDL_JOYBUTTONDOWN:
                                snprintf(device_event.event_type, sizeof(device_event.event_type), "Button %d Down", event.jbutton.button);
                                snprintf(device_event.value, sizeof(device_event.value), "Down");
                                break;
                            case SDL_JOYBUTTONUP:
                                snprintf(device_event.event_type, sizeof(device_event.event_type), "Button %d Up", event.jbutton.button);
                                snprintf(device_event.value, sizeof(device_event.value), "Up");
                                break;
                            case SDL_JOYHATMOTION:
                                snprintf(device_event.event_type, sizeof(device_event.event_type), "Hat %d", event.jhat.hat);
                                snprintf(device_event.value, sizeof(device_event.value), "%d", event.jhat.value);
                                break;
                        }

                        if (send_data) {
                            send_data(device_event);
                        }
                        break;
                    }
                }
                pthread_mutex_unlock(&devices_mutex);
            } else if (event.type == SDL_JOYDEVICEADDED) {
                pthread_mutex_lock(&devices_mutex);
                int device_index = event.jdevice.which;
                const char* device_name = SDL_JoystickNameForIndex(device_index);
                SDL_Joystick* joystick = SDL_JoystickOpen(device_index);
                if (joystick != NULL) {
                    int instance_id = SDL_JoystickInstanceID(joystick);
                    int vendor_id = SDL_JoystickGetVendor(joystick);
                    int product_id = SDL_JoystickGetProduct(joystick);

                    for (int i = 0; i < MAX_DEVICES; ++i) {
                        if (devices[i].joystick == NULL) {
                            strncpy(devices[i].device_name, device_name != NULL ? device_name : "Unknown", sizeof(devices[i].device_name) - 1);
                            devices[i].device_index = i;
                            devices[i].joystick = joystick;
                            devices[i].vendor_id = vendor_id;
                            devices[i].product_id = product_id;

                            DeviceEvent connect_event;
                            memset(&connect_event, 0, sizeof(DeviceEvent));
                            connect_event.device_id = i;
                            connect_event.vendor_id = vendor_id;
                            connect_event.product_id = product_id;
                            snprintf(connect_event.serial_number, sizeof(connect_event.serial_number), "%s", devices[i].device_name);
                            snprintf(connect_event.event_type, sizeof(connect_event.event_type), "connected");
                            snprintf(connect_event.type, sizeof(connect_event.type), "Connection");
                            snprintf(connect_event.value, sizeof(connect_event.value), "%s", devices[i].device_name);
                            if (send_data) {
                                send_data(connect_event);
                            }

                            break;
                        }
                    }
                } else {
                    fprintf(stderr, "No se pudo abrir el joystick %d: %s\n", device_index, SDL_GetError());
                }
                pthread_mutex_unlock(&devices_mutex);
            } else if (event.type == SDL_JOYDEVICEREMOVED) {
                pthread_mutex_lock(&devices_mutex);
                int instance_id = event.jdevice.which;
                for (int i = 0; i < MAX_DEVICES; ++i) {
                    if (devices[i].joystick != NULL && SDL_JoystickInstanceID(devices[i].joystick) == instance_id) {
                        SDL_JoystickClose(devices[i].joystick);
                        devices[i].joystick = NULL;
                        DeviceEvent disconnect_event;
                        memset(&disconnect_event, 0, sizeof(DeviceEvent));
                        snprintf(disconnect_event.event_type, sizeof(disconnect_event.event_type), "disconnected");
                        disconnect_event.device_id = devices[i].device_index;
                        disconnect_event.vendor_id = devices[i].vendor_id;
                        disconnect_event.product_id = devices[i].product_id;
                        snprintf(disconnect_event.serial_number, sizeof(disconnect_event.serial_number), "%s", devices[i].device_name);
                        snprintf(disconnect_event.type, sizeof(disconnect_event.type), "Disconnection");
                        snprintf(disconnect_event.value, sizeof(disconnect_event.value), "%s", devices[i].device_name);
                        if (send_data) {
                            send_data(disconnect_event);
                        }
                        break;
                    }
                }
                pthread_mutex_unlock(&devices_mutex);
            }
        }
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
