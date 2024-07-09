#!/usr/bin/env python3
"""PyBluez simple example rfcomm-server.py

Simple demonstration of a server application that uses RFCOMM sockets.

Author: Albert Huang <albert@csail.mit.edu>
$Id: rfcomm-server.py 518 2007-08-10 07:20:07Z albert $
"""

import bluetooth
import time
import ctypes
import threading

# Define the DeviceEvent class
class DeviceEvent(ctypes.Structure):
    _fields_ = [
        ("device_id", ctypes.c_int),
        ("vendor_id", ctypes.c_int),
        ("product_id", ctypes.c_int),
        ("serial_number", ctypes.c_char * 64),
        ("event_type", ctypes.c_char * 32),
        ("type", ctypes.c_char * 32),
        ("value", ctypes.c_char * 256),
    ]

    def __str__(self):
        device_id = self.device_id
        vendor_id = self.vendor_id
        product_id = self.product_id
        device_name = self.serial_number.decode('utf-8').strip()
        event_type = self.event_type.decode('utf-8').strip()
        type_ = self.type.decode('utf-8').strip()
        value = self.value.decode('utf-8').strip()
        return f"Device {device_id}, {device_name}, {vendor_id}, {product_id}, {event_type}, {value}"

# Load the shared library
lib = ctypes.CDLL('./libdevice_manager.so')

# Global flag to indicate if Bluetooth connection is ready
bluetooth_ready = threading.Event()

# Define global client_sock and event to control the device detection thread
client_sock = None
client_info = None
stop_detection_event = threading.Event()

# Define the send_data function in Python
@ctypes.CFUNCTYPE(None, DeviceEvent)
def send_data(event):
    global client_sock, client_info
    message = str(event)
    if client_sock and bluetooth_ready.is_set():
        try:
            client_sock.send(message + "\n")
        except OSError as e:
            print(f"Error sending data: {e}")
            # Reset the bluetooth_ready flag to prevent further sending
            bluetooth_ready.clear()

    if "connected" in message:
        client_info_str = f" ({client_info[0]})" if client_info else ""
        print(f"Connected{client_info_str}: {message}")
    elif "disconnected" in message:
        print(f"Disconnected (red): {message}")
    else:
        print(f"Generated Event: {message}")

# Define the prototype of the detect_devices function
lib.detect_devices.argtypes = [ctypes.CFUNCTYPE(None, DeviceEvent)]
lib.detect_devices.restype = None

# Define library functions
lib.clean_up_devices.argtypes = []
lib.clean_up_devices.restype = None

lib.get_device_count.argtypes = []
lib.get_device_count.restype = ctypes.c_int

class Device(ctypes.Structure):
    _fields_ = [("device_index", ctypes.c_int),
                ("joystick", ctypes.c_void_p),
                ("device_name", ctypes.c_char * 128),
                ("vendor_id", ctypes.c_int),
                ("product_id", ctypes.c_int)]

lib.get_device.argtypes = [ctypes.c_int]
lib.get_device.restype = ctypes.POINTER(Device)

# Bluetooth setup
server_sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
server_sock.bind(("", 22))
server_sock.listen(1)

def wait_for_connection():
    global client_sock, client_info
    print("Waiting for connection on RFCOMM channel 22")
    client_sock, client_info = server_sock.accept()
    print(f"Connected (blue {client_info[0]})")
    bluetooth_ready.set()

def detect_devices_thread():
    print("Device detection thread started.")
    lib.detect_devices(send_data)
    descriptor_sent = False  # Flag to track if the descriptor has been sent
    while not stop_detection_event.is_set():
        if not bluetooth_ready.is_set():
            descriptor_sent = False  # Reset the flag when Bluetooth is not ready
        else:
            if not descriptor_sent:
                device_count = lib.get_device_count()
                for i in range(device_count):
                    device = lib.get_device(i)
                    if device and device.contents:
                        # Send HID report descriptor here if not sent
                        descriptor_sent = True
                        break
        time.sleep(1)
    lib.clean_up_devices()
    print("Device detection thread stopping.")

# Main loop
try:
    while True:
        wait_for_connection()
        stop_detection_event.clear()

        # Start the device detection in a separate thread
        device_thread = threading.Thread(target=detect_devices_thread)
        device_thread.start()

        while bluetooth_ready.is_set():
            try:
                time.sleep(1)
            except OSError:
                bluetooth_ready.clear()

        # Stop the device detection thread
        stop_detection_event.set()
        device_thread.join()

        # Cleanup client socket
        if client_sock:
            client_sock.close()
        client_sock = None
        client_info = None

        print("Disconnected (red)")

except KeyboardInterrupt:
    pass

# Cleanup
print("Disconnected.")
if client_sock:
    client_sock.close()
server_sock.close()
print("All done.")

lib.clean_up_devices()
