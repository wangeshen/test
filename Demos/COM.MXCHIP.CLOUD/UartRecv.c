
#include "MICODefine.h"
#include "MICOAppDefine.h"

#include "haProtocol.h"
#include "MicoPlatform.h"
#include "platform.h"
#include "MICONotificationCenter.h"

#define uart_recv_log(M, ...) custom_log("UART RECV", M, ##__VA_ARGS__)
#define uart_recv_log_trace() custom_log_trace("UART RECV")

static int _uart_get_one_packet(uint8_t* buf, int maxlen);

void uartRecv_thread(void *inContext)
{
  uart_recv_log_trace();
  mico_Context_t *Context = inContext;
  int recvlen;
  uint8_t *inDataBuffer;
  
  inDataBuffer = malloc(UART_ONE_PACKAGE_LENGTH);
  require(inDataBuffer, exit);
  
  while(1) {
    memset(inDataBuffer, 0, UART_ONE_PACKAGE_LENGTH);
    recvlen = _uart_get_one_packet(inDataBuffer, UART_ONE_PACKAGE_LENGTH);
    if (recvlen <= 0)
      continue;
    
    haUartCommandProcess(inDataBuffer, recvlen, Context);
  }
  
exit:
  if(inDataBuffer) free(inDataBuffer);
}

/* Packet format: BB 00 CMD(2B) Status(2B) datalen(2B) data(x) checksum(2B)
* copy to buf, return len = datalen+10
*/
int _uart_get_one_packet(uint8_t* inBuf, int inBufLen)
{
  uart_recv_log_trace();
  int datalen;
    
  while(1) {
    if( MicoUartRecv( UART_FOR_APP, inBuf, inBufLen, UART_RECV_TIMEOUT) == kNoErr){
      return inBufLen;
    }
    else{
      datalen = MicoUartGetLengthInBuffer( UART_FOR_APP );
      if(datalen){
        MicoUartRecv(UART_FOR_APP, inBuf, datalen, UART_RECV_TIMEOUT);
        return datalen;
      }
    }
  }
}
