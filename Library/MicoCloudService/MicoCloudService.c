/**
******************************************************************************
* @file    CloudUtils.c
* @author  Eshen Wang
* @version V1.0.0
* @date    15-Oct-2014
* @brief   This file contains implementation of cloud services based
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

#include "MICO.h"
#include "MICONotificationCenter.h"

#include "JSON-C/json.h"
#include "SocketUtils.h"
#include "HTTPUtils.h"
#include "StringUtils.h"
#include "MicoAlgorithm.h"

#include "MicoMQTTClient.h"
#include "MicoCloudService.h"


//#define  debug_out 

//#ifdef debug_out
//#define  _debug_out debug_out
//#else
#define _debug_out(format, ...) do {;}while(0)

#define mico_cloud_service_log(M, ...) custom_log("CloudService", M, ##__VA_ARGS__)
#define mico_cloud_service_log_trace() custom_log_trace("CloudService")
//#endif

/*******************************************************************************
 * DEFINES
 ******************************************************************************/

//request type
#define DEVICE_ACTIVATE                  0
#define DEVICE_AUTHORIZE                 1
   
#define DEFAULT_DEVICE_ACTIVATE_URL      "/v1/device/activate"
#define DEFAULT_DEVICE_AUTHORIZE_URL     "/v1/device/authorize"

/*******************************************************************************
 * VARIABLES
 ******************************************************************************/

// cloud service info
static cloud_servcie_context_t cloudServiceContext = {0};
static mico_mutex_t cloud_service_context_mutex = NULL;

//wifi connect status
static volatile bool _wifiConnected = false;
static mico_semaphore_t  _wifiConnected_sem = NULL;

//mqtt client info
static mqtt_client_config_t mqtt_client_config_info = {0};

static mico_thread_t cloud_service_thread_handler = NULL;

/*******************************************************************************
 * STATIC FUNCTIONS
 ******************************************************************************/

static void _cloud_service_thread(void *arg);

static OSStatus _device_activate_authorize(int request_type, char *host, char *request_url, char *product_id, char *bssid, char *device_token, char *user_token, char out_device_id[MAX_DEVICE_ID_STRLEN], char out_master_device_key[MAX_DEVICE_KEY_STRLEN]);
//static OSStatus _device_authorize( char *product_id,  char *bssid, char *device_token, char *user_token, char out_device_id[MAX_DEVICE_ID_STRLEN]);

static OSStatus _parseResponseMessage(int fd, HTTPHeader_t* inHeader, char out_device_id[MAX_DEVICE_ID_STRLEN], char out_master_device_key[MAX_DEVICE_KEY_STRLEN]);
static OSStatus _configIncommingJsonMessage( const char *input, unsigned int len, char out_device_id[MAX_DEVICE_ID_STRLEN], char out_master_device_key[MAX_DEVICE_KEY_STRLEN]);
//static OSStatus _getHttpChunkedJsonData( const char *input, char **outHttpJsonData, int *outDataLen);

static OSStatus _cal_device_token(char* bssid, char* product_key, char out_device_token[32]);
//static OSStatus _cal_user_token(const char* bssid, const char* login_id, const char * login_passwd, unsigned char out_user_token[16]);

/*******************************************************************************
 * EXTERNS
 ******************************************************************************/

extern mico_thread_t mqtt_client_thread_handler;
//extern void  CloudServiceStartedCallback(CloudServiceStatus status);

/*******************************************************************************
 * IMPLEMENTATIONS
 ******************************************************************************/

void cloudNotify_WifiStatusHandler(int event, mico_Context_t * const inContext)
{
  mico_cloud_service_log_trace();
  (void)inContext;
  switch (event) {
  case NOTIFY_STATION_UP:
    _wifiConnected = true;
    mico_rtos_set_semaphore(&_wifiConnected_sem);
    break;
  case NOTIFY_STATION_DOWN:
    _wifiConnected = false;
    break;
  default:
    break;
  }
  return;
}

void MicoCloudServiceInit(cloud_service_config_t init)
{
  if(cloud_service_context_mutex == NULL)
    mico_rtos_init_mutex( &cloud_service_context_mutex );

  mico_rtos_lock_mutex( &cloud_service_context_mutex );
  //service init
  cloudServiceContext.service_config_info = init;
  cloudServiceContext.service_status.state = CLOUD_SERVICE_STATUS_STOPPED;
  //device status init
  cloudServiceContext.device_status.isActivated = false;
  cloudServiceContext.device_status.isAuthorized = false;
  mico_rtos_unlock_mutex( &cloud_service_context_mutex );
}

