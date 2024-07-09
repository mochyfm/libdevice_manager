import ctypes
import threading

# Definir las estructuras de datos en Python equivalentes a las definidas en C
class DeviceEvent(ctypes.Structure):
    _fields_ = [
        ("device_id", ctypes.c_int),
        ("vendor_id", ctypes.c_int),
        ("product_id", ctypes.c_int),
        ("serial_number", ctypes.c_char * 64),
        ("event_type", ctypes.c_char * 32),
        ("type", ctypes.c_char * 32),
        ("value", ctypes.c_char * 256)
    ]

# Cargar la biblioteca compartida
device_manager = ctypes.CDLL('./libdevice_manager.so')

# Definir el tipo de la función de callback
CALLBACK_TYPE = ctypes.CFUNCTYPE(None, DeviceEvent)

# Definir la función de callback
def device_event_callback(event):
    print("Device Event:")
    print(f"  Device ID: {event.device_id}")
    print(f"  Vendor ID: {event.vendor_id}")
    print(f"  Product ID: {event.product_id}")
    print(f"  Serial Number: {event.serial_number.decode('utf-8')}")
    print(f"  Event Type: {event.event_type.decode('utf-8')}")
    print(f"  Type: {event.type.decode('utf-8')}")
    print(f"  Value: {event.value.decode('utf-8')}")

# Convertir la función de Python a un tipo de callback de C
callback = CALLBACK_TYPE(device_event_callback)

# Configurar la función detect_devices para que use la callback definida
device_manager.detect_devices.argtypes = [CALLBACK_TYPE]
device_manager.detect_devices.restype = None

# Iniciar la detección de dispositivos en un hilo separado
def start_device_detection():
    device_manager.detect_devices(callback)

device_thread = threading.Thread(target=start_device_detection)
device_thread.start()

# Mantener el hilo principal vivo
try:
    while True:
        pass
except KeyboardInterrupt:
    print("Deteniendo la detección de dispositivos.")
    device_manager.clean_up_devices()
