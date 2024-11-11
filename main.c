#include <stdio.h>
#include <string.h>
#include <libwebsockets.h>
#include <time.h>
#include "cJSON.h"
#include <pthread.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>


#define KGRN  "\x1B[32m"
#define KRED  "\x1B[31m"
#define FULLCHAIN_PEM_FILE "ca_certificate.crt"
#define CACERT_FILE "cacert.pem"
// console inputs definitions
bool input_heartbeat = false;
bool finishedTransactionChecker = false;
int errCodeReason;
int can_socket;
struct sockaddr_can addr;
struct ifreq ifr;
struct can_frame canFrame;
pthread_t can_receive_pth;

pthread_t heartbeat_pth;
pthread_t meterValues_pth;

uint8_t EVSE_CPstate;
uint8_t EVSE_ChargeState;
double EVPresentVoltage;
double EVPresentCurrent;
uint8_t EVSOC;
double EVDeliveredEnergy;
double EVPower;
uint16_t EVCCID = 38;
uint8_t EVSE_Session = 0;

int transaction_id;
int evccidtxt;
char evccidStr[20];
char timestampBuffer[30];
cJSON *bootNotificationJSON = NULL;
cJSON *statusNotificationJSON = NULL;
cJSON *startTransactionJSON = NULL;
cJSON *stopTransactionJSON = NULL;
cJSON *remoteStartTransactionJSON = NULL;
cJSON *remoteStopTransactionJSON = NULL;
cJSON *heartbeatJSON = NULL;
cJSON *idTagInfo = NULL;
cJSON *meterValuesRootJson = NULL;
cJSON *meterValuesJSON = NULL;
cJSON *meterValueArray = NULL;
cJSON *sampledValueArray = NULL;
cJSON *sampledValueVoltageObj = NULL;
cJSON *sampledValueCurrentObj = NULL;
cJSON *sampledValuePowerObj = NULL;
cJSON *sampledValueEnergyObj = NULL;
cJSON *sampledValueSoCObj = NULL;
pthread_t ocpp_process_pth;
cJSON *receiveBufferJSON;
char txBuffer[1000];
char rcv_str[200];
char rxRequestOp[50];
struct lws *wsi;
int process_id;
char UUID[37];
int ocppStateMachineState = 0;
int rcv_flag = 0;
char strVoltage[20];
char strCurrent[20];
char strEnergy[20];
char strPower[20];
char strSOC[20];
uint8_t meterValueOneMinCounter = 0;

cJSON *ocpp_server_host = NULL;
cJSON *ocpp_server_port = NULL;
cJSON *ocpp_server_path = NULL;
cJSON *cp_id = NULL;
cJSON *chargePointVendor = NULL;
cJSON *chargePointModel = NULL;
char OCPP_SERVER_FULL_PATH[1024];
char received_frame_buffer[2048];

int uart_fd, uart_len;
char uart_data[32];
struct termios uart_options;


void UART_Init(void)
{
    uart_fd = open("/dev/ttyS4", O_RDWR | O_NDELAY | O_NOCTTY);
    if ( uart_fd < 0 )
    {
        perror("Error opening serial port");
        return -1;
    }
    uart_options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    uart_options.c_iflag = IGNPAR;
    uart_options.c_oflag = 0;
    uart_options.c_lflag = 0;

    tcflush(uart_fd, TCIFLUSH);
    tcsetattr(uart_fd, TCSANOW, &uart_options);
}

//uint8_t isNormalProcess = 0;

/*
int uart_fd;
struct termios uart_config;
char start_data[7] = "Start\n";
char stop_data[6] = "Stop\n";
int bytes_written;
char *portname = "/dev/ttyS2";

void UART_Init(void)
{
    uart_fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (uart_fd < 0)
    {
        printf("error %d opening %s: %s", errno, portname, strerror(errno));
        return;
    }

    set_interface_attribs (uart_fd, B9600, 0);  // set speed to 9,600 bps, 8n1 (no parity)
    set_blocking (uart_fd, 0);                // set no blocking
}

int set_interface_attribs (int fd, int speed, int parity)
{
    struct termios tty;
    if (tcgetattr (fd, &tty) != 0)
    {
        printf("error %d from tcgetattr", errno);
        return -1;
    }

    cfsetospeed (&tty, speed);
    cfsetispeed (&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
                                    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
            printf("error %d from tcsetattr", errno);
            return -1;
    }
    return 0;
}

void set_blocking (int fd, int should_block)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)
    {
            printf("error %d from tggetattr", errno);
            return;
    }

    tty.c_cc[VMIN]  = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
        printf("error %d setting term attributes", errno);
    }
}
*/


int isRemoteMsg(char *receivedStr)
{
    if((strstr(receivedStr, (const char *)"RemoteStartTransaction") != NULL))
    {
        return 1;
    }
    else if((strstr(receivedStr, (const char *)"RemoteStopTransaction") != NULL))
    {
        return 2;
    }
    else
    {
        return 0;
    }
}