OSStatus MicoCloudServiceStop(void)
{
  OSStatus err = kNoErr;
  
  MicoMQTTClientStop();
  
  if (NULL != mqtt_client_thread_handler)
    mico_rtos_thread_join(&mqtt_client_thread_handler);

  MICORemoveNotification( mico_notify_WIFI_STATUS_CHANGED, (void *)cloudNotify_WifiStatusHandler );
  if(_wifiConnected_sem) mico_rtos_deinit_semaphore(&_wifiConnected_sem);
  
  mico_rtos_lock_mutex( &cloud_service_context_mutex );
  cloudServiceContext.service_status.state = CLOUD_SERVICE_STATUS_STOPPED;
  mico_rtos_unlock_mutex( &cloud_service_context_mutex );
    
  mico_cloud_service_log("cloud service stop.");
  if (NULL != cloud_service_thread_handler) {
    mico_rtos_delete_thread(&cloud_service_thread_handler);
    cloud_service_thread_handler = NULL;
  }
  
  return err;
}

cloudServiceState MicoCloudServiceState(void)
{
  cloudServiceState state = CLOUD_SERVICE_STATUS_STOPPED;
  
  mico_rtos_lock_mutex( &cloud_service_context_mutex );
  state = cloudServiceContext.service_status.state;
  mico_rtos_unlock_mutex( &cloud_service_context_mutex );
  
  return state;
}

OSStatus MicoCloudServiceUpload(const unsigned char* msg, unsigned int msglen)
{
  int ret = kUnknownErr;
  mico_rtos_lock_mutex( &cloud_service_context_mutex );
  char *pubtopic = mqtt_client_config_info.pubtopic;
  mico_rtos_unlock_mutex( &cloud_service_context_mutex );
  
  if (NULL == msg || 0 == msglen)
    return kParamErr;
  
  if (CLOUD_SERVICE_STATUS_CONNECTED != MicoCloudServiceState()){
    return kStateErr;
  }
  
  ret = MicoMQTTClientPublish(pubtopic, msg, msglen);
  return ret;
}

