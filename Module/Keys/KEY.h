#ifndef  __key_h
#define  __key_h /**< __key_h 头文件重复包含保护宏。 */

#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#define LONGPRESSMS 1000 /**< LONGPRESSMS 宏定义。 */
#define PRESSMS 20 /**< PRESSMS 宏定义。 */
#define DOUBLEPRESSMS 300 /**< DOUBLEPRESSMS 宏定义。 */
typedef enum{
   B1_KEY=0,
   B2_KEY,
   B3_KEY,
   B4_KEY,
  KEY_COUNT
}KEY_ID;

typedef enum
{
  key_release=0,
  key_press,
  key_longpress,
  key_doubleclick
}Key_StateTypedef;

/**
 * @brief 独立按键扫描句柄。
 * @note 旧按键模块使用的状态结构，保存 GPIO 位置、按下时间和单击/长按/双击状态。
 */
typedef struct{
  GPIO_Regs* gpio;              /**< 按键所在 GPIO 端口。 */
  uint32_t pins;                /**< 按键 GPIO 引脚掩码。 */

  uint8_t Down;                 /**< 当前按键电平/按下状态缓存。 */
  uint32_t press_tick;          /**< 最近一次按下时刻，单位由 key_count 决定。 */
  uint32_t release_tick;        /**< 最近一次释放时刻，单位由 key_count 决定。 */
  Key_StateTypedef key_state;   /**< 当前按键事件状态。 */
  uint16_t click_cnt;           /**< 连击计数。 */
}KEY_HandleTypedef;

extern int Key_choice; /**< Key_choice 全局状态或配置变量。 */
extern int Key_choice_old; /**< Key_choice_old 全局状态或配置变量。 */
extern bool Key_choice_flag; /**< Key_choice_flag 全局状态或配置变量。 */

extern KEY_HandleTypedef keyhandle[KEY_COUNT]; /**< keyhandle 全局状态或配置变量。 */
//extern int key_value;
extern uint16_t key_count;		//放入1ms定时器 累加
/**
 * @brief 执行 K E Y  Get 功能。
 * @param hkey hkey 参数。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void KEY_Get(KEY_HandleTypedef *hkey);
int KEY_FLAG_Get(void);				//10ms定时器中断读取
void KEY_Init(KEY_HandleTypedef *hkey,GPIO_Regs *gpio,uint32_t pins);

/*			*******		使用方法		*********
	①											//放入初始化
	KEY_Init(&keyhandle[B1_KEY],Key_PORT,Key_F1_PIN);
	KEY_Init(&keyhandle[B2_KEY],Key_PORT,Key_F2_PIN);

	②											//放入1ms定时器
	key_count++;
	if(++TA_10ms>=10)
	{
		Key_choice=KEY_FLAG_Get();
		if(Key_choice==0){Key_choice_flag=1;}
		TA_10ms=0;
	}
	③											//放入while(1)中
	Key_Proc()
*/
#endif