void* meterValues_thread(void * param)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    while (1)
    {   
        if (EVSE_ChargeState != 14)
        {
            break;
        }

        meterValuesRootJson = cJSON_CreateObject();
        cJSON_AddNumberToObject(meterValuesRootJson, "connectorId", 1);
        cJSON_AddNumberToObject(meterValuesRootJson, "transactionId", transaction_id);
        meterValueArray = cJSON_AddArrayToObject(meterValuesRootJson, "meterValue");
        meterValuesJSON = cJSON_CreateObject();
        getTimestamp();
        cJSON_AddStringToObject(meterValuesJSON, "timestamp", timestampBuffer);
        sampledValueArray = cJSON_AddArrayToObject(meterValuesJSON, "sampledValue");
        

        sprintf(strVoltage, "%d", 230);
        sampledValueVoltageObj = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueVoltageObj, "value", strVoltage);
        cJSON_AddStringToObject(sampledValueVoltageObj, "context", "Sample.Periodic");
        cJSON_AddStringToObject(sampledValueVoltageObj, "format", "Raw");
        cJSON_AddStringToObject(sampledValueVoltageObj, "measurand", "Voltage");
        cJSON_AddStringToObject(sampledValueVoltageObj, "location", "Inlet");
        cJSON_AddStringToObject(sampledValueVoltageObj, "unit", "V");
        cJSON_AddItemToArray(sampledValueArray, sampledValueVoltageObj);
        
        sprintf(strCurrent, "%lf", EVPresentCurrent);
        sampledValueCurrentObj = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueCurrentObj, "value", strCurrent);
        cJSON_AddStringToObject(sampledValueCurrentObj, "context", "Sample.Periodic");
        cJSON_AddStringToObject(sampledValueCurrentObj, "format", "Raw");
        cJSON_AddStringToObject(sampledValueCurrentObj, "measurand", "Current.Import");
        cJSON_AddStringToObject(sampledValueCurrentObj, "location", "Outlet");
        cJSON_AddStringToObject(sampledValueCurrentObj, "unit", "A");
        cJSON_AddItemToArray(sampledValueArray, sampledValueCurrentObj);

        sprintf(strPower, "%lf ", EVPower);
        sampledValuePowerObj = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValuePowerObj, "value", strPower);
        cJSON_AddStringToObject(sampledValuePowerObj, "context", "Sample.Periodic");
        cJSON_AddStringToObject(sampledValuePowerObj, "format", "Raw");
        cJSON_AddStringToObject(sampledValuePowerObj, "measurand", "Power.Active.Import");
        cJSON_AddStringToObject(sampledValuePowerObj, "location", "Outlet");
        cJSON_AddStringToObject(sampledValuePowerObj, "unit", "W");
        cJSON_AddItemToArray(sampledValueArray, sampledValuePowerObj);

        sprintf(strEnergy, "%lf", EVDeliveredEnergy);
        sampledValueEnergyObj = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueEnergyObj, "value", strEnergy);
        cJSON_AddStringToObject(sampledValueEnergyObj, "context", "Sample.Periodic");
        cJSON_AddStringToObject(sampledValueEnergyObj, "format", "Raw");
        cJSON_AddStringToObject(sampledValueEnergyObj, "measurand", "Energy.Active.Import.Register");
        cJSON_AddStringToObject(sampledValueEnergyObj, "location", "Outlet");
        cJSON_AddStringToObject(sampledValueEnergyObj, "unit", "kWh");
        cJSON_AddItemToArray(sampledValueArray, sampledValueEnergyObj);

        sprintf(strSOC, "%d", EVSOC);
        sampledValueSoCObj = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueSoCObj, "value", strSOC);
        cJSON_AddStringToObject(sampledValueSoCObj, "unit", "Percent");
        cJSON_AddStringToObject(sampledValueSoCObj, "measurand", "SoC");
        cJSON_AddItemToArray(sampledValueArray, sampledValueSoCObj);
        
        cJSON_AddItemToArray(meterValueArray, meterValuesJSON);
        char *jsonString = cJSON_Print(meterValuesRootJson);

        sendOCPPFrame(2, "MeterValues", meterValuesRootJson);
        printf("METERVALUES SENT OK\n");
        memset(txBuffer, 0x00, 1000 * sizeof(char));

        memset(strVoltage, 0x00, sizeof(strVoltage));
        memset(strCurrent, 0x00, sizeof(strCurrent));
        memset(strPower, 0x00, sizeof(strPower));
        memset(strEnergy, 0x00, sizeof(strEnergy));
        memset(strSOC, 0x00, sizeof(strSOC));

        if(meterValueOneMinCounter == 6)
        {
            sleep(60);
        }
        else
        {
            meterValueOneMinCounter++;
            sleep(10);
        }
    }
    return NULL;
}

void* heartbeat_thread(void * param)
{  
    while(1)
    {
        input_heartbeat = true;
        //EVSE_ChargeState = 14;
        //ocppStateMachineState = 0;
        sleep(1);
        heartbeatJSON = cJSON_CreateObject();
        cJSON_AddStringToObject(heartbeatJSON, "", "");
        sendOCPPFrame(2,"Heartbeat", heartbeatJSON);
        printf("HEARTBEAT SENT OK\n");
        cJSON_Delete(heartbeatJSON);
        memset(txBuffer, 0x00, 1000 * sizeof(char));
        input_heartbeat = false;
        sleep(10);
    }

    
    return NULL;
}

void* can_rx_thread(void * param)
{
    struct can_frame RXmsg;
    while (1)
    {   
        read(can_socket, &RXmsg, sizeof(RXmsg));
        
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
    }
}

static int callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

static struct lws_protocols protocols[] = 
{
    {
        "ocpp1.6",
        callback,
        0,
        0,
    },
    { NULL, NULL, 0, 0 } /* terminator */
};

int sendOCPPFrame(int operation,const char *action, cJSON* jsonData)
{
    snprintf(txBuffer, sizeof(txBuffer), "[%d,\"d9d6618e-d015-4e3d-ae4c-a6f0840b71fa\",\"%s\",%s]", operation, action, cJSON_Print(jsonData));
    return lws_write(wsi, (unsigned char *)txBuffer, strlen(txBuffer), LWS_WRITE_TEXT);
    //memset(txBuffer, 0x00, 1000 * sizeof(char));
}