/* cloud service main thread */
void _cloud_service_thread(void *arg)
{
  mico_Context_t* inContext = (mico_Context_t*)arg;
  OSStatus err = kUnknownErr;
  //  micoMemInfo_t *memInfo = NULL;
  
  /* thread local var */
  bool isAcitivated = false;
  bool isAuthorized = false;

  char product_id[MAX_PRODUCT_ID_STRLEN] = {0};
  char product_key[MAX_PRODUCT_KEY_STRLEN] = {0};

  char device_token[MAX_DEVICE_TOKEN_STRLEN] = {0};
  char user_token[MAX_USER_TOKEN_STRLEN] = {0};

  char device_id[MAX_DEVICE_ID_STRLEN] = {0};
  char master_device_key[MAX_DEVICE_ID_STRLEN] = {0};

  char subscribe_topic[MAX_SUBSCRIBE_TOPIC_STRLEN] = {0};
  char publish_topic[MAX_PUBLISH_TOPIC_STRLEN] = {0};
  
  mico_cloud_service_log("_cloud_service_thread start...");
  
  /* Regisist wifi connect notifications */
  err = MICOAddNotification( mico_notify_WIFI_STATUS_CHANGED, (void *)cloudNotify_WifiStatusHandler );
  require_noerr( err, exit );
  
  /* get device info from flash */
  mico_rtos_lock_mutex(&inContext->flashContentInRam_mutex);
  isAcitivated = inContext->flashContentInRam.appConfig.isAcitivated;
  strncpy(product_id, inContext->flashContentInRam.appConfig.product_id, MAX_PRODUCT_ID_STRLEN);
  strncpy(product_key, inContext->flashContentInRam.appConfig.product_key, MAX_PRODUCT_KEY_STRLEN);
  strncpy(user_token, inContext->flashContentInRam.appConfig.user_token, MAX_USER_TOKEN_STRLEN);
  mico_rtos_unlock_mutex(&inContext->flashContentInRam_mutex);
  
ReStartService:
  mico_rtos_lock_mutex( &cloud_service_context_mutex );
  cloudServiceContext.service_status.state = CLOUD_SERVICE_STATUS_STARTED;
  mico_rtos_unlock_mutex( &cloud_service_context_mutex );
  
  /* wait for wifi connect */
  if(_wifiConnected == false) {
    mico_cloud_service_log("cloud service wait for wifi connect...");
    mico_rtos_get_semaphore(&_wifiConnected_sem, MICO_WAIT_FOREVER);
  }

  /* 1. Device active or authorize
   * use cloudServiceContext.service_config_info to get:
   * device_id:device_key, which will be used in MQTT client (set mqtt_client_config_info)
   */
  //strncpy(device_token, "e52be374c47c391414ab68c971954d13", strlen("e52be374c47c391414ab68c971954d13"));
ReActivate:
  if (!isAcitivated) {
    //cal device_token(MD5)
    err = _cal_device_token(inContext->micoStatus.mac, product_key, device_token);
    require_noerr( err, ReActivate );
    mico_cloud_service_log("calculate device_token[%d]=%s", strlen(device_token), device_token);
    //activate, get device_id, master_device_key from server
    err = _device_activate_authorize(DEVICE_ACTIVATE, 
                                     cloudServiceContext.service_config_info.host, 
                                     (char *)DEFAULT_DEVICE_ACTIVATE_URL,
                                     product_id, inContext->micoStatus.mac,
                                     device_token, user_token,
                                     device_id, master_device_key);
    require_noerr( err, ReActivate );
    
    isAcitivated = true;
    //write back to flash
    mico_rtos_lock_mutex(&inContext->flashContentInRam_mutex);
    inContext->flashContentInRam.appConfig.isAcitivated = isAcitivated;
    strncpy(inContext->flashContentInRam.appConfig.device_id, device_id, MAX_DEVICE_ID_STRLEN);
    strncpy(inContext->flashContentInRam.appConfig.master_device_key, master_device_key, MAX_DEVICE_KEY_STRLEN);
    mico_rtos_unlock_mutex(&inContext->flashContentInRam_mutex);
    MICOUpdateConfiguration(inContext);
  }
//  else
//  {
//    mico_rtos_lock_mutex(&inContext->flashContentInRam_mutex);
//    isAcitivated = inContext->flashContentInRam.appConfig.isAcitivated;
//    strncpy(device_id, inContext->flashContentInRam.appConfig.device_id, MAX_DEVICE_ID_STRLEN);
//    strncpy(master_device_key, inContext->flashContentInRam.appConfig.master_device_key, MAX_DEVICE_KEY_STRLEN);
//    mico_rtos_unlock_mutex(&inContext->flashContentInRam_mutex);
//  }
  
ReAuthorize:
  if (!isAuthorized){
    //cal device_token(MD5)
    err = _cal_device_token(inContext->micoStatus.mac, product_key, device_token);
    require_noerr( err, ReActivate );
    //use user_token in flash to authorize
    err = _device_activate_authorize(DEVICE_AUTHORIZE, cloudServiceContext.service_config_info.host, (char *)DEFAULT_DEVICE_AUTHORIZE_URL,
                                     product_id, inContext->micoStatus.mac,
                                     device_token, user_token,
                                     device_id, master_device_key);
    require_noerr( err, ReAuthorize);
    
    isAuthorized = true;
    //write back to flash
    mico_rtos_lock_mutex(&inContext->flashContentInRam_mutex);
    strncpy(inContext->flashContentInRam.appConfig.device_id, device_id, MAX_DEVICE_ID_STRLEN);
    //strncpy(inContext->flashContentInRam.appConfig.user_token, user_token, MAX_USER_TOKEN_STRLEN);
    mico_rtos_unlock_mutex(&inContext->flashContentInRam_mutex);
    MICOUpdateConfiguration(inContext);
  }
//  else
//  {
//    mico_rtos_lock_mutex(&inContext->flashContentInRam_mutex);
//    strncpy(device_id, inContext->flashContentInRam.appConfig.device_id, MAX_DEVICE_ID_STRLEN);
//    strncpy(user_token, inContext->flashContentInRam.appConfig.user_token, MAX_USER_TOKEN_STRLEN);
//    mico_rtos_unlock_mutex(&inContext->flashContentInRam_mutex);
//  }

  /* 2. start MQTT client */  
  /* set publish && subscribe topic for MQTT client
   * subscribe_topic = device_id/in
   * publish_topic  = device_id/out
   */
  memset(subscribe_topic, 0, sizeof(subscribe_topic));
  strncpy(subscribe_topic, device_id, strlen(device_id));
  strncat(subscribe_topic, "/in", 3);
  
  memset(publish_topic, 0, sizeof(publish_topic));
  strncpy(publish_topic, device_id, strlen(device_id));
  strncat(publish_topic, "/out", 4);
  
  mico_cloud_service_log("subscribe_topic=%s, publish_topic=%s", subscribe_topic, publish_topic);

ReStartMQTTClient:  
  mico_rtos_lock_mutex( &cloud_service_context_mutex );
  mqtt_client_config_info.host = cloudServiceContext.service_config_info.mqtt_server_host;
  mqtt_client_config_info.port = cloudServiceContext.service_config_info.mqtt_server_port;
  mqtt_client_config_info.client_id = cloudServiceContext.service_config_info.bssid;
  mqtt_client_config_info.subscribe_qos = QOS2;  //here for subscribe, just for onece
  mqtt_client_config_info.username = master_device_key;  //use device_key as the username
  mqtt_client_config_info.password = "no-use";   //server not check temporary
  mqtt_client_config_info.subtopic = subscribe_topic;
  mqtt_client_config_info.pubtopic = publish_topic;
  mqtt_client_config_info.hmsg = cloudServiceContext.service_config_info.hmsg;  //msg recv message user handler
  mqtt_client_config_info.keepAliveInterval = cloudServiceContext.service_config_info.mqtt_keepAliveInterval; // heart break interval
  mico_rtos_unlock_mutex( &cloud_service_context_mutex );
  
  MicoMQTTClientInit(mqtt_client_config_info);
  err = MicoMQTTClientStart(inContext);
  require_noerr( err, exit );
  
  /* 3. wait for MQTT client start up. */
  while(1){
    if(MQTT_CLIENT_STATUS_CONNECTED == MicoMQTTClientState())
      break;
    else {
      mico_cloud_service_log("wait for mqtt client start up...");
      mico_thread_sleep(1);
    }
  }
  mico_rtos_lock_mutex( &cloud_service_context_mutex );
  cloudServiceContext.service_status.state = CLOUD_SERVICE_STATUS_CONNECTED;
  mico_rtos_unlock_mutex( &cloud_service_context_mutex );
  
  /* 4. cloud service started callback, notify user. */
  //CloudServiceStartedCallback(NOTIFY_CLOUD_SERVICE_UP);
  inContext->appStatus.isRemoteConnected = true;
  
  /* service loop */
  while(1) {
//    memInfo = mico_memory_info();
//    mico_cloud_service_log("system free mem[cloud service]=%d", memInfo->free_memory);
  
    if(CLOUD_SERVICE_STATUS_STOPPED == MicoCloudServiceState()){
      goto cloud_service_stop;
    }
    
    if(_wifiConnected){
      switch(MicoMQTTClientState()){
      case MQTT_CLIENT_STATUS_STOPPED:
        mico_cloud_service_log("mqtt client stopped! try restarting it after 3 seconds...");
        mico_rtos_lock_mutex( &cloud_service_context_mutex );
        cloudServiceContext.service_status.state = CLOUD_SERVICE_STATUS_DISCONNECTED;
        mico_rtos_unlock_mutex( &cloud_service_context_mutex );
        //CloudServiceStartedCallback(NOTIFY_CLOUD_SERVICE_DOWN);
        inContext->appStatus.isRemoteConnected = false;
        mico_thread_sleep(3);
        goto ReStartMQTTClient;
        break;
      case MQTT_CLIENT_STATUS_STARTED:
        mico_cloud_service_log("cloud service: mqtt client connecting...");
        mico_rtos_lock_mutex( &cloud_service_context_mutex );
        cloudServiceContext.service_status.state = CLOUD_SERVICE_STATUS_DISCONNECTED;
        mico_rtos_unlock_mutex( &cloud_service_context_mutex );
        inContext->appStatus.isRemoteConnected = true;
        //CloudServiceStartedCallback(NOTIFY_CLOUD_SERVICE_DOWN);
        break;
      case MQTT_CLIENT_STATUS_CONNECTED:
        //mico_cloud_service_log("cloud service runing...");
        mico_rtos_lock_mutex( &cloud_service_context_mutex );
        cloudServiceContext.service_status.state = CLOUD_SERVICE_STATUS_CONNECTED;
        mico_rtos_unlock_mutex( &cloud_service_context_mutex );
        inContext->appStatus.isRemoteConnected = false;
        //CloudServiceStartedCallback(NOTIFY_CLOUD_SERVICE_DOWN);
        break;
      case MQTT_CLIENT_STATUS_DISCONNECTED:
        mico_cloud_service_log("cloud service: mqtt client disconnected!");
        mico_rtos_lock_mutex( &cloud_service_context_mutex );
        cloudServiceContext.service_status.state = CLOUD_SERVICE_STATUS_DISCONNECTED;
        mico_rtos_unlock_mutex( &cloud_service_context_mutex );
        //CloudServiceStartedCallback(NOTIFY_CLOUD_SERVICE_DOWN);
        inContext->appStatus.isRemoteConnected = false;
        break;
      default:
        break;
      }
    }
    else {
      mico_cloud_service_log("wifi disconnect! restart cloud service after 5 seconds...");
      mico_rtos_lock_mutex( &cloud_service_context_mutex );
      cloudServiceContext.service_status.state = CLOUD_SERVICE_STATUS_DISCONNECTED;
      mico_rtos_unlock_mutex( &cloud_service_context_mutex );
      MicoMQTTClientStop();
      //CloudServiceStartedCallback(NOTIFY_CLOUD_SERVICE_DOWN);
      inContext->appStatus.isRemoteConnected = false;
      mico_thread_sleep(5);
      goto ReStartService;
    }
    
    mico_thread_sleep(5);
  }
  
cloud_service_stop:
  //CloudServiceStartedCallback(NOTIFY_CLOUD_SERVICE_DOWN);
  inContext->appStatus.isRemoteConnected = false;
  MicoMQTTClientStop();
  if (NULL != mqtt_client_thread_handler)
    mico_rtos_thread_join(&mqtt_client_thread_handler);
  
exit:
  mico_cloud_service_log("Exit: cloud thread exit with err = %d", err);
  MICORemoveNotification( mico_notify_WIFI_STATUS_CHANGED, (void *)cloudNotify_WifiStatusHandler );
  if(_wifiConnected_sem) mico_rtos_deinit_semaphore(&_wifiConnected_sem);
  mico_rtos_delete_thread(NULL);
  mqtt_client_thread_handler = NULL;
  return;
}

