#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libusb-1.0/libusb.h>
#include <pthread.h>

#define MAX_DEVICES 6
#define HID_GET_DESCRIPTOR 0x06
#define HID_REPORT_DESCRIPTOR 0x22

typedef struct {
    int device_index;
    libusb_device_handle* handle;
    char device_name[128];  // Nombre del dispositivo
    int vendor_id;
    int product_id;
} Device;

typedef struct {
    int device_id;
    int vendor_id;
    int product_id;
    char serial_number[64];
    char event_type[32];
    char type[32];
    char value[256];
} DeviceEvent;

extern Device devices[MAX_DEVICES];
extern pthread_mutex_t devices_mutex;

void clean_up_devices();
void* read_device_data(void* arg);
void detect_devices(void (*send_data_func)(DeviceEvent));
const Device* get_device(int index);
int get_device_count();

#ifdef __cplusplus
}
#endif

#endif // DEVICE_MANAGER_H
