#include "Client.h"

extern cJSON *ocpp_server_host;
extern cJSON *ocpp_server_port;
extern char OCPP_SERVER_FULL_PATH[1024];
extern cJSON *chargePointModel;
extern cJSON *chargePointVendor;

uint8_t meterValueOneMinCounter = 0;
bool finishedTransactionChecker = false;
uint heartBeatInterval = 120;
uint16_t EVCCID = 38;
uint8_t EVSE_Session = 0;
int ocppStateMachineState = 0;
volatile bool stop_meter_thread = false;


int32_t callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{        
    char received_frame_buffer[2048];
    errCodeReason = reason;
    switch (reason) 
    {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf(INFO"[CLIENT] Connected to the OCPP Server\n"RST);
            pthread_create(&ocpp_process_pth, NULL, ocpp_stateMachine, NULL);
            printf(INFO"[CLIENT] Started OCPP Client State Machine.\n"RST);
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf(RX"[SERVER] Received data from server : %.*s\n"RST, (int)len, (char *)in);
            sprintf(received_frame_buffer, "%.*s", (int)len, (char *)in);
            //sscanf(in, "[%d,", &process_id);
            sscanf(in, "[%*d,\"%36[^\"]\",", UUID);

            const char* json_data_str = rawMessage2cJSON( (const char *)in );
            //receiveBufferJSON = cJSON_Parse(json_data_str);
            transaction_id = GetTransactionID(json_data_str);
            int isRemoteFrameFlag = isReceivedRemoteMessage((const char *)received_frame_buffer);
            memset(received_frame_buffer, 0x00, 2048 * sizeof(char));
            if(isRemoteFrameFlag != 0)
            {
                switch (isRemoteFrameFlag)
                {
                case 1:
                    // Remote Start 
                    if( (EVSE_CPstate==1) && (EVSE_ChargeState != 1) )
                    {
                        writeFIFO((const char *)"RemoteStart");
                        sleep(1);

                        sendOCPPRemoteStartTransaction("Accepted");

                        sleep(1);
                        ocppStateMachineState = 2;
                    }
                    else
                    {
                        sendOCPPRemoteStartTransaction("Rejected");
                        sleep(1);
                    }
                    break;
                case 2:
                    // Remote Stop
                    if( (EVSE_CPstate==1) && (EVSE_ChargeState == 1) )
                    {
                        writeFIFO((const char *)"RemoteStop");
                        sleep(1);

                        sendOCPPRemoteStopTransaction("Accepted");
                        
                        sleep(1);
                        ocppStateMachineState = 3;
                    }
                    else
                    {
                        sendOCPPRemoteStopTransaction("Rejected");
                        sleep(1);
                    }
                    break;
                default:
                    break;
                }
            }
            break;
        case LWS_CALLBACK_CLOSED:
            printf(ERR"LWS_CALLBACK_CLOSED\n"RST);
            wsi = NULL;
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf(ERR"LWS_CALLBACK_CLIENT_CONNECTION_ERROR\n"RST);
            wsi = NULL;
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf(ERR"LWS_CALLBACK_CLIENT_CLOSED\n"RST);
            wsi = NULL;
            break;
        case LWS_CALLBACK_WSI_DESTROY:
            printf(ERR"LWS_CALLBACK_WSI_DESTROY\n"RST);
            lws_context_destroy(context);
            wsi = NULL;
            break;
        default:
            break;
    }
    return 0;
}

int8_t Client_Initialize(void)
{
    struct lws_protocols protocols[] = 
    {
        {
            "ocpp1.6",
            callback,
            0,
            0,
        },
        { NULL, NULL, 0, 0 }
    };
    memset(&info, 0, sizeof(info));

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.ssl_ca_filepath = CACERT_FILE;
    info.ssl_cert_filepath = FULLCHAIN_PEM_FILE;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    context = lws_create_context(&info);
    if (!context) 
    {
        fprintf(stderr, "libwebsocket init failed\n");
        return -1;
    }
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
    
    if (!wsi) 
    {
        fprintf(stderr, "libwebsocket connection failed\n");
        printf("%s", (char *)wsi);
    }
}