OSStatus MicoCloudServiceStart(mico_Context_t* inContext)
{
  mico_rtos_init_semaphore(&_wifiConnected_sem, 1);
  return mico_rtos_create_thread(&cloud_service_thread_handler, MICO_APPLICATION_PRIORITY, "Cloud service", _cloud_service_thread, 0x800, inContext );
}

//http host: "api.easylink.io"
//activate_url= "/v1/device/activate"
//authorize_url = "/v1/device/authorize"
static OSStatus _device_activate_authorize(int request_type, char *host, char *request_url, char *product_id, char *bssid, char *device_token, char *user_token, char out_device_id[MAX_DEVICE_ID_STRLEN], char out_master_device_key[MAX_DEVICE_KEY_STRLEN])
{
  mico_cloud_service_log("device activated[0] or authorize[1] start: request_type=[%d].", request_type);
  OSStatus err = kUnknownErr;
  bool isDone = false;
  
  int remoteTcpClient_fd = -1;
  char ipstr[16];
  struct sockaddr_t addr;
  fd_set readfds;
  
  struct timeval_t t;
  t.tv_sec = 1;
  t.tv_usec = 0;
  
  uint8_t *httpRequestData = NULL;
  size_t httpRequestDataLen = 0;
  HTTPHeader_t *httpHeader = NULL;
 
  /* create activate or authorize http request json data */
  //uint8_t* json_str = "{\"product_id\":\"f315fea0-50fc-11e4-b6fc-f23c9150064b\",
  //                      \"bssid\":\"C8:93:46:40:9B:F5\",
  //                      \"device_token\":\"e52be374c47c391414ab68c971954d13\",
  //                      \"user_token\":\"222222222\"}";
  char *json_str = NULL;
  size_t json_str_len = 0;
  json_object *object;

  object = json_object_new_object();
  require_action(object, exit, err = kNoMemoryErr);
  json_object_object_add(object, "product_id", json_object_new_string(product_id)); 
  json_object_object_add(object, "bssid", json_object_new_string(bssid)); 
  json_object_object_add(object, "device_token", json_object_new_string(device_token)); 
  json_object_object_add(object, "user_token", json_object_new_string(user_token));
  
  json_str = (char*)json_object_to_json_string(object);
  json_str_len = strlen(json_str);
  
  //mico_cloud_service_log("json_str[%d]=%s", json_str_len, json_str);
  
  httpHeader = HTTPHeaderCreate();
  require_action( httpHeader, exit, err = kNoMemoryErr );
  HTTPHeaderClear( httpHeader );
  
  while(!isDone){
    if(remoteTcpClient_fd == -1) {
      mico_cloud_service_log("tcp client start to connect...");
      while(1){
        //err = gethostbyname((char *)Context->flashContentInRam.appConfig.remoteServerDomain, (uint8_t *)ipstr, 16);
        err = gethostbyname((char *)cloudServiceContext.service_config_info.host, (uint8_t *)ipstr, 16);
        require_noerr(err, RetryGethostbyname);
        break;
        
      RetryGethostbyname:
        mico_cloud_service_log("gethostbyname err=%d, retry after 3 seconds...", err);
        mico_thread_sleep(3);
      }
      mico_cloud_service_log("cloud service host:%s, ip: %s", cloudServiceContext.service_config_info.host, ipstr);
      
      /* connect to cloud server */
      remoteTcpClient_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      require(remoteTcpClient_fd != -1, ReConnWithDelay);
      
      addr.s_ip = inet_addr(ipstr); 
      //addr.s_port = inContext->flashContentInRam.appConfig.remoteServerPort;
      addr.s_port = cloudServiceContext.service_config_info.port;
      
      err = connect(remoteTcpClient_fd, &addr, sizeof(addr));
      require_noerr_quiet(err, ReConnWithDelay);
      
      //inContext->appStatus.isRemoteConnected = true;
      mico_cloud_service_log("cloud server connected at port=%d, fd=%d", 
                             cloudServiceContext.service_config_info.port,
                             remoteTcpClient_fd);
    }
    else {
      /* send request data */
      mico_cloud_service_log("tcp client send activate request...");
      err = CreateHTTPMessageEx(kHTTPPostMethod, 
                                host, request_url,
                                kMIMEType_JSON, 
                                (uint8_t*)json_str, json_str_len,
                                &httpRequestData, &httpRequestDataLen);
      require_noerr( err, ReConnWithDelay );
      mico_cloud_service_log("send http package: len=%d,\r\n%s", httpRequestDataLen, httpRequestData);

      err = SocketSend( remoteTcpClient_fd, httpRequestData, httpRequestDataLen );
      if (httpRequestData != NULL) {
        free(httpRequestData);
        httpRequestDataLen = 0;
      }
      require_noerr( err, ReConnWithDelay );

      /* get http response */
      FD_ZERO(&readfds);
      FD_SET(remoteTcpClient_fd, &readfds);
      err = select(1, &readfds, NULL, NULL, &t);
      require(err >= 1, ReConnWithDelay);
      mico_cloud_service_log("select return ok.");

      if (FD_ISSET(remoteTcpClient_fd, &readfds)) {
        err = SocketReadHTTPHeader( remoteTcpClient_fd, httpHeader );             
        switch ( err )
        {
        case kNoErr:
          mico_cloud_service_log("read httpheader OK!");
          mico_cloud_service_log("httpHeader->buf:\r\n%s", httpHeader->buf);
          
          // Read the rest of the HTTP body if necessary
          err = SocketReadHTTPBody( remoteTcpClient_fd, httpHeader );
          require_noerr(err, ReConnWithDelay);
          mico_cloud_service_log("read httpBody OK!");
          mico_cloud_service_log("httpHeader->buf:\r\n%s", httpHeader->buf);
          
          // parse recived extra data to get devicd_id && master_device_key.
          err = _parseResponseMessage( remoteTcpClient_fd, httpHeader, out_device_id, out_master_device_key );
          HTTPHeaderClear(httpHeader);  // Reuse HTTPHeader
          require_noerr( err, ReConnWithDelay );
            
          isDone = true;
          mico_cloud_service_log("device activated done!");
          goto exit_success;
          break;
          
        case EWOULDBLOCK:
          mico_cloud_service_log("ERROR: read blocked!");
          // NO-OP, keep reading
          goto ReConnWithDelay;
          break;
          
        case kNoSpaceErr:
          mico_cloud_service_log("ERROR: Cannot fit HTTPHeader.");
          goto ReConnWithDelay;
          break;
          
        case kConnectionErr:
          // NOTE: kConnectionErr from SocketReadHTTPHeader means it's closed
          mico_cloud_service_log("ERROR: Connection closed.");
          goto ReConnWithDelay;
          break;
          
        default:
          mico_cloud_service_log("ERROR: HTTP Header parse internal error: %d", err);
          goto ReConnWithDelay; 
        }    
      }
    }
    continue;
    
  ReConnWithDelay:
    mico_cloud_service_log("Retry to activate after 3 seconds...");
    HTTPHeaderClear( httpHeader );
    if(remoteTcpClient_fd != -1){
      close(remoteTcpClient_fd);
      remoteTcpClient_fd = -1;
    }
    mico_thread_sleep(3);
  }
  
exit_success:
  if(httpHeader) free(httpHeader);
  if(remoteTcpClient_fd != -1){
      close(remoteTcpClient_fd);
      remoteTcpClient_fd = -1;
    }
//  if(NULL != object){
//    json_object_put(object);
//    object = NULL;
//  }
  if (NULL != json_str) {
    free(json_str);
    json_str = NULL;
  }
  return err;
  
exit:
  mico_cloud_service_log("Exit: cloud tcp client exit with err = %d", err);
  if(httpHeader) free(httpHeader);
  if (NULL != json_str) {
    free(json_str);
    json_str = NULL;
  }
  mico_rtos_delete_thread(NULL);
  return err;
}

