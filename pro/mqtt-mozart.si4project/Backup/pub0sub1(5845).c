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
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Sergio R. Caprile - clarifications and/or documentation extension
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "MQTTPacket.h"
#include "transport.h"


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <pthread.h>

int recv_count = 0;

struct msg_st
{
	long int msg_type;
	int flag;
	char text[4096];
};

#define KEEPALIVE_INTERVAL 20

/* This is to get a timebase in seconds to test the sample */
#include <time.h>
time_t old_t;
void start_ping_timer(void)
{
	time(&old_t);
	old_t += KEEPALIVE_INTERVAL/2 + 1;
}

int time_to_ping(void)
{
time_t t;

	time(&t);
	if(t >= old_t)
	  	return 1;
	return 0;
}


/* This is in order to get an asynchronous signal to stop the sample,
as the code loops waiting for msgs on the subscribed topic.
Your actual code will depend on your hw and approach*/
#include <signal.h>

int toStop = 0;

void cfinish(int sig)
{
	signal(SIGINT, NULL);
	toStop = 1;
}

void stop_init(void)
{
	signal(SIGINT, cfinish);
	signal(SIGTERM, cfinish);
}

//get wifi_mac
void get_mac(char *Str)
{
	char *dest = Str;
	while(*Str != '\0')
	{
		if(*Str != ':' && *Str != 0x0a)
		{
			*dest++ = *Str;
		}
		
		++Str;
	}
	*dest = '\0';
	//printf("wifi_mac:%s \n",dest);	
}


int main(int argc, char *argv[])
{

	
	//获取设备id
	int w_fd;
	char s_buf[25] = {0};
	char deviceID[30] = "SZH1-BINGOV1-";
	//char clientID[30] = {0};
	
	w_fd = open("/sys/class/net/wlan0/address",O_RDONLY);

	if(w_fd < 0){
		printf("open mac file error!!\n");
		return -1;
	}
	
	read(w_fd,s_buf,25);
	get_mac(s_buf);
	close(w_fd);
	strcat(deviceID,s_buf);


	int mysock = 0;
	int payloadlen_in;
	unsigned char *payload_in;
	MQTTString topicString = MQTTString_initializer;
	
	unsigned char buf[4096] = {0};
	int buflen = sizeof(buf);
	int len = 0;


	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	int rc = 0;
	int msgid = 1;
	int req_qos = 0;
	char *host = "47.106.157.206";

	int port = 1883;
	
new_ping:	
	//system("date >> /usr/data/ping.txt");

	stop_init();
	/*
	if (argc > 1)
		host = argv[1];

	if (argc > 2)
		port = atoi(argv[2]);
	*/
	mysock = transport_open(host, port);
	if(mysock < 0)
		return mysock;

	//printf("Sending to hostname %s port %d\n", host, port);

	data.clientID.cstring = deviceID;
	data.keepAliveInterval = KEEPALIVE_INTERVAL;
	data.cleansession = 1;
	
	len = MQTTSerialize_connect(buf, buflen, &data);
	
	rc = transport_sendPacketBuffer(mysock, buf, len);

	/* wait for connack */
	int m = MQTTPacket_read(buf, buflen, transport_getdata);
	if ( m == CONNACK)
	{
		unsigned char sessionPresent, connack_rc;

		if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1 || connack_rc != 0)
		{

			printf("Unable to connect, return code %d [%s] [%d]\n", connack_rc,__func__,__LINE__);
			/*
			printf("bbbbbbbbbbbbbbbbbbbbbb\n");
			if(system("ping -c 5 www.baidu.com") != 0){
				printf("ping baidu not ok 11111111111\n");
				goto exit;
			}
			*/
			
			len = MQTTSerialize_disconnect(buf, buflen);
			rc = transport_sendPacketBuffer(mysock, buf, len);
			transport_close(mysock);
			sleep(10);
			goto new_ping;

		}
	}
	else{

		/*
		printf("ccccccccccccccccccccccc\n");
		if(system("ping -c 5 www.baidu.com") != 0){
				printf("ping baidu not ok 22222222222\n");
				goto exit;
		}
		*/
		printf("MQTTPacket_read error m[%d] [%s] [%d]\n",m,__func__,__LINE__);
		len = MQTTSerialize_disconnect(buf, buflen);
		rc = transport_sendPacketBuffer(mysock, buf, len);
		transport_close(mysock);
		sleep(10);
		goto new_ping;

	}
	/* subscribe */
	topicString.cstring = deviceID;
	len = MQTTSerialize_subscribe(buf, buflen, 0, msgid, 1, &topicString, &req_qos);
	rc = transport_sendPacketBuffer(mysock, buf, len);
	
	m = MQTTPacket_read(buf, buflen, transport_getdata);
	if (m == SUBACK) 	/* wait for suback */
	{
		unsigned short submsgid;
		int subcount;
		int granted_qos;

		rc = MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, buf, buflen);
		if (granted_qos != 0)
		{
			/*
			if(system("ping -c 5 www.baidu.com") != 0){
				printf("ping baidu not ok 33333333333\n");
				goto exit;
			}
			*/

			printf("granted qos != 0, %d\n", granted_qos);
			len = MQTTSerialize_disconnect(buf, buflen);
			rc = transport_sendPacketBuffer(mysock, buf, len);
			transport_close(mysock);
			sleep(10);
			goto new_ping;
		}
	} else {
		/*
		printf("ddddddddddddddddddddddddddd\n");

		if(system("ping -c 5 www.baidu.com") != 0){
			printf("ping baidu not ok 4444444444444\n");
			goto exit;
		}
		*/
		printf("MQTTPacket_read error m[%d] [%s] [%d]\n",m,__func__,__LINE__);
		len = MQTTSerialize_disconnect(buf, buflen);
		rc = transport_sendPacketBuffer(mysock, buf, len);
		transport_close(mysock);
		sleep(10);
		goto new_ping;
		}


	start_ping_timer();
	
	while(!toStop)
	{
		while(!time_to_ping())
		{
			memset(buf,0,4096);
			int m = MQTTPacket_read(buf, buflen, transport_getdata);
			if(m == PUBLISH)
			{
				unsigned char dup;
				int qos;
				unsigned char retained;
				unsigned short msgid;
				
				int rc;
				MQTTString receivedTopic;
				rc = MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic,
						&payload_in, &payloadlen_in, buf, buflen);
				msq_send(2,payload_in);
			}
			else
			{
				//printf("else [%d]............######################\n",m);
			}
		}

		memset(buf,0,4096);
		len = MQTTSerialize_pingreq(buf, buflen);
		transport_sendPacketBuffer(mysock, buf, len);
		recv_count = 0;

