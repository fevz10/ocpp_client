#ifndef CLIENT_H
#define CLIENT_H

#include <libwebsockets.h>
#include "Utils.h"
#include "cJSON.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>


#define FULLCHAIN_PEM_FILE "ca_certificate.crt"
#define CACERT_FILE "cacert.pem"

struct lws *wsi;
struct lws_context *context;
char txBuffer[1000];
struct lws_context_creation_info info;
int errCodeReason;

pthread_t ocpp_process_pth;
pthread_t heartbeat_pth;
pthread_t meterValues_pth;

char evccidStr[20];
char timestampBuffer[30];

uint8_t EVSE_CPstate;
uint8_t EVSE_ChargeState;
double EVPresentVoltage;
double EVPresentCurrent;
double EVSOC;
double EVDeliveredEnergy;
double EVPower;
int transaction_id;
char UUID[37];


int callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

void* ocpp_stateMachine(void * param);
void* meterValues_thread(void * param);
void* heartbeat_thread(void * param);
void getTimestamp(void);

int sendOCPPFrame(int operation,const char *action, cJSON* jsonData);
int sendOCPPRemoteFrame(int operation, char* uuid, cJSON* jsonData);

int Client_Initialize(void);
void Client_IsAwake(void);
void Client_Reconnect(void);
void Client_Destroy(void);

void sendOCPPMeterValues(double voltage, double current, double power, double energy, int SoC, int transaction_id, char *timestamp);
void sendOCPPHeartBeat(void);
void sendOCPPStatusNotification(uint8_t ConnectorID, char * CPStatus, uint8_t EVSESession);
void sendOCPPBootNotification(char *ChargePointModel, char *ChargePointVendor);
void sendOCPPStartTransaction(uint8_t ConnectorID, char * IDTag, int meterStart, char * timestamp);
void sendOCPPStopTransaction(uint8_t ConnectorID, int TransactionID, double meterStop, char * timestamp);
void sendOCPPRemoteStartTransaction(char * CPStatus);
void sendOCPPRemoteStopTransaction(char * CPStatus);

const char* getErrorCodeForSession(int code);
const char* getDetailedErrorCodeForSession(int code);
char* rawMessage2cJSON(const char* input);
int isReceivedRemoteMessage(char *receivedStr);
int writeFIFO(const char * msg);
int GetTransactionID(char * jsonStr);
double mapValue(double value, double inMin, double inMax, double outMin, double outMax);

#endif // CLIENT_H