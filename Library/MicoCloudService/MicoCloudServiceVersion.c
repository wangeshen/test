/**
******************************************************************************
* @file    MicoCloudServiceVersion.c
* @author  Eshen Wang
* @version V1.0.0
* @date    31-Oct-2014
* @brief   This file contains the release version of the cloud service library
                based on MICO platform. 
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

#include "MicoCloudService.h"

//#define  debug_out 

//#ifdef debug_out
//#define  _debug_out debug_out
//#else
#define _debug_out(format, ...) do {;}while(0)

#define mico_cloud_service_version_log(M, ...) custom_log("CloudService", M, ##__VA_ARGS__)
#define mico_cloud_service_version_log_trace() custom_log_trace("CloudService")
//#endif

/*******************************************************************************
 * DEFINES
 ******************************************************************************/
#define MICO_CLOUD_SERVCIE_VERSION        "0.1.0"


/*******************************************************************************
 * IMPLEMENTATIONS
 ******************************************************************************/

char* MicoCloudServiceVersion(void)
{
  char *version = MICO_CLOUD_SERVCIE_VERSION;

  return version;
}