//static OSStatus _device_authorize(char *product_id, char *bssid, char *device_token, char *user_token, char out_device_id[MAX_DEVICE_ID_STRLEN])
//{
//  OSStatus err = kUnknowErr;
//  if (isAuthorized)
//    return kNoErr;
//    
//  mico_cloud_service_log("device authorized start...");
//  // send http request for user authorize
//  isAuthorized = true;
//  mico_cloud_service_log("device authorized done.");
//
//  return kNoErr;
//}

static OSStatus _parseResponseMessage(int fd, HTTPHeader_t* inHeader, char out_device_id[MAX_DEVICE_ID_STRLEN], char out_master_device_key[MAX_DEVICE_KEY_STRLEN])
{
    OSStatus err = kUnknownErr;
    const char *        value;
    size_t              valueSize;

    mico_cloud_service_log_trace();

    switch(inHeader->statusCode){
      case kStatusOK:
        mico_cloud_service_log("cloud server respond activate status OK!");
        //get content-type for json format data
        err = HTTPGetHeaderField( inHeader->buf, inHeader->len, "Content-Type", NULL, NULL, &value, &valueSize, NULL );
        require_noerr(err, exit);
          
        if( strnicmpx( value, strlen(kMIMEType_JSON), kMIMEType_JSON ) == 0 ){
          mico_cloud_service_log("JSON data received!");
          // parse json data
          err = _configIncommingJsonMessage( inHeader->extraDataPtr, inHeader->extraDataLen, out_device_id, out_master_device_key);
          require_noerr( err, exit );
          return kNoErr;
        }
        else{
          return kUnsupportedDataErr;
        }
      break;
      
      default:
        goto exit;
    }

 exit:
    return err;
}

