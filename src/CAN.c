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
		printf(ERR"[CAN] Failed to open CAN port initialize\n"RST);
		return;
	}
    strcpy(ifr.ifr_name, "can0" );
	ioctl(can_socket, SIOCGIFINDEX, &ifr);
    memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(can_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
		printf(ERR"[CAN] Failed to open CAN port binding.\n"RST);
	}
    pthread_create(&can_receive_pth, NULL, CAN_RX_THREAD, NULL);
    printf(INFO"[CAN] Starting the receive thread.\n"RST);
    printf(INFO"[CAN] Initialized succesful.\n"RST);
}

void* CAN_RX_THREAD(void * param)
{
    struct can_frame RXmsg;
    while (1)
    {
        read(can_socket, &RXmsg, sizeof(RXmsg));
        switch ( RXmsg.can_id )
        {
        case 0x123:
            EVPresentVoltage = (uint16_t)( ( (uint8_t)RXmsg.data[1] << 8) | (uint8_t)RXmsg.data[0] );
            EVSOC = (uint16_t)( ( (uint8_t)RXmsg.data[3] << 8) | (uint8_t)RXmsg.data[2] );
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
        /*
        switch ( RXmsg.can_id )
        {
            case 0x600:
                EVSE_ChargeState = RXmsg.data[0];
                EVSE_Session = RXmsg.data[1];
                EVSE_CPstate = ((uint8_t)(RXmsg.data[3] & 0x38)>>3);
                break;
            case 0x601:
                EVPresentVoltage = ( (uint16_t)( ( (uint8_t)RXmsg.data[1] << 8) | (uint8_t)RXmsg.data[0] ) * 0.1 );
                EVPresentCurrent = ( (uint16_t)( ( (uint8_t)RXmsg.data[3] << 8) | (uint8_t)RXmsg.data[2] ) * 0.1 ) - 1000;
                EVPower = EVPresentVoltage * EVPresentCurrent;
                break;
            case 0x602:
                EVDeliveredEnergy = ( (uint32_t)( ((uint8_t)RXmsg.data[7] << 24) | ((uint8_t)RXmsg.data[6] << 16) | ((uint8_t)RXmsg.data[5] << 8) | ((uint8_t)RXmsg.data[4]) ) * 0.001);
            case 0x95110104: //15110104
                //EVCCID = (uint16_t)( ( (uint8_t)RXmsg.data[1] << 8) | (uint8_t)RXmsg.data[0] );
                //sprintf(evccidStr, "%d", EVCCID);
                //if(EVCCID >= 2501 && EVCCID <= 2506){
                    sprintf(evccidStr, "%d", EVCCID);
                //}
                //printf("\nEVCCID = %d", EVCCID);

                break;
            case 0x95110107:
                EVSOC = RXmsg.data[2];
                break;
            default:
                break;
        }
        */

    }
}