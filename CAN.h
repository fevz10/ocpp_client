#ifndef CAN_H
#define CAN_H

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "Utils.h"

int can_socket;
struct sockaddr_can addr;
struct ifreq ifr;
struct can_frame canFrame;
pthread_t can_receive_pth;

/* Function Prototypes */
void* CAN_RX_THREAD(void * param);
void  CAN_Initialize(void);

#endif // CAN_H