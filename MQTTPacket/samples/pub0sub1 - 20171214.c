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

#include "MQTTPacket.h"
#include "transport.h"


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <pthread.h>

int error_count =0;
static int success_count =0;

struct msg_st
{
	long int msg_type;
	int flag;
	char text[4096];
};

static pthread_t publish_logic_thread;
int mysock = 0;
int payloadlen_in;
unsigned char *payload_in;
MQTTString topicString = MQTTString_initializer;
//char* payload = "mypayload";
//int payloadlen = strlen(payload);
unsigned char buf[4096];
int buflen = sizeof(buf);
int len = 0;

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
/* */
//创建消息队列，用于接收推送、闹钟、点播的消息
static void *publish_pthread_func(void *params)
{
	int running = 1;
	int msgid = -1;
	struct msg_st MessagesData;
	long int msgtype = 0; 

	msgid = msgget((key_t)4784, 0666 | IPC_CREAT);
	if(msgid == -1)
	{
		fprintf(stderr, "msgget failed with error: %d\n", errno);
		exit(EXIT_FAILURE);
	}
	//循环检测数据有没有
	while(running)
	{	
		memset(&MessagesData,0,sizeof(struct msg_st));
		if(msgrcv(msgid, (void*)&MessagesData, sizeof(struct msg_st), msgtype, 0) == -1)
		{
			fprintf(stderr, "msgrcv failed with errno: %d\n", errno);
			exit(EXIT_FAILURE);
		}
		if(MessagesData.flag == 4)
		{	
			memset(buf,0,4096);
			printf("pub0sub1 recv: %s\n",MessagesData.text);
			
			if(!strcmp(MessagesData.text,"1")){
					success_count++;
					printf("success count:%d .@@@@@@@@@@@@@@@@@@@@@@\n",success_count);
			}
			
			topicString.cstring = "mytopic";
        	len = MQTTSerialize_publish(buf, buflen, 0, 0, 0, 0, topicString, (unsigned char*)MessagesData.text, strlen(MessagesData.text));
       		transport_sendPacketBuffer(mysock, buf, len);
		} 
			
		usleep(100000);
		
	}
	//删除消息队列
	if(msgctl(msgid, IPC_RMID, 0) == -1)
	{
		fprintf(stderr, "msgctl(IPC_RMID) failed\n");
		return NULL;
	}
	return NULL;
}
//***


int main(int argc, char *argv[])
{

	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	int rc = 0;
	int msgid = 1;
	int req_qos = 0;
	char *host = "39.108.71.229";
	//char *host = "192.168.1.84";
	int port = 1883;
	
new_ping:

	stop_init();
	if (argc > 1)
		host = argv[1];

	if (argc > 2)
		port = atoi(argv[2]);
	
	mysock = transport_open(host, port);
	if(mysock < 0)
		return mysock;

	printf("Sending to hostname %s port %d\n", host, port);

	//data.clientID.cstring = "szh1_ccb8a82c4df8";
	data.clientID.cstring = "szh1_yhy";
	data.keepAliveInterval = KEEPALIVE_INTERVAL;
	data.cleansession = 1;
	//data.username.cstring = "testuser";
	//data.password.cstring = "testpassword";
	
	len = MQTTSerialize_connect(buf, buflen, &data);
	
	rc = transport_sendPacketBuffer(mysock, buf, len);

	/* wait for connack */
	if (MQTTPacket_read(buf, buflen, transport_getdata) == CONNACK)
	{
		unsigned char sessionPresent, connack_rc;

		if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1 || connack_rc != 0)
		{
			printf("Unable to connect, return code %d\n", connack_rc);
			goto exit;



		}
	}
	else{
		goto exit;

	}
	/* subscribe */
	topicString.cstring = "substopic_app";
	len = MQTTSerialize_subscribe(buf, buflen, 0, msgid, 1, &topicString, &req_qos);
	rc = transport_sendPacketBuffer(mysock, buf, len);
	if (MQTTPacket_read(buf, buflen, transport_getdata) == SUBACK) 	/* wait for suback */
	{
		unsigned short submsgid;
		int subcount;
		int granted_qos;

		rc = MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, buf, buflen);
		if (granted_qos != 0)
		{
			printf("granted qos != 0, %d\n", granted_qos);
			goto exit;
		}
	}	
	else{
		goto exit;

		}


	//发布线程
	//if (pthread_create(&publish_logic_thread, NULL, publish_pthread_func, NULL) == -1)
			//printf("Create updater check version pthread failed: %s.\n", strerror(errno));
	//pthread_detach(publish_logic_thread);
	/////////////////////////////////
	start_ping_timer();
	
	while(!toStop)
	{
		while(!time_to_ping())
		{
			memset(buf,0,4096);
			if(MQTTPacket_read(buf, buflen, transport_getdata) == PUBLISH)
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

				//printf("publishing reading\n");
        		//下面两行是用来发布消息。这里发布，上面订阅，就形成了一个循环。
        		//topicString.cstring = "mytopic";
        		//len = MQTTSerialize_publish(buf, buflen, 0, 0, 0, 0, topicString, (unsigned char*)payload, payloadlen);
       			//rc = transport_sendPacketBuffer(mysock, buf, len);

			}
			else
			{
				//printf("else .........\n");
			}
		}
		memset(buf,0,4096);

		len = MQTTSerialize_pingreq(buf, buflen);
		transport_sendPacketBuffer(mysock, buf, len);
		
		memset(buf,0,4096);
		int n;
		n = MQTTPacket_read(buf, buflen, transport_getdata);
				
		//if (MQTTPacket_read(buf, buflen, transport_getdata) == PINGRESP){
		if (n == PINGRESP){
			start_ping_timer();

		} else {
			goto exit;
			
			//transport_close(mysock);
			//pthread_cancel(publish_logic_thread);
			//pthread_join(publish_logic_thread,NULL);

			//sleep(5);
			//goto new_ping;
		}	
	}

	printf("disconnecting\n");
	len = MQTTSerialize_disconnect(buf, buflen);
	rc = transport_sendPacketBuffer(mysock, buf, len);

exit:
	transport_close(mysock);
	//pthread_cancel(publish_logic_thread);
	//pthread_join(publish_logic_thread,NULL);
	printf("mqtt exit .......................\n");

	return 0;

}

int msq_send(int flag,unsigned char *buf)
{
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

