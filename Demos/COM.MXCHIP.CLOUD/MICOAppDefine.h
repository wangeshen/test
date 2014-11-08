/**
  ******************************************************************************
  * @file    MICOAppDefine.h 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   This file create a TCP listener thread, accept every TCP client
  *          connection and create thread for them.
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


#ifndef __MICOAPPDEFINE_H
#define __MICOAPPDEFINE_H

#include "Common.h"
#include "MicoCloudServiceDef.h"

#define APP_INFO   "mxchipWNet cloud Demo based on MICO OS"

#define FIRMWARE_REVISION   "MICO_CLOUD_0_1"
#define MANUFACTURER        "MXCHIP Inc."
#define SERIAL_NUMBER       "20141031"
#define PROTOCOL            "com.mxchip.cloud"

/*User provided configurations*/
#define CONFIGURATION_VERSION               0x00000001 // if default configuration is changed, update this number
#define MAX_Local_Client_Num                8
#define LOCAL_PORT                          8080
#define DEAFULT_REMOTE_SERVER               "192.168.2.254"
#define DEFAULT_REMOTE_SERVER_PORT          8080
#define UART_RECV_TIMEOUT                   200
#define UART_ONE_PACKAGE_LENGTH             1024
#define wlanBufferLen                       1024
#define UART_BUFFER_LENGTH                  2048
#define UART_FOR_APP                        MICO_UART_1

#define LOCAL_TCP_SERVER_LOOPBACK_PORT      1000
#define REMOTE_TCP_CLIENT_LOOPBACK_PORT     1002
#define RECVED_UART_DATA_LOOPBACK_PORT      1003

#define BONJOUR_SERVICE                     "_easylink._tcp.local."

//device info
#define DEFAULT_PRODUCT_ID               "f315fea0-50fc-11e4-b6fc-f23c9150064b"
#define DEFAULT_PRODUCT_KEY              "41a71625-5519-11e4-ad4e-f23c9150064b"
#define DEFAULT_USER_TOKEN               "00000000"
#define DEFAULT_DEVICE_ID                "00000000"
#define DEFAULT_DEVICE_KEY               "00000000"

/*Application's configuration stores in flash*/
typedef struct
{
  uint32_t          configDataVer;
  uint32_t          localServerPort;

  /*local services*/
  bool              localServerEnable;
  bool              remoteServerEnable;
  char              remoteServerDomain[64];
  int               remoteServerPort;
  
  /*IO settings*/
  uint32_t          USART_BaudRate;
  
  /* cloud setting */
  char              cloudServerDomain[64];
  int               cloudServerPort;
  char              mqttServerDomain[64];
  int               mqttServerPort;
  uint16_t          mqttkeepAliveInterval;

  char product_id[MAX_PRODUCT_ID_STRLEN];
  char product_key[MAX_PRODUCT_KEY_STRLEN];
  
  bool isAcitivated;  //device activate flag
  char user_token[MAX_USER_TOKEN_STRLEN];  // used to authoreze for user
  char device_id[MAX_DEVICE_ID_STRLEN];  // used for mqtt client
  char master_device_key[MAX_DEVICE_KEY_STRLEN];
} application_config_t;

/*Running status*/
typedef struct _current_app_status_t {
  /*Local clients port list*/
  uint32_t          loopBack_PortList[MAX_Local_Client_Num];
  /*Remote TCP client connecte*/
  bool              isRemoteConnected;
  /* cloud service connected (MQTT) */
  bool              isCloudConnected;
} current_app_status_t;


void localTcpServer_thread(void *inContext);
void uartRecv_thread(void *inContext);



#endif