int sendOCPPRemoteFrame(int operation, char* uuid, cJSON* jsonData)
{
    snprintf(txBuffer, sizeof(txBuffer), "[%d,\"%s\",%s]", operation, uuid, cJSON_Print(jsonData));
    return lws_write(wsi, (unsigned char *)txBuffer, strlen(txBuffer), LWS_WRITE_TEXT);
    //memset(txBuffer, 0x00, 1000 * sizeof(char));
}

void getTimestamp(void)
{
    time_t currentTime;
    struct tm *timeInfo;

    time(&currentTime);
    timeInfo = gmtime(&currentTime);
    //timeInfo->tm_hour -= 3;
    // Format the timestamp
    strftime(timestampBuffer, sizeof(timestampBuffer), "%Y-%m-%dT%H:%M:%S.000Z", timeInfo);

    // Print the formatted timestamp
    //printf("Formatted Timestamp: %s\n", timestampBuffer);
}

char* extractJsonData(const char* input)
{
    const char* start = strchr(input, '{');
    const char* end = strrchr(input, '}');
    
    if (start && end) {
        size_t length = end - start + 1;
        char* json_data = (char*)malloc(length + 1);
        strncpy(json_data, start, length);
        json_data[length] = '\0';
        return json_data;
    }
    return NULL;
}

const char* getErrorCodeForSession(int code) {
    switch(code) {
        case 0:
            return "NoError";
        case 1:
            return "OtherError";
        case 2:
            return "OtherError";
        case 3:
            return "NoError";
        case 4:
            return "NoError";
        case 5:
            return "NoError";
        case 6:
            return "EVCommunicationError";
        case 7:
            return "EVCommunicationError";
        case 8:
            return "GroundFailure";
        case 9:
            return "OverCurrentFailure";
        case 10:
            return "OverVoltage";
        case 11:
            return "InternalError";
        case 12:
            return "EVCommunicationError";
        default:
            return "NoError";
    }
}

const char* getDetailedErrorCodeForSession(int code) {
    switch(code) {
        case 0:
            return "No Result";
        case 1:
            return "EV Terminated Session";
        case 2:
            return "EV is not Ready";
        case 3:
            return "EV stops the charge with CP";
        case 4:
            return "Evse stopped by button";
        case 5:
            return "Evse stopped by emergency";
        case 6:
            return "CP State Fault";
        case 7:
            return "Event Timeout";
        case 8:
            return "Isolation Fault";
        case 9:
            return "Over Current Fault";
        case 10:
            return "Over Voltage Fault";
        case 11:
            return "Hardware Fault(Check power modules)";
        case 12:
            return "PLC Communication Fault";
        default:
            return "No Result";
    }
}