//static OSStatus _getHttpChunkedJsonData( const char *input, char **outHttpJsonData, int *outDataLen)
//{
//  int nBytes;
//  char* pStart = (char*)input;    // input中存放待解码的数据
//  char* pTemp;
//  char strlength[10];   //一个chunk块的长度
//  int length = 0;
//  
//chunk:
//  pTemp = strstr(pStart,"\r\n");
//  if(NULL == pTemp){
//    return kParamErr;
//  }
//  length = pTemp - pStart;
//  memset(strlength, 0, sizeof(strlength));
//  strncpy(strlength, pStart, length);
//  pStart = pTemp + 2;  //跳过\r\n
//  nBytes = strtol(strlength, NULL, 16); //得到一个块的长度，并转化为十进制
//  *outDataLen += nBytes;
//  
//  if(nBytes == 0)//如果长度为0表明为最后一个chunk
//  {   
//    return kNoErr;         
//  }
//  if (NULL != *outHttpJsonData){
//    *outHttpJsonData = (char *)realloc(*outHttpJsonData, nBytes);
//  }
//  else { 
//    *outHttpJsonData = (char *)malloc(nBytes);  //don't forget to free *outHttpJsonData
//    memset(*outHttpJsonData, 0, nBytes);
//  }
//  strncat(*outHttpJsonData, pStart, nBytes);  //将nBytes长度的数据追加到返回字符串末尾
//  //mico_cloud_service_log("_getHttpChunkedJsonData[%d]=%s", nBytes, *outHttpJsonData);
//  pStart = pStart + nBytes + 2;  //跳过一个块的数据以及数据之后两个字节的结束符        
//  goto chunk;  //goto到chunk继续处理 
//}