void Client_IsAwake(void)
{
    if (wsi == NULL)
    {
        Client_Reconnect();
        sleep(2);
    }
    else if (errCodeReason == 28)
    {
        context = lws_create_context(&info);
        Client_Reconnect();
        sleep(2);
    }
    else
    {   
        lws_service(context, 0);
        lws_callback_on_writable(wsi);
    }
}

void Client_Destroy(void)
{
    lws_context_destroy(context);
}

void Client_Reconnect(void)
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

    if (!wsi)
    {
        fprintf(stderr, "libwebsocket connection failed: %s\n", strerror(errno));
        lws_context_destroy(context);
    }

}

const char* rawMessage2cJSON(const char* input)
{
    const char* start = strchr(input, '{');
    const char* end = strrchr(input, '}');
    
    if (start && end) 
    {
        size_t length = end - start + 1;
        char* json_data = (char*)malloc(length + 1);
        strncpy(json_data, start, length);
        json_data[length] = '\0';
        return json_data;
    }
    return NULL;
}

const char* getErrorCodeForSession(int code) 
{
    switch(code) 
    {
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

const char* getDetailedErrorCodeForSession(int code) 
{
    switch(code) 
    {
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

int isReceivedRemoteMessage(const char * receivedStr)
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

int writeFIFO(const char * msg)
{
    int fd = open("/tmp/myfifo", O_WRONLY);
    if (fd == -1)
    {
        perror("Could not opened FIFO");
        return 0x00;
    }
    write(fd, msg, sizeof(msg) * (strlen(msg) + 1));
    printf(INFO"[FIFO] Sent %s to HMI\n"RST, msg);
    close(fd);
    return 0x01;
}

void getTimestamp(void)
{
    time_t currentTime;
    struct tm *timeInfo;

    time(&currentTime);
    timeInfo = gmtime(&currentTime);
    strftime(timestampBuffer, sizeof(timestampBuffer), "%Y-%m-%dT%H:%M:%S.000Z", timeInfo);
}

double mapValue(double value, double inMin, double inMax, double outMin, double outMax)
{
    double mappedValue = (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
    return mappedValue;
}

int64_t GetTransactionID(const char * jsonStr)
{
    cJSON *receiveBufferJSON;
    cJSON *transactionIDJSON;

    receiveBufferJSON = cJSON_Parse(jsonStr);

    transactionIDJSON = cJSON_GetObjectItemCaseSensitive(receiveBufferJSON, "transactionId"); 
    if (cJSON_IsNumber(transactionIDJSON) && (transactionIDJSON->valuestring != NULL))
    { 
        return (int64_t)transactionIDJSON->valueint;
    }
    else
    {
        return 0;
    }
}

int32_t sendOCPPFrame(int operation,const char *action, cJSON* jsonData)
{
    int32_t ret;
    snprintf(txBuffer, sizeof(txBuffer), "[%d,\"d9d6618e-d015-4e3d-ae4c-a6f0840b71fa\",\"%s\",%s]", operation, action, cJSON_Print(jsonData));
    ret = lws_write(wsi, (unsigned char *)txBuffer, strlen(txBuffer), LWS_WRITE_TEXT);
    memset(txBuffer, 0x00, 1000 * sizeof(char));

    return ret;
}

int32_t sendOCPPRemoteFrame(int operation, char* uuid, cJSON* jsonData)
{
    int32_t ret;
    snprintf(txBuffer, sizeof(txBuffer), "[%d,\"%s\",%s]", operation, uuid, cJSON_Print(jsonData));
    ret = lws_write(wsi, (unsigned char *)txBuffer, strlen(txBuffer), LWS_WRITE_TEXT);
    memset(txBuffer, 0x00, 1000 * sizeof(char));

    return ret;
}

void sendOCPPMeterValues(double voltage, double current, double power, double energy, int SoC, int64_t transaction_id, const char *timestamp)
{
    cJSON *meterValuesRootJson = NULL;
    cJSON *meterValuesJSON = NULL;
    cJSON *meterValueArray = NULL;
    cJSON *sampledValueArray = NULL;
    cJSON *sampledValueVoltageObj = NULL;
    cJSON *sampledValueCurrentObj = NULL;
    cJSON *sampledValuePowerObj = NULL;
    cJSON *sampledValueEnergyObj = NULL;
    cJSON *sampledValueSoCObj = NULL;
    char strVoltage[20];
    char strCurrent[20];
    char strEnergy[20];
    char strPower[20];
    char strSOC[20];

    meterValuesRootJson = cJSON_CreateObject();
    cJSON_AddNumberToObject(meterValuesRootJson, "connectorId", 1);
    cJSON_AddNumberToObject(meterValuesRootJson, "transactionId", transaction_id);
    meterValueArray = cJSON_AddArrayToObject(meterValuesRootJson, "meterValue");
    meterValuesJSON = cJSON_CreateObject();
    cJSON_AddStringToObject(meterValuesJSON, "timestamp", timestamp);
    sampledValueArray = cJSON_AddArrayToObject(meterValuesJSON, "sampledValue");
    
    sprintf(strVoltage, "%.2f", voltage);
    sampledValueVoltageObj = cJSON_CreateObject();
    cJSON_AddStringToObject(sampledValueVoltageObj, "value", strVoltage);
    cJSON_AddStringToObject(sampledValueVoltageObj, "context", "Sample.Periodic");
    cJSON_AddStringToObject(sampledValueVoltageObj, "format", "Raw");
    cJSON_AddStringToObject(sampledValueVoltageObj, "measurand", "Voltage");
    cJSON_AddStringToObject(sampledValueVoltageObj, "location", "Inlet");
    cJSON_AddStringToObject(sampledValueVoltageObj, "unit", "V");
    cJSON_AddItemToArray(sampledValueArray, sampledValueVoltageObj);
    
    sprintf(strCurrent, "%.2f", current);
    sampledValueCurrentObj = cJSON_CreateObject();
    cJSON_AddStringToObject(sampledValueCurrentObj, "value", strCurrent);
    cJSON_AddStringToObject(sampledValueCurrentObj, "context", "Sample.Periodic");
    cJSON_AddStringToObject(sampledValueCurrentObj, "format", "Raw");
    cJSON_AddStringToObject(sampledValueCurrentObj, "measurand", "Current.Import");
    cJSON_AddStringToObject(sampledValueCurrentObj, "location", "Outlet");
    cJSON_AddStringToObject(sampledValueCurrentObj, "unit", "A");
    cJSON_AddItemToArray(sampledValueArray, sampledValueCurrentObj);

    sprintf(strPower, "%.2f ", power);
    sampledValuePowerObj = cJSON_CreateObject();
    cJSON_AddStringToObject(sampledValuePowerObj, "value", strPower);
    cJSON_AddStringToObject(sampledValuePowerObj, "context", "Sample.Periodic");
    cJSON_AddStringToObject(sampledValuePowerObj, "format", "Raw");
    cJSON_AddStringToObject(sampledValuePowerObj, "measurand", "Power.Active.Import");
    cJSON_AddStringToObject(sampledValuePowerObj, "location", "Outlet");
    cJSON_AddStringToObject(sampledValuePowerObj, "unit", "W");
    cJSON_AddItemToArray(sampledValueArray, sampledValuePowerObj);

    sprintf(strEnergy, "%lf", energy);
    sampledValueEnergyObj = cJSON_CreateObject();
    cJSON_AddStringToObject(sampledValueEnergyObj, "value", strEnergy);
    cJSON_AddStringToObject(sampledValueEnergyObj, "context", "Sample.Periodic");
    cJSON_AddStringToObject(sampledValueEnergyObj, "format", "Raw");
    cJSON_AddStringToObject(sampledValueEnergyObj, "measurand", "Energy.Active.Import.Register");
    cJSON_AddStringToObject(sampledValueEnergyObj, "location", "Outlet");
    cJSON_AddStringToObject(sampledValueEnergyObj, "unit", "kWh");
    cJSON_AddItemToArray(sampledValueArray, sampledValueEnergyObj);

    sprintf(strSOC, "%d", SoC);
    sampledValueSoCObj = cJSON_CreateObject();
    cJSON_AddStringToObject(sampledValueSoCObj, "value", strSOC);
    cJSON_AddStringToObject(sampledValueSoCObj, "unit", "Percent");
    cJSON_AddStringToObject(sampledValueSoCObj, "measurand", "SoC");
    cJSON_AddItemToArray(sampledValueArray, sampledValueSoCObj);
    
    cJSON_AddItemToArray(meterValueArray, meterValuesJSON);
    char *jsonString = cJSON_Print(meterValuesRootJson);

    sendOCPPFrame(2, "MeterValues", meterValuesRootJson);
    printf(INFO"[CLIENT] Sent Meter Values. Voltage: %s, Current: %s, Power: %s, Energy: %s, SoC: %s, Transaction ID: %jd, Timestamp: %s\n"RST,strVoltage, strCurrent, strPower, strEnergy, strSOC, transaction_id, timestamp);
    
    /*
    cJSON_Delete(sampledValueVoltageObj);
    cJSON_Delete(sampledValueCurrentObj);
    cJSON_Delete(sampledValuePowerObj);
    cJSON_Delete(sampledValueEnergyObj);
    cJSON_Delete(sampledValueSoCObj);
    cJSON_Delete(sampledValueArray);
    cJSON_Delete(meterValueArray);
    cJSON_Delete(meterValuesJSON);
    */
    cJSON_Delete(meterValuesRootJson);
    
    memset(strVoltage, 0x00, sizeof(strVoltage));
    memset(strCurrent, 0x00, sizeof(strCurrent));
    memset(strPower, 0x00, sizeof(strPower));
    memset(strEnergy, 0x00, sizeof(strEnergy));
    memset(strSOC, 0x00, sizeof(strSOC));
}

void sendOCPPHeartBeat(void)
{
    cJSON *heartbeatJSON = NULL;

    heartbeatJSON = cJSON_CreateObject();
    cJSON_AddStringToObject(heartbeatJSON, "", "");

    sendOCPPFrame(2,"Heartbeat", heartbeatJSON);
    printf(INFO"[CLIENT] Sent HeartBeat.\n"RST);

    cJSON_Delete(heartbeatJSON);
}

void sendOCPPStatusNotification(uint8_t ConnectorID, const char * CPStatus, uint8_t EVSESession)
{
    cJSON *statusNotificationJSON = NULL;

    statusNotificationJSON = cJSON_CreateObject();
    cJSON_AddNumberToObject(statusNotificationJSON, "connectorId", ConnectorID);
    cJSON_AddStringToObject(statusNotificationJSON, "status", CPStatus);
    cJSON_AddStringToObject(statusNotificationJSON, "errorCode", getErrorCodeForSession(EVSESession));
    cJSON_AddStringToObject(statusNotificationJSON, "vendorErrorCode", getDetailedErrorCodeForSession(EVSESession));

    sendOCPPFrame(2, "StatusNotification", statusNotificationJSON);
    printf(INFO"[CLIENT] Sent Status Notification. Status : %s\n"RST, CPStatus);

    cJSON_Delete(statusNotificationJSON);
}

void sendOCPPBootNotification(const char *ChargePointModel, const char *ChargePointVendor)
{
    cJSON *bootNotificationJSON = NULL;

    bootNotificationJSON = cJSON_CreateObject();
    cJSON_AddStringToObject(bootNotificationJSON, "chargePointVendor", ChargePointVendor);
    cJSON_AddStringToObject(bootNotificationJSON, "chargePointModel", ChargePointModel);

    sendOCPPFrame(2, "BootNotification", bootNotificationJSON);
    printf(INFO"[CLIENT] Sent Boot Notification.\n"RST);

    cJSON_Delete(bootNotificationJSON);
}

void sendOCPPStartTransaction(uint8_t ConnectorID, const char * IDTag, double meterStart, const char * timestamp)
{
    cJSON *startTransactionJSON = NULL;

    startTransactionJSON = cJSON_CreateObject();
    cJSON_AddNumberToObject(startTransactionJSON, "connectorId", ConnectorID);
    cJSON_AddStringToObject(startTransactionJSON, "idTag", IDTag);
    cJSON_AddNumberToObject(startTransactionJSON, "meterStart", meterStart);
    cJSON_AddStringToObject(startTransactionJSON, "timestamp", timestamp);
    
    sendOCPPFrame(2, "StartTransaction", startTransactionJSON);
    printf(INFO"[CLIENT] Sent Start Transaction.\n"RST);
    
    cJSON_Delete(startTransactionJSON);
}

void sendOCPPStopTransaction(uint8_t ConnectorID, int64_t TransactionID, double meterStop, const char * timestamp)
{
    cJSON *stopTransactionJSON = NULL;

    stopTransactionJSON = cJSON_CreateObject();
    cJSON_AddNumberToObject(stopTransactionJSON, "connectorId", ConnectorID);
    cJSON_AddNumberToObject(stopTransactionJSON, "meterStop", (int)(meterStop*1000));
    cJSON_AddStringToObject(stopTransactionJSON, "timestamp", timestamp);                    
    cJSON_AddNumberToObject(stopTransactionJSON, "transactionId", TransactionID);

    sendOCPPFrame(2, "StopTransaction", stopTransactionJSON);
    printf(INFO"[CLIENT] Sent Stop Transaction.\n"RST);

    cJSON_Delete(stopTransactionJSON);
}

void sendOCPPRemoteStartTransaction(const char * CPStatus)
{
    cJSON *remoteStartTransactionJSON = NULL;

    remoteStartTransactionJSON = cJSON_CreateObject();
    cJSON_AddStringToObject(remoteStartTransactionJSON, "status", CPStatus);

    sendOCPPRemoteFrame(3, UUID, remoteStartTransactionJSON);
    printf(INFO"[CLIENT] Sent Remote Start Transaction. Status : %s\n"RST, CPStatus);

    cJSON_Delete(remoteStartTransactionJSON);
}

void sendOCPPRemoteStopTransaction(const char * CPStatus)
{
    cJSON *remoteStopTransactionJSON = NULL;

    remoteStopTransactionJSON = cJSON_CreateObject();
    cJSON_AddStringToObject(remoteStopTransactionJSON, "status", CPStatus);

    sendOCPPRemoteFrame(3, UUID, remoteStopTransactionJSON);
    printf(INFO"[CLIENT] Sent Remote Stop Transaction. Status : %s\n"RST, CPStatus);

    cJSON_Delete(remoteStopTransactionJSON);
}

void* meterValues_thread(void * param)
{
    while (!stop_meter_thread)
    {   
        /*
        if (EVSE_ChargeState != 1)
        {
            break;
        }
        */
        getTimestamp();
        sendOCPPMeterValues(EVPresentVoltage, EVPresentCurrent, EVPower, EVDeliveredEnergy, (int)EVSOC, transaction_id, (const char *)timestampBuffer);

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
        sendOCPPHeartBeat();
        sleep(heartBeatInterval);
    }
    return NULL;
}

void* ocpp_stateMachine(void * param)
{
    while(1)
    {
        switch(ocppStateMachineState)
        {
            case 0: // Startup the station
                if(EVSE_ChargeState == 1)
                {
                    sendOCPPStatusNotification(1, "Charging", EVSE_Session);
                    sleep(1);
                    FILE *fp = fopen("/root/transaction_id.txt", "r");
                    if (fp != NULL)
                    {
                        fscanf(fp, "%jd", &transaction_id);
                        fclose(fp);
                    }                    
                    getTimestamp();
                    //printf("Transaction ID : %d", transaction_id);
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
                } 
                
                switch(EVSE_CPstate)
                {
                    case 0:
                        sendOCPPBootNotification(chargePointModel->valuestring, chargePointVendor->valuestring); //*****//
                        sleep(1);
                        sendOCPPStatusNotification(1, "Available", EVSE_Session);
                        break;
                }
                pthread_create(&heartbeat_pth, NULL, heartbeat_thread, NULL);
                sleep(1);
                ocppStateMachineState = 1;
                break;
            case 1: // Wait Plug State
                if(!finishedTransactionChecker) 
                {
                    FILE *fp2 = fopen("/root/transaction_id.txt", "r");
                    if (fp2 != NULL)
                    {
                        fscanf(fp2, "%jd", &transaction_id);
                        fclose(fp2);
                    }
                }
                
                if (transaction_id != 0 && EVSE_ChargeState != 1 && !finishedTransactionChecker)
                {
                    getTimestamp();
                    sendOCPPStopTransaction(1, transaction_id, EVDeliveredEnergy, (const char *)timestampBuffer);
                    sleep(1);

                    FILE *fp1 = fopen("/root/transaction_id.txt", "w");
                    if (fp1 != NULL)
                    {
                        fprintf(fp1, "%jd", 0);
                        fclose(fp1);
                    }
                    finishedTransactionChecker = true;
                }

                if( EVSE_CPstate == 0x01 )    //if plug is connected
                {
                    switch(EVSE_CPstate)
                    {
                        case 0:
                            sendOCPPStatusNotification(1, "Available", EVSE_Session);
                            break;
                        case 1:
                            if (EVSE_ChargeState != 1)
                            {
                                sendOCPPStatusNotification(1, "Preparing", EVSE_Session);
                            }
                            break;
                        /*
                        case 3:
                            if (EVSE_ChargeState == 1)
                            {
                                sendOCPPStatusNotification(1, "Charging", EVSE_Session);
                            }
                            else
                            {
                                sendOCPPStatusNotification(1, "Preparing", EVSE_Session);
                            }
                            break;
                        */
                        default:
                            break;
                    }
                    sleep(1);
                    ocppStateMachineState = 2;
                }
                break;
            case 2: // Charging State
                if(EVSE_ChargeState == 1)  //Charging Startup
                {
                    switch(EVSE_CPstate)
                    {
                        case 0:
                            sendOCPPStatusNotification(1, "Available", EVSE_Session);
                            break;
                        case 1:
                            if(EVSE_ChargeState == 1)
                            {
                                sendOCPPStatusNotification(1, "Charging", EVSE_Session);
                            }
                            else
                            {
                                sendOCPPStatusNotification(1, "Preparing", EVSE_Session);
                            }
                            break;
                        default:
                            break;
                    }

                    sleep(1);
                    sprintf(evccidStr, "%u", EVCCID);
                    /*
                    FILE *fp = fopen("/root/evccid.txt", "w");
                    if (fp != NULL)
                    {
                        fprintf(fp, "%s", evccidStr);
                        fclose(fp);
                    }
                    */
                    sleep(1);
                    getTimestamp();
                    sendOCPPStartTransaction(1, evccidStr, 0, timestampBuffer);
                    sleep(1);
                    //transaction_id = cJSON_GetObjectItem(receiveBufferJSON, "transactionId")->valueint;
                    if(transaction_id != 0)
                    {
                        FILE *fp1 = fopen("/root/transaction_id.txt", "w");
                        if (fp1 != NULL)
                        {
                            fprintf(fp1, "%jd", transaction_id);
                            fclose(fp1);
                        }
                    }
                    sleep(1);
                    stop_meter_thread = false;
                    pthread_create(&meterValues_pth, NULL, meterValues_thread, NULL);
                    ocppStateMachineState = 3;
                }
                break;
            case 3: // Charging Finished State
                if(EVSE_ChargeState != 1)
                {
                    sendOCPPStatusNotification(1, "Finishing", EVSE_Session);
                    sleep(1);
                    FILE *fp = fopen("/root/transaction_id.txt", "r");
                    if (fp != NULL)
                    {
                        fscanf(fp, "%jd", &transaction_id);
                        fclose(fp);
                    }
                    getTimestamp();
                    sendOCPPStopTransaction(1, transaction_id, EVDeliveredEnergy, (const char *)timestampBuffer);
                    sleep(1);

                    stop_meter_thread = true;
                    if (pthread_join(meterValues_pth, NULL) != 0) 
                    {
                        perror("Failed to join thread");
                    }
                    /*
                    FILE *fp1 = fopen("/root/transaction_id.txt", "w");
                    
                    if (fp1 != NULL && (idTagInfo->valuestring == "Accepted"))
                    {
                        fprintf(fp1, "%d", 0);
                        fclose(fp1);
                    }
                    
                    idTagInfo = NULL;
                    */
                    ocppStateMachineState = 4;
                }                     
                break;
            case 4: //  Wait Unplug State
                if( EVSE_CPstate == 1 )
                {
                    ocppStateMachineState = 4;
                }
                else
                {
                    sendOCPPStatusNotification(1, "Available", EVSE_Session);
                    sleep(1);
                    ocppStateMachineState = 1;
                }
                break;
        }
    }
}