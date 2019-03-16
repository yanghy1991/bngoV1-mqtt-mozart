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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>


#include "MQTTPacket.h"
#include "transport.h"
#include "cJSON.h"
#include "bngo_info.h"


static void (*cJSON_freeMQTT)(void *ptr) = free;


typedef struct sds_results_file{
	int ret;
	/**
	 * @brief input 输入文本.
	 */
	char input[1024];
	/**
	 * @brief output BNGO输出.
	 */
	char output[1024];
	/**   
	 * @brief src 操作意图
	 */  
	char domain[128];
	/**   
	 * @brief src 操作意图
	 */  
	char data[1024];
	/**   
	 * @brief src 指令标识
	 */  
	char mark[128];
}sds_results_file;


//int makeJson(char *p,char *input,char *output,const char *domain,int asr_ret)
int makeJson(char *p,sds_results_file *sds_results_msg)
{
	cJSON *root = NULL;
	cJSON *root_out = NULL;
	cJSON *root_bngo = NULL;
	char *root_src = NULL;
	
	root = cJSON_CreateObject(); // 创建根
	if (!root)  
    {  
        printf("Error before: [%s]\n",cJSON_GetErrorPtr());  
        return -1;  
    } 
	// 方法 使用cJSON_AddItemToObject，推荐使用  
    cJSON_AddItemToObject(root, "input", cJSON_CreateString(sds_results_msg->input)); 
	
	if(strstr(sds_results_msg->output,"{") != NULL){
		root_out = cJSON_Parse(sds_results_msg->output);
		cJSON_AddItemToObject(root, "output", root_out);
	} else {
		cJSON_AddItemToObject(root, "output", cJSON_CreateString(sds_results_msg->output)); 
	}


	cJSON_AddItemToObject(root, "domain", cJSON_CreateString(sds_results_msg->domain));
	cJSON_AddItemToObject(root, "mark", cJSON_CreateString(sds_results_msg->mark));
	cJSON_AddItemToObject(root, "asr_ret", cJSON_CreateNumber(sds_results_msg->ret)); 

	//如果是获取mqtt对话信息，把其他返回去掉
	if (!strcmp(sds_results_msg->domain, "APP_ASR_INFO")){
		goto asr_info;	
	}

	if (!strcmp(sds_results_msg->domain, "APP_COMMAND_OK"))
		cJSON_AddItemToObject(root, "data", cJSON_CreateString(sds_results_msg->data)); 

	int ret = 0;
	char ASR_COMMAND[512] = {0};
	if(sds_results_msg->ret == 50){
		//bngo处于唤醒状态 不返回bngo_info
		strcpy(ASR_COMMAND,"{\"error\":\"get bngo info failed\"}");
	} else {
		//该函数在libbngo_info.so 里面定义
		ret =  send_bngo_info(ASR_COMMAND);
		if(ret != 0)
			strcpy(ASR_COMMAND,"{\"error\":\"get bngo info failed\"}");
	}
	
	if(strstr(ASR_COMMAND,"{") != NULL){
		root_bngo = cJSON_Parse(ASR_COMMAND);
		cJSON_AddItemToObject(root, "bngo_info", root_bngo);
	} 


	//保存十条语音数据
	if(strstr(sds_results_msg->domain, "BNGO") != NULL && strcmp(sds_results_msg->output, "小乐处于工作状态，请稍后再试"))
		save_mqtt_results(sds_results_msg->input,sds_results_msg->output);

asr_info:
	
	root_src = cJSON_PrintUnformatted(root);
	memcpy(p,root_src,strlen(root_src)+1);

	if(root_src != NULL){
		//free(root_src);
		cJSON_freeMQTT(root_src);
	}
		
	if(root != NULL){
		cJSON_Delete(root);
	}	
	return 0;
}