void* ocpp_stateMachine(void * param)
{
    while(1)
    {
        switch(ocppStateMachineState)
        {
            case 0: // Startup the station
                if(EVSE_ChargeState == 14)
                {
                    statusNotificationJSON = cJSON_CreateObject();
                    cJSON_AddNumberToObject(statusNotificationJSON, "connectorId", 1);
                    printf("charging 0");
                    cJSON_AddStringToObject(statusNotificationJSON, "status", "Charging");
                    cJSON_AddStringToObject(statusNotificationJSON, "errorCode", getErrorCodeForSession(EVSE_Session));
                    cJSON_AddStringToObject(statusNotificationJSON, "vendorErrorCode", getDetailedErrorCodeForSession(EVSE_Session));
                    sendOCPPFrame(2, "StatusNotification", statusNotificationJSON);
                    printf("STATUSNOTIFICATION SENT OK\n");
                    memset(txBuffer, 0x00, 1000 * sizeof(char));
                    sleep(1);
                    FILE *fp = fopen("/root/transaction_id.txt", "r");
                    if (fp != NULL)
                    {
                        fscanf(fp, "%d", &transaction_id);
                        fclose(fp);
                    }

                    FILE *fp1 = fopen("/root/evccid.txt", "r");
                    if (fp1 != NULL)
                    {
                        fscanf(fp1, "%d", &evccidtxt);
                        fclose(fp1);
                    }
                    
                    getTimestamp();
                    //sprintf(evccidStr, "%d", EVCCID);
                    printf("%d", transaction_id);
                    //while(EVCCID==0);

                    if (transaction_id != 0) 
                    {
                        ocppStateMachineState = 3;
                        
                        pthread_create(&meterValues_pth, NULL, meterValues_thread, NULL);
                        break;
                    }
                    else
                    {
                        ocppStateMachineState = 2;
                        break;
                    }
                    /*
                    stopTransactionJSON = cJSON_CreateObject();
                    cJSON_AddNumberToObject(stopTransactionJSON, "connectorId", 1);
                    cJSON_AddStringToObject(stopTransactionJSON, "idTag", evccidStr);
                    cJSON_AddNumberToObject(stopTransactionJSON, "meterStop", (int)(EVDeliveredEnergy*1000));
                    getTimestamp();
                    cJSON_AddStringToObject(stopTransactionJSON, "timestamp", timestampBuffer);
                    cJSON_AddNumberToObject(stopTransactionJSON, "transactionId", transaction_id);
                    sendOCPPFrame(2, "StopTransaction", stopTransactionJSON);
                    printf("STOP TRANSACTION SENT OK\n");
                    memset(txBuffer, 0x00, 1000 * sizeof(char));
                    sleep(5);
                    */
                } 
                
                switch(EVSE_CPstate)
                {
                    case 1:
                        //cJSON_AddStringToObject(statusNotificationJSON, "status", "Available");
                        bootNotificationJSON = cJSON_CreateObject();
                        cJSON_AddStringToObject(bootNotificationJSON, "chargePointVendor", chargePointVendor->valuestring);
                        cJSON_AddStringToObject(bootNotificationJSON, "chargePointModel", chargePointModel->valuestring);

                        sendOCPPFrame(2, "BootNotification", bootNotificationJSON);
                        printf("BOOTNOTIFICATION SENT OK\n");
                        memset(txBuffer, 0x00, 1000 * sizeof(char));

                        //rcv_flag = 0;
                        //while(rcv_flag==0);
                        sleep(1);
                        //while(strcmp(cJSON_GetObjectItem(receiveBufferJSON, "status")->valuestring,"Accepted"));
                        printf("GET ACCEPTED\r\n");

                        statusNotificationJSON = cJSON_CreateObject();
                        cJSON_AddNumberToObject(statusNotificationJSON, "connectorId", 1);
                        cJSON_AddStringToObject(statusNotificationJSON, "status", "Available");
                        cJSON_AddStringToObject(statusNotificationJSON, "errorCode", getErrorCodeForSession(EVSE_Session));
                        break;
                }
                cJSON_AddStringToObject(statusNotificationJSON, "errorCode", getErrorCodeForSession(EVSE_Session));
                cJSON_AddStringToObject(statusNotificationJSON, "vendorErrorCode", getDetailedErrorCodeForSession(EVSE_Session));
                sendOCPPFrame(2, "StatusNotification", statusNotificationJSON);
                printf("STATUSNOTIFICATION SENT OK\n");
                memset(txBuffer, 0x00, 1000 * sizeof(char));
                pthread_create(&heartbeat_pth, NULL, heartbeat_thread, NULL);
                sleep(1);
                cJSON_Delete(bootNotificationJSON);
                cJSON_Delete(statusNotificationJSON);
                ocppStateMachineState = 1;
                /*
                ocppstates = BootNotification;
                while(ocppstates==BootNotification);
                status = 0;
                ocppstates = StatusNotification;
                isBootNotificationSend = false;
                while((ocppstates==StatusNotification) && (isStartupProcessOK==false));
                isStatusNotificationSend = false;
                ocppStateMachineState = 1;
                isHeartBeatOK = true;
                pthread_create(&heartbeat_pth, NULL, heartbeat_thread, NULL);
                printf("Startup state --> OK\n\n");
                */
                break;
            case 1: // Wait Plug State
                if(!finishedTransactionChecker) 
                {
                    FILE *fp2 = fopen("/root/transaction_id.txt", "r");
                    if (fp2 != NULL)
                    {
                        fscanf(fp2, "%d", &transaction_id);
                        fclose(fp2);
                    }
                    
                }
                
                if (transaction_id != 0 && EVSE_ChargeState != 14 && !finishedTransactionChecker)
                { 
                    //sprintf(evccidStr, "%d", EVCCID);
                    stopTransactionJSON = cJSON_CreateObject();
                    cJSON_AddNumberToObject(stopTransactionJSON, "connectorId", 1);
                    //cJSON_AddStringToObject(stopTransactionJSON, "idTag", evccidStr);
                    cJSON_AddNumberToObject(stopTransactionJSON, "meterStop", (int)(EVDeliveredEnergy*1000));
                    getTimestamp();
                    cJSON_AddStringToObject(stopTransactionJSON, "timestamp", timestampBuffer);
                    //txt yaz
                    
                    cJSON_AddNumberToObject(stopTransactionJSON, "transactionId", transaction_id);
                    sendOCPPFrame(2, "StopTransaction", stopTransactionJSON);
                    printf("STOP TRANSACTION SENT OK\n");
                    memset(txBuffer, 0x00, 1000 * sizeof(char));
                    sleep(1);
                    cJSON_Delete(stopTransactionJSON);

                    FILE *fp1 = fopen("/root/transaction_id.txt", "w");
                    if (fp1 != NULL)
                    {
                        fprintf(fp1, "%d", 0);
                        fclose(fp1);
                    }
                    finishedTransactionChecker = true;
                }

                if( (EVSE_CPstate==2) || (EVSE_CPstate==3) )    //if plug is connected
                {
                    statusNotificationJSON = cJSON_CreateObject();
                    cJSON_AddNumberToObject(statusNotificationJSON, "connectorId", 1);
                    //cJSON_AddStringToObject(statusNotificationJSON, "status", "Available");
                    //cJSON_AddStringToObject(statusNotificationJSON, "errorCode", "NoError");
                    switch(EVSE_CPstate)
                    {
                        case 1:
                            cJSON_AddStringToObject(statusNotificationJSON, "status", "Available");
                            break;
                        case 2:
                            if (EVSE_ChargeState >= 6 && EVSE_ChargeState < 14)
                            {
                                cJSON_AddStringToObject(statusNotificationJSON, "status", "Preparing"); // YADA FİNİSHİNG
                            }
                            else
                            {
                                cJSON_AddStringToObject(statusNotificationJSON, "status", "Finishing"); // YADA FİNİSHİNG
                            }
                                
                            break;
                        case 3:
                            if (EVSE_ChargeState == 14)
                            {
                                cJSON_AddStringToObject(statusNotificationJSON, "status", "Charging"); // YADA FİNİSHİNG
                            }
                            else if (EVSE_ChargeState >= 6 && EVSE_ChargeState < 14)
                            {
                                cJSON_AddStringToObject(statusNotificationJSON, "status", "Preparing"); // YADA FİNİSHİNG
                            }
                            else
                            {
                                cJSON_AddStringToObject(statusNotificationJSON, "status", "Finishing"); // YADA FİNİSHİNG
                            }
                            break;
                        default:
                            cJSON_AddStringToObject(statusNotificationJSON, "status", "Faulted"); // YADA FİNİSHİNG
                            break;
                    }

                    
                    cJSON_AddStringToObject(statusNotificationJSON, "errorCode", getErrorCodeForSession(EVSE_Session));
                    cJSON_AddStringToObject(statusNotificationJSON, "vendorErrorCode", getDetailedErrorCodeForSession(EVSE_Session));
                    sendOCPPFrame(2, "StatusNotification", statusNotificationJSON);
                    printf("STATUSNOTIFICATION SENT OK\n");
                    memset(txBuffer, 0x00, 1000 * sizeof(char));
                    sleep(1);
                    cJSON_Delete(statusNotificationJSON);
                    //isNormalProcess = 1;
                    ocppStateMachineState = 2;
                }
                break;
            case 2: // Charging State
                if(EVSE_ChargeState == 14)  //Charging Startup
                {
                    statusNotificationJSON = cJSON_CreateObject();
                    cJSON_AddNumberToObject(statusNotificationJSON, "connectorId", 1);
                    //cJSON_AddStringToObject(statusNotificationJSON, "status", "Available");
                    //cJSON_AddStringToObject(statusNotificationJSON, "errorCode", "NoError");
                    
                    switch(EVSE_CPstate)
                    {
                        case 1:
                            cJSON_AddStringToObject(statusNotificationJSON, "status", "Available");
                            break;
                        case 2:
                            cJSON_AddStringToObject(statusNotificationJSON, "status", "Available");
                            if(EVSE_ChargeState == 14)
                            {
                                printf("CHARGING");
                                cJSON_AddStringToObject(statusNotificationJSON, "status", "Charging");
                            }
                            else
                            {
                                cJSON_AddStringToObject(statusNotificationJSON, "status", "Preparing");
                            }
                            break;
                        case 3:
                            cJSON_AddStringToObject(statusNotificationJSON, "status", "Available");
                            if(EVSE_ChargeState == 14)
                            {
                                cJSON_AddStringToObject(statusNotificationJSON, "status", "Charging");
                            }
                            else
                            {
                                cJSON_AddStringToObject(statusNotificationJSON, "status", "Preparing");
                            }
                            break;
                        default:
                            break;
                    }
                    
                    cJSON_AddStringToObject(statusNotificationJSON, "errorCode", getErrorCodeForSession(EVSE_Session));
                    cJSON_AddStringToObject(statusNotificationJSON, "vendorErrorCode", getDetailedErrorCodeForSession(EVSE_Session));
                    sendOCPPFrame(2, "StatusNotification", statusNotificationJSON);
                    printf("STATUSNOTIFICATION SENT OK\n");
                    memset(txBuffer, 0x00, 1000 * sizeof(char));
                    
                    sleep(1);
                    getTimestamp();
                    
                    sprintf(evccidStr, "%u", EVCCID);

                    FILE *fp = fopen("/root/evccid.txt", "w");
                    if (fp != NULL)
                    {
                        fprintf(fp, "%s", evccidStr);
                        fclose(fp);
                    }
                    sleep(1);
                    startTransactionJSON = cJSON_CreateObject();
                    cJSON_AddNumberToObject(startTransactionJSON, "connectorId", 1);
                    cJSON_AddStringToObject(startTransactionJSON, "idTag", evccidStr);
                    cJSON_AddNumberToObject(startTransactionJSON, "meterStart", 0);
                    cJSON_AddStringToObject(startTransactionJSON, "timestamp", timestampBuffer);
                    printf("EVCCID : %s\n", evccidStr);
                    sendOCPPFrame(2, "StartTransaction", startTransactionJSON);
                    memset(txBuffer, 0x00, 1000 * sizeof(char));
                    sleep(1);
                    //idTagInfo = cJSON_GetObjectItem(receiveBufferJSON, "idTagInfo");
                    transaction_id = cJSON_GetObjectItem(receiveBufferJSON, "transactionId")->valueint;
                    FILE *fp1 = fopen("/root/transaction_id.txt", "w");
                    if (fp1 != NULL)
                    {
                        fprintf(fp1, "%d", transaction_id);
                        fclose(fp1);
                    }
                    //cJSON_Delete(statusNotificationJSON);
                    cJSON_Delete(startTransactionJSON);

                    sleep(1);
                    ocppStateMachineState = 3;
                    
                    pthread_create(&meterValues_pth, NULL, meterValues_thread, NULL);
                }
                break;
            case 3: // Charging Finished State
                if(EVSE_ChargeState != 14)
                {
                    statusNotificationJSON = cJSON_CreateObject();
                    cJSON_AddNumberToObject(statusNotificationJSON, "connectorId", 1);
                    printf("finishing3");
                    cJSON_AddStringToObject(statusNotificationJSON, "status", "Finishing");
                    cJSON_AddStringToObject(statusNotificationJSON, "errorCode", getErrorCodeForSession(EVSE_Session));
                    cJSON_AddStringToObject(statusNotificationJSON, "vendorErrorCode", getDetailedErrorCodeForSession(EVSE_Session));                  
                    sendOCPPFrame(2, "StatusNotification", statusNotificationJSON);
                    printf("STATUSNOTIFICATION SENT OK\n");
                    memset(txBuffer, 0x00, 1000 * sizeof(char));
                    sleep(2);
                    idTagInfo->valuestring = cJSON_GetObjectItem(receiveBufferJSON, "idTagInfo")->child->valuestring;
                    getTimestamp();
                    //sprintf(evccidStr, "%d", EVCCID);
                    stopTransactionJSON = cJSON_CreateObject();
                    cJSON_AddNumberToObject(stopTransactionJSON, "connectorId", 1);
                    //cJSON_AddStringToObject(stopTransactionJSON, "idTag", evccidStr);
                    cJSON_AddNumberToObject(stopTransactionJSON, "meterStop", (int)(EVDeliveredEnergy*1000));
                    getTimestamp();
                    cJSON_AddStringToObject(stopTransactionJSON, "timestamp", timestampBuffer);
                    //txt oku
                    FILE *fp = fopen("/root/transaction_id.txt", "r");
                    if (fp != NULL)
                    {
                        fscanf(fp, "%d", &transaction_id);
                        fclose(fp);
                        
                    }
                    cJSON_AddNumberToObject(stopTransactionJSON, "transactionId", transaction_id);
                    sendOCPPFrame(2, "StopTransaction", stopTransactionJSON);
                    printf("STOP TRANSACTION SENT OK\n");
                    memset(txBuffer, 0x00, 1000 * sizeof(char));
                    sleep(1);
                    cJSON_Delete(statusNotificationJSON);
                    cJSON_Delete(stopTransactionJSON);
                    ///////////////////
                    printf("meter thread üstü");
                    pthread_cancel(meterValues_pth);
                    // İş parçacığının tamamlanmasını bekleyin
                    pthread_join(meterValues_pth, NULL);
                    
                    printf("meter thread stop oldu");
                    FILE *fp1 = fopen("/root/transaction_id.txt", "w");
                    if (fp1 != NULL && (idTagInfo->valuestring == "Accepted"))
                    {
                        fprintf(fp1, "%d", 0);
                        fclose(fp1);
                        
                    }
                    idTagInfo = NULL;
                    ocppStateMachineState = 4;
                }                     
                break;
            case 4: //  Wait Unplug State
                if( ((EVSE_CPstate==2) || (EVSE_CPstate==3)))
                {
                    ocppStateMachineState = 4;
                }
                else
                {
                    statusNotificationJSON = cJSON_CreateObject();
                    cJSON_AddNumberToObject(statusNotificationJSON, "connectorId", 1);
                    cJSON_AddStringToObject(statusNotificationJSON, "status", "Available");
                    cJSON_AddStringToObject(statusNotificationJSON, "errorCode", getErrorCodeForSession(EVSE_Session));
                    cJSON_AddStringToObject(statusNotificationJSON, "vendorErrorCode", getDetailedErrorCodeForSession(EVSE_Session));
                    sendOCPPFrame(2, "StatusNotification", statusNotificationJSON);
                    printf("STATUSNOTIFICATION SENT OK\n");
                    memset(txBuffer, 0x00, 1000 * sizeof(char));
                    sleep(1);
                    cJSON_Delete(statusNotificationJSON);
                    ocppStateMachineState = 1;
                }
                break;
        }
    }
}