static OSStatus _configIncommingJsonMessage( const char *input , unsigned int len, char out_device_id[MAX_DEVICE_ID_STRLEN], char out_master_device_key[MAX_DEVICE_KEY_STRLEN])
{
  mico_cloud_service_log_trace();
  OSStatus err = kUnknownErr;
  json_object *new_obj = NULL;

  mico_cloud_service_log("Recv json data input=%s", input);
  new_obj = json_tokener_parse(input);
  mico_cloud_service_log("Recv json data=%s", json_object_to_json_string(new_obj));
  //new_obj = json_tokener_parse("{\"device_id\":\"123456\", \"master_device_key\": \"88888888\"}");
  require_action(new_obj, exit, err = kUnknownErr);
  mico_cloud_service_log("Recv json config object=%s", json_object_to_json_string(new_obj));

  //save each key-value in json data for system.
  json_object_object_foreach(new_obj, key, val) {
    if(!strcmp(key, "device_id")){
      memset((char *)out_device_id, 0, strlen(out_device_id));
      strncpy((char*)out_device_id, json_object_get_string(val), MAX_DEVICE_ID_STRLEN);
      mico_cloud_service_log("get out_device_id[%d]=%s", strlen(out_device_id), out_device_id);
    }else if(!strcmp(key, "master_device_key")){
      memset((char *)out_master_device_key, 0, strlen(out_master_device_key));
      strncpy(out_master_device_key, json_object_get_string(val), MAX_DEVICE_KEY_STRLEN);
      mico_cloud_service_log("get out_master_device_key[%d]=%s", strlen(out_master_device_key), out_master_device_key);
    }
    else {
    }
  }

  //free unused memory
  json_object_put(new_obj);
  return kNoErr;

exit:
  return err; 
}

