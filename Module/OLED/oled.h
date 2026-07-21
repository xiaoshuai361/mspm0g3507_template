#ifndef MODULE_OLED_H
#define MODULE_OLED_H /**< MODULE_OLED_H 头文件重复包含保护宏。 */

#include "delay.h"
#include "stdlib.h"	

//-----------------OLED端口定义----------------

#define OLED_SCL_Clr()  do { DL_GPIO_clearPins(OLED_PORT, OLED_SCL_PIN); DL_GPIO_enableOutput(OLED_PORT, OLED_SCL_PIN); } while(0)//SCL
/** @brief OLED 显示驱动配置宏：OLED_SCL_Set()。 */
#define OLED_SCL_Set()  DL_GPIO_disableOutput(OLED_PORT, OLED_SCL_PIN)

#define OLED_SDA_Clr()  do { DL_GPIO_clearPins(OLED_PORT, OLED_SDA_PIN); DL_GPIO_enableOutput(OLED_PORT, OLED_SDA_PIN); } while(0)//SDA
/** @brief OLED 显示驱动配置宏：OLED_SDA_Set()。 */
#define OLED_SDA_Set()  DL_GPIO_disableOutput(OLED_PORT, OLED_SDA_PIN)

// 4-pin I2C OLED modules do not expose RES/DC/CS. Keep these no-op
// macros so old code that references them still compiles.
/** @brief OLED 显示驱动配置宏：OLED_RES_Clr()。 */
#define OLED_RES_Clr()  ((void)0)
/** @brief OLED 显示驱动配置宏：OLED_RES_Set()。 */
#define OLED_RES_Set()  ((void)0)
/** @brief OLED 显示驱动配置宏：OLED_DC_Clr()。 */
#define OLED_DC_Clr()   ((void)0)
/** @brief OLED 显示驱动配置宏：OLED_DC_Set()。 */
#define OLED_DC_Set()   ((void)0)
/** @brief OLED 显示驱动配置宏：OLED_CS_Clr()。 */
#define OLED_CS_Clr()   ((void)0)
/** @brief OLED 显示驱动配置宏：OLED_CS_Set()。 */
#define OLED_CS_Set()   ((void)0)


#define OLED_CMD  0	//写命令
#define OLED_DATA 1	//写数据

/**
 * @brief 执行 O L E D  Clear Point 功能。
 * @param x 横坐标或输入值。
 * @param y 纵坐标或输入值。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_ClearPoint(u8 x,u8 y);
void OLED_ColorTurn(u8 i);
void OLED_DisplayTurn(u8 i);
/**
 * @brief 执行 O L E D  W R  Byte 功能。
 * @param dat dat 参数。
 * @param mode 显示或控制模式。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_WR_Byte(u8 dat,u8 mode);
void OLED_DisPlay_On(void);
void OLED_DisPlay_Off(void);
/**
 * @brief 执行 O L E D  Refresh 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_Refresh(void);
void OLED_ClearBuffer(void);
void OLED_Clear(void);
void OLED_DrawPoint(u8 x,u8 y,u8 t);
/**
 * @brief 执行 O L E D  Draw Line 功能。
 * @param x1 x1 参数。
 * @param y1 y1 参数。
 * @param x2 x2 参数。
 * @param y2 y2 参数。
 * @param mode 显示或控制模式。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_DrawLine(u8 x1,u8 y1,u8 x2,u8 y2,u8 mode);
/**
 * @brief 执行 O L E D  Draw Circle 功能。
 * @param x 横坐标或输入值。
 * @param y 纵坐标或输入值。
 * @param r r 参数。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_DrawCircle(u8 x,u8 y,u8 r);
void OLED_ShowChar(u8 x,u8 y,char chr,u8 size1,u8 mode);
/**
 * @brief 执行 O L E D  Show Char6x8 功能。
 * @param x 横坐标或输入值。
 * @param y 纵坐标或输入值。
 * @param chr chr 参数。
 * @param mode 显示或控制模式。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_ShowChar6x8(u8 x,u8 y,u8 chr,u8 mode);
/**
 * @brief 在 OLED 指定位置显示字符串。
 * @param x 横坐标或输入值。
 * @param y 纵坐标或输入值。
 * @param chr chr 参数。
 * @param size1 size1 参数。
 * @param mode 显示或控制模式。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_ShowString(u8 x,u8 y,const char *chr,u8 size1,u8 mode);
/**
 * @brief 执行 O L E D  Show Num 功能。
 * @param x 横坐标或输入值。
 * @param y 纵坐标或输入值。
 * @param num num 参数。
 * @param len 数据长度。
 * @param size1 size1 参数。
 * @param mode 显示或控制模式。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_ShowNum(u8 x,u8 y,u32 num,u8 len,u8 size1,u8 mode);
/**
 * @brief 执行 O L E D  Show Chinese 功能。
 * @param x 横坐标或输入值。
 * @param y 纵坐标或输入值。
 * @param num num 参数。
 * @param size1 size1 参数。
 * @param mode 显示或控制模式。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_ShowChinese(u8 x,u8 y,u8 num,u8 size1,u8 mode);
/**
 * @brief 执行 O L E D  Scroll Display 功能。
 * @param num num 参数。
 * @param space space 参数。
 * @param mode 显示或控制模式。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_ScrollDisplay(u8 num,u8 space,u8 mode);
void OLED_ShowPicture(u8 x,u8 y,u8 sizex,u8 sizey,u8 BMP[],u8 mode);
/**
 * @brief 初始化 OLED 显示屏。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void OLED_Init(void);

#endif