static struct lws_context *context;

static int callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    if (reason != 10)
        //printf("Reason --> %d\n", reason);
        
    errCodeReason = reason;
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("Socket Connected\n");
            pthread_create(&ocpp_process_pth, NULL, ocpp_stateMachine, NULL);
            //sleep(3);
            //pthread_create(&heartbeat_pth, NULL, heartbeat_thread, NULL);
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("Received: %.*s\n", (int)len, (char *)in);
            sprintf(received_frame_buffer, "%.*s", (int)len, (char *)in);
            sscanf(in, "[%d,", &process_id);
            sscanf(in, "[%*d,\"%36[^\"]\",", UUID);
            char* json_data_str = extractJsonData(in);
            receiveBufferJSON = cJSON_Parse(json_data_str);
            //printf("====================================================================\r\nProcessID : %d\r\nUUID : %s\r\nJSON : %s\r\n====================================================================\r\n", process_id, UUID, cJSON_Print(receiveBufferJSON));
            int isRemoteFrameFlag = isRemoteMsg(received_frame_buffer);
            if(isRemoteFrameFlag != 0)
            {
                switch (isRemoteFrameFlag)
                {
                case 1:
                    // Remote Start 
                    /*
                    remoteStartTransactionJSON = cJSON_CreateObject();
                    cJSON_AddStringToObject(remoteStartTransactionJSON, "status", "Accepted");
                    //sendOCPPFrame(2, "RemoteStartTransaction", remoteStartTransactionJSON);
                    sendOCPPRemoteFrame(3, UUID, remoteStartTransactionJSON);
                    printf("RemoteStartTransaction SENT OK accepted\n");
                    memset(txBuffer, 0x00, 1000 * sizeof(char));
                    sleep(1);
                    cJSON_Delete(remoteStartTransactionJSON);
                    ocppStateMachineState = 2;
                    */
                    
                    if( ( (EVSE_CPstate==2) || (EVSE_CPstate==3) ) && (EVSE_ChargeState != 14) )
                    {
                        system("echo \"StartTransaction\" > /dev/ttyS4");
                        sleep(1);

                        remoteStartTransactionJSON = cJSON_CreateObject();
                        cJSON_AddStringToObject(remoteStartTransactionJSON, "status", "Accepted");
                        //sendOCPPFrame(2, "RemoteStartTransaction", remoteStartTransactionJSON);
                        sendOCPPRemoteFrame(3, UUID, remoteStartTransactionJSON);
                        printf("RemoteStartTransaction SENT OK accepted\n");
                        memset(txBuffer, 0x00, 1000 * sizeof(char));
                        sleep(1);
                        cJSON_Delete(remoteStartTransactionJSON);
                        ocppStateMachineState = 2;
                    }
                    else
                    {
                        remoteStartTransactionJSON = cJSON_CreateObject();
                        cJSON_AddStringToObject(remoteStartTransactionJSON, "status", "Rejected");
                        //sendOCPPFrame(2, "RemoteStartTransaction", remoteStartTransactionJSON);
                        sendOCPPRemoteFrame(3, UUID, remoteStartTransactionJSON);
                        printf("RemoteStartTransaction SENT OK rejected\n");
                        memset(txBuffer, 0x00, 1000 * sizeof(char));
                        sleep(1);
                        cJSON_Delete(remoteStartTransactionJSON);
                    }
                    
                    break;
                case 2:
                    // Remote Stop
                    if( ( (EVSE_CPstate==2) || (EVSE_CPstate==3) ) && (EVSE_ChargeState == 14) )
                    {
                        system("echo \"StopTransaction\" > /dev/ttyS4");
                        sleep(1);

                        remoteStopTransactionJSON = cJSON_CreateObject();
                        cJSON_AddStringToObject(remoteStopTransactionJSON, "status", "Accepted");
                        //sendOCPPFrame(3, "RemoteStopTransaction", remoteStopTransactionJSON);
                        sendOCPPRemoteFrame(3, UUID, remoteStopTransactionJSON);
                        printf("RemoteStopTransaction SENT OK accepted\n");
                        memset(txBuffer, 0x00, 1000 * sizeof(char));
                        sleep(1);
                        cJSON_Delete(remoteStopTransactionJSON);
                        ocppStateMachineState = 3;
                    }
                    else
                    {
                        remoteStopTransactionJSON = cJSON_CreateObject();
                        cJSON_AddStringToObject(remoteStopTransactionJSON, "status", "Rejected");
                        //sendOCPPFrame(3, "RemoteStopTransaction", remoteStopTransactionJSON);
                        sendOCPPRemoteFrame(3, UUID, remoteStopTransactionJSON);
                        printf("RemoteStopTransaction SENT OK rejected\n");
                        memset(txBuffer, 0x00, 1000 * sizeof(char));
                        sleep(1);
                        cJSON_Delete(remoteStopTransactionJSON);
                    }
                    break;
                default:
                    break;
                }
            }
            rcv_flag = 1;
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            //printf("LWS_CALLBACK_CLIENT_WRITEABLE\n");
            break;
        case LWS_CALLBACK_CLOSED:
            printf("LWS_CALLBACK_CLOSED\n");
            wsi = NULL;
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf("LWS_CALLBACK_CLIENT_CONNECTION_ERROR\n");
            wsi = NULL;
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("LWS_CALLBACK_CLIENT_CLOSED\n");
            wsi = NULL;
            break;
        case LWS_CALLBACK_WSI_DESTROY:
            printf("LWS_CALLBACK_WSI_DESTROY\n");
            lws_context_destroy(context);
            wsi = NULL;
            break;
        default:
            break;
    }

    return 0;
}