static void *mqtt_publish_func(void *sds_results_bk)
{

		sds_results_file sds_results_to_app;
		memset(&sds_results_to_app,0,sizeof(sds_results_file));
		memcpy(&sds_results_to_app,sds_results_bk,sizeof(sds_results_file));
				
		//获取bngo设备ID
		char deviceID[30] = {0};
		char clientID[30] = {0};
	
		get_bngoID_mqttpub(deviceID);
		
		strcpy(clientID,deviceID);
		//用户ID
		strcat(clientID,"_b");
	
		//发布主题
		strcat(deviceID,"_1");
		////
	
		char p[2048] = {0};
	
		memset(p,0,2048);
		
		
		makeJson(p,sds_results_bk);
	
		//printf("###\n\n [%s] [%d] p:%s### \n\n",__func__,__LINE__,p);
	
		MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
		
		int rc = 0;
		
		char buf[2048] = {0};
		int buflen = sizeof(buf);
		int mysock = 0;
		MQTTString topicString = MQTTString_initializer;
		char* payload = p;
		int payloadlen = strlen(payload);
		int len = 0;
		char *host = "47.106.157.206";
	
		int port = 1883;
	
		mysock = transport_open(host,port);
		if(mysock < 0){
			return NULL;
		}
	
	
		//printf("Sending to hostname %s port %d\n", host, port);
	
		data.clientID.cstring = clientID;
		data.keepAliveInterval = 20;
		data.cleansession = 1;
		data.MQTTVersion = 4;
	
		//data.username.cstring = "LTAI9oCP1CgSjNGO";
		//data.password.cstring = "8/99LxBY5z/f/BpUm13aFHph7Tc=";
		
		len = MQTTSerialize_connect((unsigned char *)buf, buflen, &data);
	
		topicString.cstring = deviceID;
		len += MQTTSerialize_publish((unsigned char *)(buf + len), buflen - len, 0, 0, 0, 0, topicString, (unsigned char *)payload, payloadlen);
	
		len += MQTTSerialize_disconnect((unsigned char *)(buf + len), buflen - len);
	
		rc = transport_sendPacketBuffer(mysock, (unsigned char*)buf, len);
	
		if (rc == len)
			;//printf("[%s] pub Successfully\n",__func__);
		else
			printf("[%s] pub  failed\n",__func__);
		
	
		close(mysock);
	return NULL;
}



int mqtt_publish(sds_results_file *sds_results_msg)
{
	sds_results_file sds_results_bk;
	memset(&sds_results_bk,0,sizeof(sds_results_file));
	memcpy(&sds_results_bk,sds_results_msg,sizeof(sds_results_file));

	pthread_t mqtt_publish_pthread;
	if (pthread_create(&mqtt_publish_pthread, NULL, mqtt_publish_func, &sds_results_bk) == -1)
			printf("Create akeymatch_pthread failed: %s.\n", strerror(errno));
	pthread_detach(mqtt_publish_pthread);

/*
	//判断terminal_uid:是否存在
	if (access("/usr/data/terminal_uid.txt",F_OK) != 0)
		return -1;
	char terminal_str[50] = {0};

	FILE *fd;
	fd = fopen("/usr/data/terminal_uid.txt","r+");
	if(fd == NULL){
		printf("[%s] open terminal_uid.txt error!!!!.\n",__func__);
		return -1;
	}
	fread(terminal_str,50,1,fd);
	fclose(fd);

	if(strlen(terminal_str) < 10)
		return -1;
*/

	
#if 0
	//获取bngo设备ID
	char deviceID[30] = {0};
	char clientID[30] = {0};

	get_bngoID_mqttpub(deviceID);
	
	strcpy(clientID,deviceID);
	//用户ID
	strcat(clientID,"_b");

	//发布主题
	strcat(deviceID,"_1");
	////

	char p[2048] = {0};

	memset(p,0,2048);
	

	makeJson(p,&sds_results_bk);

	printf("### [%s] [%d] ### \n",__func__,__LINE__);

	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	
	int rc = 0;
	
	char buf[2048] = {0};
	int buflen = sizeof(buf);
	int mysock = 0;
	MQTTString topicString = MQTTString_initializer;
	char* payload = p;
	int payloadlen = strlen(payload);
	int len = 0;
	char *host = "47.106.157.206";

	int port = 1883;

	mysock = transport_open(host,port);
	if(mysock < 0){
		return mysock;
	}


	//printf("Sending to hostname %s port %d\n", host, port);

	data.clientID.cstring = clientID;
	data.keepAliveInterval = 20;
	data.cleansession = 1;
	data.MQTTVersion = 4;

	//data.username.cstring = "LTAI9oCP1CgSjNGO";
	//data.password.cstring = "8/99LxBY5z/f/BpUm13aFHph7Tc=";
	
	len = MQTTSerialize_connect((unsigned char *)buf, buflen, &data);

	topicString.cstring = deviceID;
	len += MQTTSerialize_publish((unsigned char *)(buf + len), buflen - len, 0, 0, 0, 0, topicString, (unsigned char *)payload, payloadlen);

	len += MQTTSerialize_disconnect((unsigned char *)(buf + len), buflen - len);

	rc = transport_sendPacketBuffer(mysock, (unsigned char*)buf, len);

	if (rc == len)
		printf("[%s] pub Successfully\n",__func__);
	else
		printf("[%s] pub  failed\n",__func__);
	

	close(mysock);
	//transport_close(mysock);

#endif
	return 0;
}
