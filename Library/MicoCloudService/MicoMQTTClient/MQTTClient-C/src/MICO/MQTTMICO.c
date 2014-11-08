/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "MQTTMICO.h"
#include "Common.h"

#define MQTTClient_log(M, ...) custom_log("MQTT_MICO", M, ##__VA_ARGS__)
#define MQTTClient_log_trace() custom_log_trace("MQTT_MICO")

unsigned long MilliTimer;
mico_timer_t MQTTClientSystick1ms;

void SysTickIntHandler(void) {
  MilliTimer++;
}

char expired(Timer* timer) {
  long left = timer->end_time - MilliTimer;
  return (left < 0);
}


void countdown_ms(Timer* timer, unsigned int timeout) {
  timer->end_time = MilliTimer + timeout;
}


void countdown(Timer* timer, unsigned int timeout) {
  timer->end_time = MilliTimer + (timeout * 1000);
}


int left_ms(Timer* timer) {
  long left = timer->end_time - MilliTimer;
  return (left < 0) ? 0 : left;
}


void InitTimer(Timer* timer) {
  timer->end_time = 0;
}


int MICO_read(Network* n, unsigned char* buffer, int len, int timeout_ms) {
  struct timeval_t timeVal;
  fd_set fdset;
  int rc = 0;
  int recvLen = 0;
  Timer readLenTimer;
  int socket_errno = 0;
  int socket_errno_len = 0;
  
  FD_ZERO(&fdset);
  FD_SET(n->my_socket, &fdset);
  
  timeVal.tv_sec = 0;
  timeVal.tv_usec = timeout_ms * 1000;
  
  //read left timer
  InitTimer(&readLenTimer);
  countdown_ms(&readLenTimer, timeout_ms);
  
  if (select(n->my_socket + 1, &fdset, NULL, NULL, &timeVal) == 1) {
    // by wes, 20141021, must be non blocking read.
    do {
      rc = recv(n->my_socket, buffer + recvLen, len - recvLen, 0);
      if(rc <= 0){
        rc = getsockopt(n->my_socket, SOL_SOCKET, SO_ERROR, &socket_errno, &socket_errno_len);
        if ((rc < 0) || ( 0 != socket_errno))
          return -1;   //socket error
      }
      else
        recvLen += rc;
    } while((recvLen < len) && (!expired(&readLenTimer))); // by wes, 20141021, must be non blocking read.
  }
  return recvLen;
}


int MICO_write(Network* n, unsigned char* buffer, int len, int timeout_ms) {
  struct timeval_t timeVal;
  fd_set fdset;
  int rc = 0;
  int readySock;
  
  //add timeout by wes 20141106
  Timer writeTimer;
  
  InitTimer(&writeTimer);
  countdown_ms(&writeTimer, timeout_ms);
  
  FD_ZERO(&fdset);
  FD_SET(n->my_socket, &fdset);
  
  timeVal.tv_sec = 0;
  timeVal.tv_usec = timeout_ms * 1000;
  do {
    readySock = select(n->my_socket + 1, NULL, &fdset, NULL, &timeVal);
  } while((readySock != 1) && (!expired(&writeTimer)));  //add timeout by wes 20141106
  rc = send(n->my_socket, buffer, len, 0);
  return rc;
}


void MICO_disconnect(Network* n) {
  close(n->my_socket);
}


void NewNetwork(Network* n) {
  n->my_socket = 0;
  n->mqttread = MICO_read;
  n->mqttwrite = MICO_write;
  n->disconnect = MICO_disconnect;
}


int ConnectNetwork(Network* n, char* addr, int port)
{
  struct sockaddr_t sAddr;
  int addrSize;
  unsigned long ipAddress;
  char ipstr_mqtt[16];
  int retVal = -1;
  OSStatus err = kUnknownErr;
  
  int nNetTimeout = 1000;//1Ãë£¬
  
  MQTTClient_log("connect to MQTT server %s:%d", addr, port);
  
  //******************************* ??? ************************************
  // gethostbyname in different thread(NTPClient) ???
  //NetAppDnsGetHostByName(addr, strlen(addr), &ipAddress, AF_INET);
  
  while(1){
    memset(ipstr_mqtt, 0, sizeof(ipstr_mqtt));
    err = gethostbyname(addr, (uint8_t *)ipstr_mqtt, 16); // may be failed ???
    if(kNoErr == err){
      MQTTClient_log("MQTT server address: %s",ipstr_mqtt);
      break;
    }
    
    MQTTClient_log("retry: get MQTT host address after 3 seconds...");
    mico_thread_sleep(3);
  }
  //*******************************************************************
  
  ipAddress = inet_addr(ipstr_mqtt);
  //ipAddress = inet_addr("198.41.30.241");
  MQTTClient_log("MQTT gethostbyname: addr=%s ipStr=%s ip=%d port=%d", 
                 addr, ipstr_mqtt, ipAddress, (unsigned short)port);
  
  sAddr.s_port = (unsigned short)port;
  sAddr.s_ip = ipAddress;
  addrSize = sizeof(sAddr);
  
  n->my_socket = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
  if( n->my_socket < 0 ) {
    // error
    MQTTClient_log("socket error!");
    return -1;
  }
  
  retVal = setsockopt(n->my_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&nNetTimeout,sizeof(int));
  if( retVal < 0 ) {
    // error
    MQTTClient_log("setsockopt error:[%d]", retVal);
    close(n->my_socket);
    return retVal;
  }
  
  retVal = setsockopt(n->my_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&nNetTimeout,sizeof(int));
  if( retVal < 0 ) {
    // error
    MQTTClient_log("setsockopt error:[%d]", retVal);
    close(n->my_socket);
    return retVal;
  }
  
  retVal = connect(n->my_socket, &sAddr, addrSize);
  if( retVal < 0 ) {
    // error
    MQTTClient_log("connect error:[%d]", retVal);
    close(n->my_socket);
    return retVal;
  }
  
  // SysTickIntRegister(SysTickIntHandler);
  // SysTickPeriodSet(80000);
  // SysTickEnable();
  MQTTClient_log("start systick timer for MQTTClient");
  
  if (mico_is_timer_running(&MQTTClientSystick1ms)){
    mico_stop_timer(&MQTTClientSystick1ms);
    mico_deinit_timer(&MQTTClientSystick1ms);
  }
  mico_init_timer(&MQTTClientSystick1ms,1,(timer_handler_t)SysTickIntHandler,0);
  mico_start_timer(&MQTTClientSystick1ms);
  
  return retVal;
}
