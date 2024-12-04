#include "CAN.h"

extern uint8_t EVSE_CPstate;
extern uint8_t EVSE_ChargeState;
extern double EVPresentVoltage;
extern double EVPresentCurrent;
extern double EVSOC;
extern double EVDeliveredEnergy;
extern double EVPower;
extern uint8_t EVSE_Session;
extern double mapValue(double value, double inMin, double inMax, double outMin, double outMax);


void CAN_Initialize(void)
{
    if ((can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
    {
		(int)printf(ERR"[CAN] Failed to open CAN port initialize\n"RST);
		return;
	}
    (int)strcpy((char *)ifr.ifr_name, (const char *)"can0");
	ioctl(can_socket, SIOCGIFINDEX, &ifr);
    (int)memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(can_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
		(int)printf(ERR"[CAN] Failed to open CAN port binding.\n"RST);
	}
    (int)pthread_create(&can_receive_pth, NULL, CAN_RX_THREAD, NULL);
    (int)printf(INFO"[CAN] Starting the receive thread.\n"RST);
    (int)printf(INFO"[CAN] Initialized succesful.\n"RST);
}

void* CAN_RX_THREAD(void * param)
{
    (void *) param; //  UNUSED
    struct can_frame RXmsg;
    while (1)
    {
        read(can_socket, &RXmsg, sizeof(RXmsg));
        switch ( RXmsg.can_id )
        {
        case 0x123:
            EVPresentVoltage = (double)((uint16_t)( ( (uint8_t)RXmsg.data[1] << 8) | (uint8_t)RXmsg.data[0] ));
            EVSOC = (double)( (uint16_t)( ( (uint8_t)RXmsg.data[3] << 8) | (uint8_t)RXmsg.data[2] ));
            EVSOC = mapValue(EVSOC, 0, 4095, 0, 100);
            EVPresentVoltage = mapValue(EVPresentVoltage, 0, 4095, 0, 400);
            EVPresentCurrent = 100.6;
            EVPower = EVPresentVoltage * EVPresentCurrent;
            EVSE_ChargeState = RXmsg.data[4] & 0x01u;
            EVSE_CPstate = RXmsg.data[5] & 0x01u;
            EVDeliveredEnergy = 1.5;
            EVSE_Session = 0;
            break;
        
        default:
            break;
        }
    }
}