static void reconnect()
{
    printf("Attempting to reconnect...\n");
    struct lws_client_connect_info ccinfo = {
        .context = context,
        .address = ocpp_server_host->valuestring,
        .port = ocpp_server_port->valueint,
        .path = OCPP_SERVER_FULL_PATH,
        .host = ocpp_server_host->valuestring,
        .origin = ocpp_server_host->valuestring,
        .protocol = "ocpp1.6",
        .ssl_connection = 0, // No SSL
    };

    wsi = lws_client_connect_via_info(&ccinfo);

    if (!wsi) {
    fprintf(stderr, "libwebsocket connection failed: %s\n", strerror(errno));
    lws_context_destroy(context);
    //sleep(5); // 5 saniye bekle
    //reconnect();
    }

}

void process_command(const char* command) {
    // Burada gelen komutu işleyin
    printf("Received command: %s\n", command);

    // Örneğin, "exit" komutu girildiğinde programı sonlandırabiliriz.
    if (strcmp(command, "exit") == 0) {
        printf("Exiting program...\n");
        exit(0);
    }
    else if (strcmp(command, "hb") == 0 && !input_heartbeat) {
        printf("Starting heartbeat thread...\n");
        pthread_t heartbeat_pth;
        pthread_create(&heartbeat_pth, NULL, heartbeat_thread, NULL);
        // input_heartbeat bayrağını true yapmayı buraya taşıdık
    }
    else if (strcmp(command, "test") == 0) {
        printf("Starting charging\n");
        EVSE_ChargeState = 14;
        EVSE_CPstate = 2;
        
        // input_heartbeat bayrağını true yapmayı buraya taşıdık
    }
    else if (strcmp(command, "7") == 0) {
        printf("Wait auth\n");
        EVSE_ChargeState = 7;
        EVSE_CPstate = 2;
        
        // input_heartbeat bayrağını true yapmayı buraya taşıdık
    }
    else if (strcmp(command, "14") == 0) {
        printf(" charg\n");
        EVSE_ChargeState = 14;
        EVSE_CPstate = 3;
        
        // input_heartbeat bayrağını true yapmayı buraya taşıdık
    }
    else if (strcmp(command, "4") == 0) {
        printf("unplug\n");
        EVSE_ChargeState = 4;
        EVSE_CPstate = 2;
        
        // input_heartbeat bayrağını true yapmayı buraya taşıdık
    }
    else if (strcmp(command, "5") == 0) {
        printf("cekildi\n");
        EVSE_ChargeState = 5;
        EVSE_CPstate = 1;
        
        // input_heartbeat bayrağını true yapmayı buraya taşıdık
    }
    // Diğer komutları burada işleyebilirsiniz...
}

