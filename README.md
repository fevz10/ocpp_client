# ocpp_client
ocpp_client is a software that works compatible with OCPP 1.6j servers, developed using **libwebsocket** and **cJSON** library dependencies.

## Usage
#### Prerequisites
You will need:

 * libwebsockets
 * CMake 3.5+

 #### Building The Project

```bash
$ git clone \
    https://github.com/fevz10/ocpp_client.git
$ cd ocpp_client
$ mkdir build && cd build
$ cmake ..
$ make install
```
 #### Run The Project

```bash
$ ocpp_client
```

