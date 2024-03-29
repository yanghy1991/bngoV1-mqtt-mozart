#ifndef __BNGO_INFO_H_
#define __BNGO_INFO_H_

/**
 * @brief music_info_t BNGO信息结构体.
 */
typedef struct bngo_info_t {
	/**
	 * @brief led 夜灯亮度.
	 */
	char led_val[10];
	/**
	 * @brief led 夜灯开关标志.
	 */
	int led_ison;
	/**
	 * @brief 延时熄灯闹钟标志.
	 */
	int led_alarm_flag;

	/**
	 * @brief title 系统休眠.
	 */
	int sleep_flag;

	/**
	 * @brief title 显示时间
	 */
	int time_flag;
	
	/**
	 * @brief title 夜间模式
	 */
	int night_flag;
}bngo_info_t;

int set_bngo_info(int cmd,bngo_info_t bngo_info_msg);
int get_bngo_info(bngo_info_t *bngo_info_msg);
//发送bngo info 给app
int send_bngo_info(char *ASR_COMMAND);
//修改设备名称
int set_bngo_name(char *name,char *ASR_COMMAND);
//获取wifi信号强度
int get_wifi_signal(unsigned char *dbm);
//获取wifi SSID
int get_wifi_ssid(char *ssid);
//获取设备id
//int get_bngoID(char *bngoId);
//获取设备id 没有经过加密
int get_bngoID_mqttpub(char *bngoId);
//保存十条语音数据，用于对话显示
int save_mqtt_results(char *input,char *output);
//读取十条语音数据，用于对话显示
int get_mqtt_results(char *ASR_COMMAND);

#endif //__BNGO_INFO_H_

