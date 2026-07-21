#ifndef __IMU_H
#define __IMU_H /**< __IMU_H 头文件重复包含保护宏。 */

#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "math.h"
#ifndef M_PI
#define M_PI (3.1415926535f) /**< M_PI 宏定义。 */
#endif
/**
 * @brief 三轴浮点数据。
 * @note 用于保存加速度、角速度或姿态解算中的方向向量。
 */
typedef struct
{
    float x; /**< x 轴数据。 */
    float y; /**< y 轴数据。 */
    float z; /**< z 轴数据。 */
} xyz_f_t;
extern xyz_f_t north, west;
extern volatile float yaw[5]; // 处理航向的增值
extern float motion6[7]; /**< motion6 全局状态或配置变量。 */
// Mini IMU AHRS 解算的API
bool IMU_init(void);                  // 初始化，成功返回 true
bool IMU_getYawPitchRoll(float *ypr); // 更新姿态，读取失败返回 false
int16_t IMU_DegreesToTenths(float degrees); // 角度转换为 0.1 度单位
/**
 * @brief 执行 I M U  Sensor Data Is All Zero 功能。
 * @param sensorData sensorData 参数。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval true 表示满足条件，false 表示未满足。
 */
bool IMU_SensorDataIsAllZero(const float sensorData[6]);
void IMU_GetLastSensorData(float sensorData[6]);
uint8_t IMU_GetLastBusStatus(void);
uint8_t IMU_GetWhoAmI(void);
/**
 * @brief 执行 I M U  Probe Address 功能。
 * @param address address 参数。
 * @param whoAmI whoAmI 参数。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t IMU_ProbeAddress(uint8_t address, uint8_t *whoAmI);
void IMU_TT_getgyro(float *zsjganda);
// uint32_t micros(void);	//读取系统上电后的时间  单位 us
/**
 * @brief 执行 M P U6050  Init Ang  Offset 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void MPU6050_InitAng_Offset(void);
#endif

//------------------End of File----------------------------
