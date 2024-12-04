#include "Client.h"
#include "CAN.h"
#include "Config.h"

void main(void)
{
    Config_Initialize();
    CAN_Initialize();
    Client_Initialize();

    while (1) 
    {
        Client_IsAwake();
    }

    Client_Destroy();

    //return 0;
}