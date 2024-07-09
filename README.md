This repository is created for the development of a C library capable of reading direct inputs from various HID devices through the machine.

There are two versions of this library:

1. In the Input Data Direct Reading folder, you can find the version of the library that reads the input data directly from the operating system, instead of reading the input data directly itself. This is a simpler way to create the library, but it may not be sufficient depending on the use case.
2. In the HID Direct Reading folder, you can find the most appropriate version of the library. This version is capable of reading the data directly received by the operating system, but instead of letting the OS handle the data, we receive the raw version of this data to manipulate it for our own purposes once we call the library. (This version requires sudo or administrator privileges in order to execute the code).

To understand the library, you need to know that we are using Python for testing and the approach we have followed to fully implement this code. It is prepared for any language, as we use a callback function to process the data and return it as desired by the user.

To compile the library in C, you need to execute the following command:

```sh
gcc -shared -o libdevice_manager.so -fPIC device_manager.c -lusb-1.0 -lSDL2 -lpthread
```