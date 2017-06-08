/*
Copyright (c) 2016 Intrepid Control Systems, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ics/icsneo40API.h>

static void* hObject = 0;
static bool bShutDown;

static pthread_mutexattr_t cs_mutex_attr; 	
static pthread_mutex_t cs_mutex;  

void *ReadThread(void *lParam)
{
    int NumErrors = 0;
    int NumMsgs = 0;
    icsSpyMessage Msgs[20000];
    bool bDone = false;
  
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    while(1)
    {	
       //check for shutdown from the main thread
        pthread_mutex_lock(&cs_mutex); 
          bDone = bShutDown;
       pthread_mutex_unlock(&cs_mutex); 
       
          if(bDone)   
             break;
                 
        if(icsneoWaitForRxMessagesWithTimeOut(hObject, 100) == 0)
            continue;
            
        if(icsneoGetMessages(hObject, Msgs, &NumMsgs, &NumErrors) == 0)
            continue;     	
       
         for(int i = 0; i < NumMsgs; i++)
        {
            double time;

            icsneoGetTimeStampForMsg(hObject, &Msgs[i], &time);
            printf("Time %f Network %d ArbID = %X - Data Bytes: ", time, Msgs[i].NetworkID, Msgs[i].ArbIDOrHeader);
            
            for(int j = 0; j < Msgs[i].NumberBytesData; j++)
                printf("%02X ", Msgs[i].Data[j]);

            printf("\n");           
         }  	
    }	
        
      printf("ReadThread done\n");
 
    return 0;
}

int main(int argc, char** argv)
{
    NeoDevice Nd[255];
    int iRetVal = 0;
    int i, index_to_open = -1;
    int NumDevices = 255;
    int NumErrors;
    pthread_t thread;
    bool bExit = false;
    icsSpyMessage OutMsg;
    char DeviceType[25];
    char chIn;
    unsigned long serial_to_open = 0;

    if(argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) 
    {
        printf("icsneo sample application\n-s, --serial\t open a specific serial number\n-h, --help\tprint this message\n");
        return 0;
    }

    if(argc >= 3 && (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "--serial") == 0))
        icsneoSerialNumberFromString(&serial_to_open, argv[2]);

    
    pthread_mutexattr_init(&cs_mutex_attr); 	
    pthread_mutexattr_settype(&cs_mutex_attr, PTHREAD_MUTEX_RECURSIVE_NP); 
    pthread_mutex_init(&cs_mutex, &cs_mutex_attr); 

    bShutDown = false;

    printf("Starting......\n");

    iRetVal = icsneoFindNeoDevices(NEODEVICE_ALL & ~NEODEVICE_RADGALAXY, Nd, &NumDevices);
        
    if(iRetVal == 0 || NumDevices == 0)
    {
        printf("No devices found...exiting with ret = %d\n",iRetVal);
        return 0;		
    }

    printf("%d Devices found\n", NumDevices);

    for(i = 0; i < NumDevices; i++)
    {
        switch(Nd[i].DeviceType)
        {
            case NEODEVICE_VCAN3:
                strcpy(DeviceType, "ValueCAN3");
                break;

            case NEODEVICE_FIRE:
                strcpy(DeviceType, "neoVI FIRE");
                break;

            case NEODEVICE_PLASMA_1_11:
            case NEODEVICE_PLASMA_1_12: 
            case NEODEVICE_PLASMA_1_13:
                strcpy(DeviceType, "neoVI PLASMA");
                break;

            case NEODEVICE_ION_2:
		    case NEODEVICE_ION_3:
                strcpy(DeviceType, "neoVI ION");
                break;

            case NEODEVICE_FIRE2:
                strcpy(DeviceType, "neoVI FIRE2");
                break;

            case NEODEVICE_VCANRF:
                strcpy(DeviceType, "VCAN.rf");
                break;
                
            default:
                strcpy(DeviceType, "Other device");                
                break;
        }		

        if(Nd[i].SerialNumber == serial_to_open)
            index_to_open = i;

        char serialStr[64];
        icsneoSerialNumberToString(Nd[i].SerialNumber, serialStr, sizeof(serialStr));
        printf("Device %d: ", i + 1);
        printf("Serial # %s Type = %s\n", serialStr, DeviceType); 
    }
    
    if(serial_to_open == 0)
        index_to_open = 0;
    else if(index_to_open == -1)
    {
        printf("\nUnable to find device with serial # %s\n", argv[2]);
        return 0;
    }

    printf("\nOpening device %i\n", index_to_open + 1);

    iRetVal = icsneoOpenNeoDevice(&Nd[index_to_open], &hObject, NULL, 1, 0/*DEVICE_OPTION_DONT_ENABLE_NETCOMS*/);

    if(iRetVal == 1)
    {
        printf("Device was opened!\n\n");
    }
    else
    {
        printf("Failed to open the device\n");
        icsneoShutdownAPI();	
        return 0;    	
    }     

    pthread_t threadid;    
    pthread_create(&threadid,NULL,&ReadThread,NULL); 

    while(!bExit)
    {
        printf("enter t to transmit\n");
        printf("enter e to exit\n");
        chIn = getc(stdin);              
        
        switch(chIn)
        {            
            case 't':
            {           
                icsSpyMessage OutMsg = {0};
                OutMsg.ArbIDOrHeader = 0x500;
                OutMsg.Data[0] = Nd[index_to_open].SerialNumber & 0xff;
                OutMsg.Data[1] = (Nd[index_to_open].SerialNumber >> 8) & 0xff;
                OutMsg.Data[2] = (Nd[index_to_open].SerialNumber >> 16);
                OutMsg.Data[3] = (Nd[index_to_open].SerialNumber >> 24);
                OutMsg.Data[4] = 0xde;
                OutMsg.Data[5] = 0xad;
                OutMsg.Data[6] = 0xbe;
                OutMsg.Data[7] = 0xef;
                OutMsg.NumberBytesData = 8;
                OutMsg.NumberBytesHeader = 2;      
                iRetVal = icsneoTxMessages(hObject, &OutMsg, NETID_HSCAN, 1);
            }   break;    		

            case 'e':
                bExit = true;
                break;		    		
        }    
    }

    printf("Ending......\n");

    pthread_mutex_lock(&cs_mutex); 
    bShutDown = true;
    pthread_mutex_unlock(&cs_mutex);        
    usleep(1000 * 1000); //let the read thread cdlose

    icsneoClosePort(hObject, &NumErrors);
    icsneoFreeObject(hObject);	
    icsneoShutdownAPI();   
            
    printf("Main thread done....\n");

    pthread_mutex_destroy(&cs_mutex); 
    pthread_mutexattr_destroy(&cs_mutex_attr); 	
        
    return 0;
}

