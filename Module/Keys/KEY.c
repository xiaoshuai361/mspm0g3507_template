#include "KEY.h"

int Key_choice=0; /**< Key_choice 全局状态或配置变量。 */
int Key_choice_old; /**< Key_choice_old 全局状态或配置变量。 */
bool Key_choice_flag=0; /**< Key_choice_flag 全局状态或配置变量。 */


KEY_HandleTypedef keyhandle[KEY_COUNT]; /**< keyhandle 全局状态或配置变量。 */
uint16_t key_count; /**< key_count 全局状态或配置变量。 */

int key_value; /**< key_value 全局状态或配置变量。 */
/**
 * @brief 初始化独立按键模块。
 * @param hkey 按键句柄。
 * @param gpio gpio 参数。
 * @param pins GPIO 引脚掩码。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void KEY_Init(KEY_HandleTypedef *hkey, GPIO_Regs *gpio, uint32_t pins)
{
  hkey->gpio = gpio;
  hkey->pins = pins;
  hkey->key_state = key_release;
  hkey->click_cnt = 0;
  hkey->Down = 0;
  hkey->press_tick = 0;
  hkey->release_tick = 0;
}
/**
 * @brief 扫描独立按键状态。
 * @param hkey 按键句柄。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void KEY_Get(KEY_HandleTypedef *hkey)
{
  // uint32_t now = RTCC_GetTime();
  uint32_t now = key_count; 
  if (hkey->click_cnt == 1 && (now - hkey->release_tick) > DOUBLEPRESSMS)
  {
    hkey->click_cnt = 0;
    hkey->key_state = key_press;
  }

  if (DL_GPIO_readPins(hkey->gpio, hkey->pins) == 0)
  {
    if (!hkey->Down)
    {
      hkey->Down = 1;
      hkey->press_tick = now;
    }
    else if ((now - hkey->press_tick) >= LONGPRESSMS && hkey->key_state != key_longpress)
    {
      hkey->key_state = key_longpress;
    }
  }
  else
  {
    if (hkey->Down)
    {
      uint32_t delta = now - hkey->press_tick;
      if (delta >= PRESSMS && delta < LONGPRESSMS)
      {
        if (hkey->click_cnt == 0)
        {
          hkey->release_tick = now;
          hkey->click_cnt = 1;
        }
        else
        {
          if ((now - hkey->release_tick) <= DOUBLEPRESSMS)
          {
            hkey->key_state = key_doubleclick;
          }
          hkey->click_cnt = 0;
        }
      }
      hkey->Down = 0;
    }
  }
}
/**
 * @brief 获取独立按键事件标志。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回整型结果。
 */
int KEY_FLAG_Get(void)
{
  for (int i = 0; i < 2; i++)
  {
    KEY_Get(&keyhandle[i]);
  }
  
  if(keyhandle[0].key_state == key_press)
  {
     key_value = 1;
    keyhandle[0].key_state = key_release;
  }
  
  else if(keyhandle[0].key_state == key_doubleclick)		//F1双击
  {
     key_value = 11;
    keyhandle[0].key_state = key_release;
  }
  
  else if(keyhandle[0].key_state == key_longpress)		//F1长按
  {
     key_value = 111;
    keyhandle[0].key_state = key_release;
  }

/***************************************************************/
  
  else if(keyhandle[1].key_state == key_press)
  {
    key_value= 2;
    keyhandle[1].key_state = key_release;
  }
  
  else if(keyhandle[1].key_state == key_doubleclick)	//双击
  {
    key_value= 22;
    keyhandle[1].key_state = key_release;
  }
  
   else if(keyhandle[1].key_state == key_longpress)		//F2长按
  {
     key_value = 222;
    keyhandle[1].key_state = key_release;
  }
  

  else
  {
    key_value = 0;
  } 
   return key_value;
}

/*******************		使用时放入while（1）中		**********************************/
//void	Key_Proc(void)
//{
//	if(Key_choice!=0)	//记录按键按下的值
//	{
//		Key_choice_old=Key_choice;
//	}
//	
//	if(Key_choice_flag){	//状态变化标志位  只执行一次  等待按键松开  标置位变为1  等待按下
//		
//		switch(Key_choice) {
//		
//		case 1: // 按键1单击
//			U_Num++;
//		
//			Key_choice_flag=0;
//			break;
//		
//		case 111: // 按键1长按
//			U_Num=U_Num+10;
//		
//			Key_choice_flag=0;
//			break;
//		
//		case 2: // 按键2单击
//			U_Num--;
//		
//			Key_choice_flag=0;
//			break;
//		
//		case 222: // 按键2长按
//			U_Num=U_Num-10;

//		
//			Key_choice_flag=0;
//			break;
//		default:
//			
//			break;
//		}
//	
//	}
//}

