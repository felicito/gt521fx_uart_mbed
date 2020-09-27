/**
 * @section LICENSE
 *
 * Copyright (c) 2013 @tosihisa, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * Fingerprint reader module "GT-511C3" class.
 *
 * http://www.adh-tech.com.tw/?22,gt-511c3-gt-511c31
 * http://www.adh-tech.com.tw/files/GT-511C3_datasheet_V1%201_20131127[1].pdf
 * https://www.sparkfun.com/products/11792
 * https://github.com/sparkfun/Fingerprint_Scanner-TTL/
 */
 
#include "mbed.h"
#include "GT521FX.hpp"
#include "BufferedSerial.h"
 
#define SET_AND_SUMADD(idx,val) sendbuf[idx]=(( char)(val));sum += sendbuf[idx]
 
int GT521FX::Init(void)
{
    baud(9600);
    ClearLine();
    return 0;
}
 
int GT521FX::SendCommand( long Parameter, short Command)
{
    char sendbuf[12];
    short sum = 0;
    int idx = 0;
    int i;
 
    SET_AND_SUMADD(idx,0x55);
    idx++;
    SET_AND_SUMADD(idx,0xAA);
    idx++;
    SET_AND_SUMADD(idx,0x01);
    idx++;
    SET_AND_SUMADD(idx,0x00);
    idx++;
    SET_AND_SUMADD(idx,Parameter & 0xff);
    idx++;
    SET_AND_SUMADD(idx,(Parameter >> 8) & 0xff);
    idx++;
    SET_AND_SUMADD(idx,(Parameter >> 16) & 0xff);
    idx++;
    SET_AND_SUMADD(idx,(Parameter >> 24) & 0xff);
    idx++;
    SET_AND_SUMADD(idx,Command & 0xff);
    idx++;
    SET_AND_SUMADD(idx,(Command >> 8) & 0xff);
    idx++;
    sendbuf[idx] = sum & 0xff;
    idx++;
    sendbuf[idx] = (sum >> 8) & 0xff;
    idx++;
 
    for(i = 0; i < idx; i++) {
        while(!writeable());
        putc(sendbuf[i]);
    }
    return 0;
}
 
int GT521FX::RecvResponse( long *Parameter, short *Response)
{
    const  char fixedbuf[4] = { 0x55,0xAA,0x01,0x00 };
     char buf[12];
     short sum = 0;
    int i;
 
    *Parameter = 0;
    *Response = CMD_Nack;
 
    for(i = 0; i < sizeof(buf); i++) {
        while(!readable());
        buf[i] = getc();
        if(i < 9) {
            sum += buf[i];
        }
        if(i < 4) {
            if(buf[i] != fixedbuf[i]) {
                return -1;
            }
        }
    }
    if(buf[10] != (sum & 0xff))
        return -2;
    if(buf[11] != ((sum >> 8) & 0xff))
        return -2;
 
    *Parameter =  buf[7];
    *Parameter = (*Parameter << 8) | buf[6];
    *Parameter = (*Parameter << 8) | buf[5];
    *Parameter = (*Parameter << 8) | buf[4];
 
    *Response = buf[9];
    *Response = (*Response << 8) | buf[8];
 
    return 0;
}
 
int GT521FX::SendData( char *data, long size)
{
    const  char fixedbuf[4] = { 0x5A,0xA5,0x01,0x00 };
     short sum = 0;
    int i;
 
    for(i = 0;i < 4;i++){
        while(!writeable());
        putc(fixedbuf[i]);
        sum += fixedbuf[i];
    }    
    
    for(i = 0;i < size;i++){
        while(!writeable());
        putc(data[i]);
        sum += data[i];
    }
    
    while(!writeable());
    putc(( char)(sum & 0xff));
    while(!writeable());
    putc(( char)((sum >> 8) & 0xff));
    
    return 0;
}
 
int GT521FX::RecvData( char *data, long size)
{
    const  char fixedbuf[4] = { 0x5A,0xA5,0x01,0x00 };
     short sum = 0;
    int i;
 
    for(i = 0; i < size; i++) {
        while(!readable());
        *(data + i) = getc();
        if(i < (size-2)) {
            sum += *(data + i);
        }
        if(i < 4) {
            if(*(data + i) != fixedbuf[i]) {
                return -1;
            }
        }
    }
    if(*(data + size - 2) != (sum & 0xff))
        return -2;
    if(*(data + size - 1) != ((sum >> 8) & 0xff))
        return -2;
    return 0;
}
 
int GT521FX::SendRecv( short Command, long *Parameter, short *Response)
{
    int sts;
    if((sts = SendCommand(*Parameter,Command)) == 0) {
        *Parameter = 0;
        if((sts = RecvResponse(Parameter,Response)) != 0) {
            *Response = CMD_Nack;
            *Parameter = NACK_IO_ERR;
        }
    }
    if(*Response == CMD_Nack) {
        LastError = *Parameter;
    }
    return sts;
}
 
int GT521FX::ClearLine(void)
{
    while(readable()) {
        (void)getc();
    }
    return 0;
}
 
int GT521FX::Open(void)
{
     long Parameter = 1;
     short Response = 0;
     char buf[4+sizeof(FirmwareVersion)+sizeof(IsoAreaMaxSize)+sizeof(DeviceSerialNumber)+2];
    int sts = 0;
 
    if((sts = Init()) != 0)
        return -1;
 
    sts = SendRecv(CMD_Open,&Parameter,&Response);
    if((sts != 0) || (Response != CMD_Ack)) {
        return -1;
    }
    if((sts = RecvData(buf,sizeof(buf))) == 0) {
        memcpy(&FirmwareVersion,&buf[4+0],sizeof(FirmwareVersion));
        memcpy(&IsoAreaMaxSize,&buf[4+sizeof(FirmwareVersion)],sizeof(IsoAreaMaxSize));
        memcpy(DeviceSerialNumber,&buf[4+sizeof(FirmwareVersion)+sizeof(IsoAreaMaxSize)],sizeof(DeviceSerialNumber));
    }
    return sts;
}
 
