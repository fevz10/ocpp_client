#include "Client.h"
#include "CAN.h"
#include "Config.h"

int main(void)
{
    Config_Read();
    CAN_Initialize();
    Client_Initialize();
    while (1) 
    {
        Client_IsAwake();
    }
    Client_Destroy();
    return 0;
}