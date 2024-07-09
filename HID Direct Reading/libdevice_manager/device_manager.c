#include "device_manager.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <libusb-1.0/libusb.h>
#include <unistd.h>

void (*send_data)(DeviceEvent event) = NULL;

Device devices[MAX_DEVICES];
pthread_mutex_t devices_mutex = PTHREAD_MUTEX_INITIALIZER;

void clean_up_devices() {
    pthread_mutex_lock(&devices_mutex);
    for (int i = 0; i < MAX_DEVICES; ++i) {
        if (devices[i].handle != NULL) {
            libusb_release_interface(devices[i].handle, 0);
            libusb_close(devices[i].handle);
            devices[i].handle = NULL;
        }
    }
    pthread_mutex_unlock(&devices_mutex);
    libusb_exit(NULL);
    printf("Dispositivos limpiados y libusb cerrada.\n");
}

void print_hid_report_descriptor(const unsigned char *data, int length) {
    printf("HID Report Descriptor:\n");
    for (int i = 0; i < length; ++i) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

void send_hid_report_descriptor(libusb_device_handle *handle, int interface_number, DeviceEvent *connect_event) {
    unsigned char data[256];
    int res = libusb_control_transfer(handle,
                                      LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE,
                                      HID_GET_DESCRIPTOR,
                                      (HID_REPORT_DESCRIPTOR << 8),
                                      interface_number,
                                      data,
                                      sizeof(data),
                                      1000);

    if (res < 0) {
        fprintf(stderr, "Control transfer failed: %s\n", libusb_strerror(res));
    } else {
        print_hid_report_descriptor(data, res);
        connect_event->value[0] = '\0'; // Limpiar el valor previo
        for (int i = 0; i < res; ++i) {
            char byte_str[4];
            snprintf(byte_str, sizeof(byte_str), "%02x ", data[i]);
            strncat(connect_event->value, byte_str, sizeof(connect_event->value) - strlen(connect_event->value) - 1);
        }
        if (send_data) {
            send_data(*connect_event);
        }
    }
}

void* read_device_data(void* arg) {
    Device* device = (Device*)arg;
    libusb_device_handle *handle = device->handle;
    unsigned char data[256];
    int actual_length;

    while (1) {
        int res = libusb_interrupt_transfer(handle, LIBUSB_ENDPOINT_IN | 1, data, sizeof(data), &actual_length, 1000);
        if (res == 0 && actual_length > 0) {
            DeviceEvent data_event;
            memset(&data_event, 0, sizeof(DeviceEvent));
            data_event.device_id = device->device_index;
            data_event.vendor_id = device->vendor_id;
            data_event.product_id = device->product_id;
            snprintf(data_event.serial_number, sizeof(data_event.serial_number), "%s", device->device_name);
            data_event.value[0] = '\0'; // Limpiar el valor previo
            for (int i = 0; i < actual_length; ++i) {
                char byte_str[4];
                snprintf(byte_str, sizeof(byte_str), "%02x ", data[i]);
                strncat(data_event.value, byte_str, sizeof(data_event.value) - strlen(data_event.value) - 1);
            }
            if (send_data) {
                send_data(data_event);
            }
        } else if (res != 0) {
            fprintf(stderr, "Interrupt transfer failed: %s\n", libusb_strerror(res));
            break;  // Salir del bucle si falla la transferencia
        }
    }

    // Si sale del bucle, el dispositivo probablemente se ha desconectado
    pthread_mutex_lock(&devices_mutex);
    libusb_release_interface(device->handle, 0);
    libusb_close(device->handle);
    device->handle = NULL;
    pthread_mutex_unlock(&devices_mutex);
    return NULL;
}

void* monitor_devices(void* /*arg*/) {
    libusb_context *context = NULL;
    libusb_init(&context);
    libusb_device **devices_list;
    ssize_t count;

    while (1) {
        count = libusb_get_device_list(context, &devices_list);
        if (count < 0) {
            fprintf(stderr, "Error getting USB device list\n");
            continue;
        }

        pthread_mutex_lock(&devices_mutex);
        for (ssize_t i = 0; i < count; ++i) {
            libusb_device *device = devices_list[i];
            struct libusb_device_descriptor desc;

            int res = libusb_get_device_descriptor(device, &desc);
            if (res < 0) {
                fprintf(stderr, "Failed to get device descriptor\n");
                continue;
            }

            // Check if it's a HID device
            if (desc.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE) {
                int already_connected = 0;
                for (int j = 0; j < MAX_DEVICES; ++j) {
                    if (devices[j].handle != NULL &&
                        devices[j].vendor_id == desc.idVendor &&
                        devices[j].product_id == desc.idProduct) {
                        already_connected = 1;
                        break;
                    }
                }

                if (already_connected) {
                    continue;
                }

                libusb_device_handle *handle;
                res = libusb_open(device, &handle);
                if (res < 0) {
                    fprintf(stderr, "Failed to open device: %s\n", libusb_strerror(res));
                    continue;
                }

                // Detach the kernel driver if necessary
                if (libusb_kernel_driver_active(handle, 0) == 1) {
                    res = libusb_detach_kernel_driver(handle, 0);
                    if (res < 0) {
                        fprintf(stderr, "Failed to detach kernel driver: %s\n", libusb_strerror(res));
                        libusb_close(handle);
                        continue;
                    }
                }

                res = libusb_claim_interface(handle, 0);
                if (res < 0) {
                    fprintf(stderr, "Failed to claim interface: %s\n", libusb_strerror(res));
                    libusb_close(handle);
                    continue;
                }

                for (int j = 0; j < MAX_DEVICES; ++j) {
                    if (devices[j].handle == NULL) {
                        devices[j].handle = handle;
                        devices[j].vendor_id = desc.idVendor;
                        devices[j].product_id = desc.idProduct;
                        snprintf(devices[j].device_name, sizeof(devices[j].device_name), "%04x:%04x", desc.idVendor, desc.idProduct);
                        devices[j].device_index = j;

                        DeviceEvent connect_event;
                        memset(&connect_event, 0, sizeof(DeviceEvent));
                        connect_event.device_id = j;
                        connect_event.vendor_id = desc.idVendor;
                        connect_event.product_id = desc.idProduct;
                        snprintf(connect_event.serial_number, sizeof(connect_event.serial_number), "%04x:%04x", desc.idVendor, desc.idProduct);
                        snprintf(connect_event.event_type, sizeof(connect_event.event_type), "connected");
                        snprintf(connect_event.type, sizeof(connect_event.type), "Connection");

                        send_hid_report_descriptor(handle, 0, &connect_event);

                        if (send_data) {
                            send_data(connect_event);
                        }

                        pthread_t data_thread;
                        if (pthread_create(&data_thread, NULL, read_device_data, &devices[j]) != 0) {
                            fprintf(stderr, "Failed to create data thread\n");
                        } else {
                            pthread_detach(data_thread);
                        }

                        break;
                    }
                }
            }
        }

        // Detect disconnected devices
        for (int j = 0; j < MAX_DEVICES; ++j) {
            if (devices[j].handle != NULL) {
                struct libusb_device_descriptor desc;
                int res = libusb_get_device_descriptor(libusb_get_device(devices[j].handle), &desc);
                if (res < 0) {
                    fprintf(stderr, "Device %d disconnected\n", j);
                    libusb_release_interface(devices[j].handle, 0);
                    libusb_close(devices[j].handle);
                    devices[j].handle = NULL;

                    DeviceEvent disconnect_event;
                    memset(&disconnect_event, 0, sizeof(DeviceEvent));
                    disconnect_event.device_id = j;
                    disconnect_event.vendor_id = devices[j].vendor_id;
                    disconnect_event.product_id = devices[j].product_id;
                    snprintf(disconnect_event.serial_number, sizeof(disconnect_event.serial_number), "%s", devices[j].device_name);
                    snprintf(disconnect_event.event_type, sizeof(disconnect_event.event_type), "disconnected");
                    snprintf(disconnect_event.type, sizeof(disconnect_event.type), "Disconnection");
                    snprintf(disconnect_event.value, sizeof(disconnect_event.value), "%s", devices[j].device_name);

                    if (send_data) {
                        send_data(disconnect_event);
                    }
                }
            }
        }

        libusb_free_device_list(devices_list, 1);
        pthread_mutex_unlock(&devices_mutex);
        usleep(500000);  // 500ms
    }

    libusb_exit(context);
    return NULL;
}

void detect_devices(void (*send_data_func)(DeviceEvent)) {
    send_data = send_data_func;

    pthread_t monitor_thread;
    if (pthread_create(&monitor_thread, NULL, monitor_devices, NULL) != 0) {
        fprintf(stderr, "Failed to create monitor thread\n");
    } else {
        pthread_detach(monitor_thread);
    }

    printf("Device detection started.\n");
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
        if (devices[i].handle != NULL) {
            count++;
        }
    }
    pthread_mutex_unlock(&devices_mutex);
    return count;
}