int GT521FX::WaitPress(int press)
{
    while(IsPress() != press);
    return 0;
}
 
int GT521FX::CmosLed(int onoff)
{
     long Parameter = onoff & 1;
     short Response = 0;
    int sts = 0;
 
    sts = SendRecv(CMD_CmosLed,&Parameter,&Response);
    if((sts != 0) || (Response != CMD_Ack)) {
        return -1;
    }
    return 0;
}
 
int GT521FX::IsPress(void)
{
     long Parameter = 0;
     short Response = 0;
    int sts = 0;
    sts = SendRecv(CMD_IsPressFinger,&Parameter,&Response);
    if((sts != 0) || (Response != CMD_Ack))
        return 0;
    if(Parameter != 0)
        return 0;
    return 1;
}
 
int GT521FX::Capture(int best)
{
     long Parameter = best;
     short Response = 0;
    int sts = 0;
 
    sts = SendRecv(CMD_CaptureFinger,&Parameter,&Response);
    if((sts != 0) || (Response != CMD_Ack))
        return -1;
    return 0;
}
 
int GT521FX::Enroll_N(int N)
{
     long Parameter = 0;
     short Response = 0;
    int sts = 0;
    enum Command cmd;
 
    switch(N) {
        default:
        case 1:
            cmd = CMD_Enroll1;
            break;
        case 2:
            cmd = CMD_Enroll2;
            break;
        case 3:
            cmd = CMD_Enroll3;
            break;
    }
    sts = SendRecv(cmd,&Parameter,&Response);
    if((sts != 0) || (Response != CMD_Ack))
        return -1;
    return 0;
}
 
int GT521FX::Identify(void)
{
     long Parameter = 0;
     short Response = 0;
    int sts = 0;
 
    sts = SendRecv(CMD_Identify,&Parameter,&Response);
    if((sts != 0) || (Response != CMD_Ack))
        return -1;
    return Parameter;
}
 
int GT521FX::Enroll(int ID,int (*progress)(int status,char *msg))
{
     long Parameter = 0;
     short Response = 0;
    int sts = 0;
 
    CmosLed(1);
 
    while(1) {
        if((sts = (*progress)(1,"EnrollStart\n")) != 0)
            return -9999;
        Parameter = ID;
        sts = SendRecv(CMD_EnrollStart,&Parameter,&Response);
        if(sts != 0)
            return sts;
        if(Response != CMD_Ack)
            return -100;
 
        if((sts = (*progress)(0,"Remove finger\n")) != 0)
            return -9999;
        WaitPress(0);
 
        while(1) {
            if((sts = (*progress)(10,"Press finger to Enroll (1st)\n")) != 0)
                return -9999;
            WaitPress(1);
            if(Capture(1) == 0)
                break;
        }
 
        if((sts = (*progress)(0,"Remove finger\n")) != 0)
            return -9999;
        if(Enroll_N(1) != 0)
            continue;
        WaitPress(0);
 
        while(1) {
            if((sts = (*progress)(20,"Press finger to Enroll (2nd)\n")) != 0)
                return -9999;
            WaitPress(1);
            if(Capture(1) == 0)
                break;
        }
 
        if((sts = (*progress)(0,"Remove finger\n")) != 0)
            return -9999;
        if(Enroll_N(2) != 0)
            continue;
        WaitPress(0);
 
        while(1) {
            if((sts = (*progress)(30,"Press finger to Enroll (3rd)\n")) != 0)
                return -9999;
            WaitPress(1);
            if(Capture(1) == 0)
                break;
        }
 
        if((sts = (*progress)(0,"Remove finger\n")) != 0)
            return -9999;
        if(Enroll_N(3) != 0)
            continue;
        WaitPress(0);
 
        if((sts = (*progress)(100,"Enroll OK\n")) != 0)
            return -9999;
 
        break;
    }
    return 0;
}
 
int GT521FX::CheckEnrolled(int ID)
{
     long Parameter = ID;
     short Response = 0;
    int sts = 0;
 
    sts = SendRecv(CMD_CheckEnrolled,&Parameter,&Response);
    if((sts == 0) && (Response == CMD_Ack))
        return 0;  //This ID is enrolled
    return -1;
}
 
 
int GT521FX::SetTemplate(int ID,  char *data,  long size)
{
    long Parameter = ID;
    short Response = 0;
    int sts = 0;
 
    sts = SendRecv(CMD_SetTemplate,&Parameter,&Response);
    
    if ((sts != 0) || (Response != CMD_Ack))
        return -1;
    
    sts = SendData(data, size);
    
    if (sts != 0)
        return -2;
        
    sts = RecvResponse(&Parameter, &Response);
    
    if ((sts != 0) || (Response != CMD_Ack))
        return Response; // -3;
    
    return 0;
}
 
int GT521FX::DeleteID(int ID)
{
     long Parameter = ID;
     short Response = 0;
    int sts = 0;
 
    sts = SendRecv(CMD_DeleteID,&Parameter,&Response);
    if((sts == 0) && (Response == CMD_Ack))
        return 0;
    return -1;
}
 
int GT521FX::DeleteAllIDs()
{
     long Parameter = 0;
     short Response = 0;
    int sts = 0;
 
    sts = SendRecv(CMD_DeleteAll,&Parameter,&Response);
    if((sts == 0) && (Response == CMD_Ack))
        return 0;
    return -1;
}