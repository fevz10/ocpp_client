#include "Config.h"

char OCPP_SERVER_FULL_PATH[1024];
cJSON *ocpp_server_host = NULL;
cJSON *ocpp_server_port = NULL;
cJSON *ocpp_server_path = NULL;
cJSON *cp_id = NULL;
cJSON *chargePointVendor = NULL;
cJSON *chargePointModel = NULL;

void Config_Initialize(void)
{
    Config_Read();
}

void Config_Read(void)
{
    FILE *fp = fopen("config.json", "r"); 

    if (fp == NULL) 
    { 
        printf("Error: Unable to open the file.\n"); 
        return; 
    }
    char buffer[2048]; 
    (int)fread(buffer, 1, sizeof(buffer), fp); 
    fclose(fp); 
  
    cJSON *json = cJSON_Parse(buffer);

    if (json == NULL) 
    { 
        const char *error_ptr = cJSON_GetErrorPtr(); 
        if (error_ptr != NULL) 
        { 
            printf("Error: %s\n", error_ptr); 
        } 
        cJSON_Delete(json); 
        return; 
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

    sprintf(OCPP_SERVER_FULL_PATH, "%s%s",ocpp_server_path->valuestring,cp_id->valuestring);
}