read_pingresp:
		memset(buf,0,4096);
		int n;
		n = MQTTPacket_read(buf, buflen, transport_getdata);
				
		//if (MQTTPacket_read(buf, buflen, transport_getdata) == PINGRESP){
		if (n == PINGRESP){
			start_ping_timer();		
		} else if(n == PUBLISH){
				//printf("\n\n ##########  publish flag #############\n\n");
				unsigned char dup1;
				int qos1;
				unsigned char retained1;
				unsigned short msgid1;
				MQTTString receivedTopic1;
				rc = MQTTDeserialize_publish(&dup1, &qos1, &retained1, &msgid1, &receivedTopic1,
						&payload_in, &payloadlen_in, buf, buflen);
				
				msq_send(2,payload_in);
				recv_count++;
				
				//printf("recv_count[%d] message arrived: %.*s####################\n",recv_count, payloadlen_in, payload_in);

				if(recv_count > 30){
					printf("[%s] recv_count[%d] no PINGRESP ........................\n",__func__,recv_count);
					len = MQTTSerialize_disconnect(buf, buflen);
					rc = transport_sendPacketBuffer(mysock, buf, len);
					transport_close(mysock);
					goto new_ping;
				}
				goto read_pingresp;
					
		} else {
			len = MQTTSerialize_disconnect(buf, buflen);
			rc = transport_sendPacketBuffer(mysock, buf, len);
			transport_close(mysock);
			usleep(5000000);
			printf("errid:[%d] \n",n);
			//system("date >> /usr/data/ping.txt");

			goto new_ping;
		}	
	}

	printf("disconnecting\n");
	len = MQTTSerialize_disconnect(buf, buflen);
	rc = transport_sendPacketBuffer(mysock, buf, len);

exit:
	transport_close(mysock);
	//system("date >> /usr/data/ping.txt");
	printf("mqtt exit .......................\n");

	return 0;

}

int msq_send(int flag,unsigned char *buf)
{
	int fd_ir;
	if(strstr(buf,"IRLEARNCLOSE") != NULL){
		//printf("$$$$$$$$$$$$$$ [%s] ###########\n",buf);
		
		 char ir_byte = 0;
		 fd_ir = open("/dev/ir_learn",O_WRONLY);
		 if(fd_ir<0){
			 printf("open /dev/ir_learn error\n"); 
			 return -1;
		 }
		 write(fd_ir,&ir_byte,sizeof(ir_byte));	 
		 close(fd_ir);
		 
		//printf("退出红外学习模式 [%s] ###########\n",buf);
		return 0;
	}


	struct msg_st MessagesUart;
	int msgid = -1;
	//建立消息队列
	msgid = msgget((key_t)4781, 0666 | IPC_EXCL);
	if(msgid == -1)
	{
		fprintf(stderr, "msgget failed with error: %d\n", errno);
		return -1;
	}

	MessagesUart.msg_type = 1;
	MessagesUart.flag = flag;
	strcpy(MessagesUart.text,buf);

	//向队列发送数据
	if(msgsnd(msgid,&MessagesUart,sizeof(struct msg_st),0) == -1)
	{
		fprintf(stderr, "msgsnd failed\n");
		return -2;
	}
	return 0;
}