/* brief: calculate device_token = MD5(bssid + product_id)
 * input:
 *    bssid + product_key
 * output:
 *    device_token
 * return:
 *     return kNoErr if success
 */
static OSStatus _cal_device_token(char *bssid, char *product_key, char out_device_token[32])
{
  md5_context md5;
  unsigned char *md5_input = NULL;
  unsigned char device_token_16[16];
  char *ptoken32 = NULL;
  unsigned int input_len = 0;
  
  if ((NULL == bssid) || (NULL == product_key) || NULL == out_device_token)
    return kParamErr;
  memset(out_device_token, 0, strlen(out_device_token));
  
  mico_cloud_service_log("bssid[%d]=%s", strlen(bssid), bssid);
  mico_cloud_service_log("product_key[%d]=%s", strlen(product_key), product_key);

  input_len = strlen(bssid) + strlen(product_key) + 1;
  md5_input = (unsigned char *)malloc(input_len);
  if (NULL == md5_input)
    return kNoMemoryErr;
  memset(md5_input, 0, input_len);
  md5_input[input_len] = '\0';
  
  memcpy(md5_input, bssid, strlen(bssid));
  memcpy((md5_input + strlen(bssid)), product_key, strlen(product_key));
  mico_cloud_service_log("md5_input[%d]=%s", strlen((char*)md5_input), (unsigned char*)md5_input);
  
  InitMd5(&md5);
  //Md5Update(&md5, (uint8_t*)("C8:93:46:40:9B:F541a71625-5519-11e4-ad4e-f23c9150064b"), strlen("C8:93:46:40:9B:F541a71625-5519-11e4-ad4e-f23c9150064b"));
  Md5Update(&md5, md5_input, strlen((char*)md5_input));
  Md5Final(&md5, device_token_16);
  mico_cloud_service_log("device_token_16[%d]=%s", sizeof(device_token_16), device_token_16);
  //convert hex data to hex string
  ptoken32 = DataToHexStringLowercase(device_token_16,  sizeof(device_token_16));
  mico_cloud_service_log("out_device_token[%d]=%s", strlen(ptoken32), ptoken32);
  
  if (NULL != ptoken32){
    strncpy(out_device_token, ptoken32, strlen(ptoken32));
    free(ptoken32);
    ptoken32 = NULL;
  }
  else
    return kNoMemoryErr;
  
  if (NULL != md5_input){
    free(md5_input);
    md5_input = NULL;
  }
  
  mico_cloud_service_log("out_device_token[%d]=%s", strlen(out_device_token), out_device_token);
  
  return kNoErr;
}
//
///* brief: calculate user_token = MD5(bssid + login_id + login_passwd)
// * input:
// *    bssid + login_id + login_passwd
// * output:
// *    user_token
// * return:
// *     return kNoErr if success
// */
//static OSStatus _cal_user_token(const char* bssid, const char* login_id, const char * login_passwd, unsigned char out_user_token[16])
//{
//   OSStatus err = kNoErr;
//   return err;
//}