void* input_thread(void* arg) {
    while (1) {
        char input[100];
        printf("Enter a command: ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0'; // Satır sonu karakterini kaldır

        // Alınan komutları işleme fonksiyonuna gönder
        process_command(input);
    }
    return NULL;
}

int OCPP_Config_Read(void)
{
    FILE *fp = fopen("config.json", "r"); 

    if (fp == NULL) 
    { 
        printf("Error: Unable to open the file.\n"); 
        return 1; 
    }
    char buffer[2048]; 
    int len = fread(buffer, 1, sizeof(buffer), fp); 
    fclose(fp); 
  
    cJSON *json = cJSON_Parse(buffer);

    if (json == NULL) { 
        const char *error_ptr = cJSON_GetErrorPtr(); 
        if (error_ptr != NULL) { 
            printf("Error: %s\n", error_ptr); 
        } 
        cJSON_Delete(json); 
        return 1; 
    }

    ocpp_server_host = cJSON_GetObjectItemCaseSensitive(json, "OCPP_Server_Host"); 
    if (cJSON_IsString(ocpp_server_host) && (ocpp_server_host->valuestring != NULL))
    { 
        printf("OCPP_Server_Host: %s\n", ocpp_server_host->valuestring); 
    }

    ocpp_server_port = cJSON_GetObjectItemCaseSensitive(json, "OCPP_Server_Port"); 
    if (cJSON_IsNumber(ocpp_server_port) && (ocpp_server_port->valuestring != NULL))
    { 
        printf("OCPP_Server_Port: %d\n", ocpp_server_port->valueint); 
    }

    ocpp_server_path = cJSON_GetObjectItemCaseSensitive(json, "OCPP_Server_Path"); 
    if (cJSON_IsString(ocpp_server_path) && (ocpp_server_path->valuestring != NULL))
    { 
        printf("OCPP_Server_Path: %s\n", ocpp_server_path->valuestring); 
    }

    cp_id = cJSON_GetObjectItemCaseSensitive(json, "CP_ID"); 
    if (cJSON_IsString(cp_id) && (cp_id->valuestring != NULL))
    { 
        printf("CP_ID: %s\n", cp_id->valuestring); 
    }

    chargePointVendor = cJSON_GetObjectItemCaseSensitive(json, "ChargePointVendor"); 
    if (cJSON_IsString(chargePointVendor) && (chargePointVendor->valuestring != NULL))
    { 
        printf("ChargePointVendor: %s\n", chargePointVendor->valuestring); 
    }

    chargePointModel = cJSON_GetObjectItemCaseSensitive(json, "ChargePointModel"); 
    if (cJSON_IsString(chargePointModel) && (chargePointModel->valuestring != NULL))
    { 
        printf("ChargePointModel: %s\n", chargePointModel->valuestring); 
    }
}

int main(void)
{
    // UART_Init();
    if(OCPP_Config_Read() == 1)
    {
        return 1;
    }
    
    sprintf(OCPP_SERVER_FULL_PATH, "%s%s",ocpp_server_path->valuestring,cp_id->valuestring);

    //EVSE_ChargeState = 14;
    EVSE_CPstate = 1;

    //pthread_t input_pth;
    //pthread_create(&input_pth, NULL, input_thread, NULL);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    
    // CAN Interface Initialize
    if ((can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
    {
		fprintf(stderr, "Failed to open CAN port initialize\n");
		return -1;
	}
    strcpy(ifr.ifr_name, "can0" );
	ioctl(can_socket, SIOCGIFINDEX, &ifr);
    memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(can_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
		fprintf(stderr,"CAN port open succesfully\n");
		return -1;
	}
    pthread_create(&can_receive_pth, NULL, can_rx_thread, NULL);

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.ssl_ca_filepath = CACERT_FILE;
    info.ssl_cert_filepath = FULLCHAIN_PEM_FILE;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "libwebsocket init failed\n");
        return -1;
    }

    // Connect to the OCPP server
    struct lws_client_connect_info ccinfo = {
        .context = context,
        .address = ocpp_server_host->valuestring,
        .port = ocpp_server_port->valueint,
        .path = OCPP_SERVER_FULL_PATH,
        .host = ocpp_server_host->valuestring,
        .origin = ocpp_server_host->valuestring,
        .protocol = "ocpp1.6",
        .ssl_connection = LCCSCF_USE_SSL | LCCSCF_PIPELINE,
    };

    wsi = lws_client_connect_via_info(&ccinfo);
    
    if (!wsi) {
        fprintf(stderr, "libwebsocket connection failed\n");
        //lws_context_destroy(context);
        printf("%s", (char *)wsi);
        //return -1;
    }

    // Service the WebSocket connection
    while (1) {
        if (wsi == NULL) // ilk başta bağlanamadığında ,biraz gereksiz.
        {
            reconnect();
            sleep(2);
        }
        else if (errCodeReason == 28)
        {
            context = lws_create_context(&info);
            reconnect();
            sleep(2);
        }
        else
        {   
            lws_service(context, 0);
            lws_callback_on_writable(wsi);
        }
        
    }
    // Cleanup
    lws_context_destroy(context);

    // Temizlik işlemleri...
    //pthread_join(input_pth, NULL); // input_thread iş parçacığının tamamlanmasını bekleyin.
    close(uart_fd);
    return 0;
}
