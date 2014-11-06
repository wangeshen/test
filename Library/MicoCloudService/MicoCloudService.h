/**
******************************************************************************
* @file    CloudUtils.h 
* @author  Eshen Wang
* @version V1.0.0
* @date    15-Oct-2014
* @brief   This header contains function prototypes of cloud service based
           on MICO platform. 
  operation
******************************************************************************
* @attention
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
******************************************************************************
*/ 

#ifndef __MICO_CLOUD_SERVICE_H_
#define __MICO_CLOUD_SERVICE_H_

//#include "Common.h"
#include "MICODefine.h"

#include "MicoCloudServiceDef.h"

/*******************************************************************************
 * DEFINES
 ******************************************************************************/

/*******************************************************************************
 * STRUCTURES
 ******************************************************************************/

typedef enum {
  CLOUD_SERVICE_STATUS_STOPPED = 1, //service stopped
  CLOUD_SERVICE_STATUS_STARTED = 2, //service start up
  CLOUD_SERVICE_STATUS_CONNECTED = 3, //service work ok
  CLOUD_SERVICE_STATUS_DISCONNECTED = 4 //service diconnected from server
} cloudServiceState;

typedef struct _cloud_service_info_t {
  //cloud server info
  char *host;
  uint16_t port;
  
  //device info
  char *bssid;
  char *product_id;
  char *priduct_key;
  
  //user info, not check now
//  char *login_id;
//  char *password;
  char *user_token;
  
  //mqtt client info
  char *mqtt_server_host;
  uint16_t mqtt_server_port;
  msgRecvHandler hmsg;
  uint16_t mqtt_keepAliveInterval;
} cloud_service_config_t;

typedef struct _cloud_service_status_t {
  cloudServiceState state;
} cloud_service_status_t;

typedef struct _device_status_t {
  bool isActivated;
  bool isAuthorized;
} device_status_t;

typedef struct _cloud_service_context_t {
  /*cloud service config info*/
  cloud_service_config_t service_config_info;
  
  /*cloud service running status*/
  cloud_service_status_t service_status;
  
  /*device status*/
  device_status_t device_status;
} cloud_servcie_context_t;

/*******************************************************************************
 * USER INTERFACES
 ******************************************************************************/

void MicoCloudServiceInit(cloud_service_config_t init);
OSStatus MicoCloudServiceStart(mico_Context_t *inContext);
OSStatus MicoCloudServiceUpload(const unsigned char *msg, unsigned int msglen);
cloudServiceState MicoCloudServiceState(void);
OSStatus MicoCloudServiceStop(void);

char* MicoCloudServiceVersion(void);


#endif
