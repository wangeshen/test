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

#ifndef __MQTT_MICO_H_
#define __MQTT_MICO_H_

#include "MICO.h"

#define MQTTCLIENT_INFO "MQTT_MICO_CLIENT"

typedef struct Timer Timer;

struct Timer {
  unsigned long systick_period;
  unsigned long end_time;
};

typedef struct Network Network;

struct Network
{
  int my_socket;
  int (*mqttread) (Network*, unsigned char*, int, int);
  int (*mqttwrite) (Network*, unsigned char*, int, int);
  void (*disconnect) (Network*);
};

char expired(Timer*);
void countdown_ms(Timer*, unsigned int);
void countdown(Timer*, unsigned int);
int left_ms(Timer*);

void InitTimer(Timer*);

int MICO_read(Network*, unsigned char*, int, int);
int MICO_write(Network*, unsigned char*, int, int);
void MICO_disconnect(Network*);
void NewNetwork(Network*);

int ConnectNetwork(Network*, char*, int);
//int TLSConnectNetwork(Network*, char*, int, SlSockSecureFiles_t*, unsigned char, unsigned int, char);

#endif
