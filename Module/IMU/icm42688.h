#ifndef __BSP_ICM42688_H__
#define __BSP_ICM42688_H__ /**< __BSP_ICM42688_H__ 头文件重复包含保护宏。 */

#include <stdint.h>

//#define ICM_USE_HARD_SPI
#define ICM_USE_I2C /**< 启用 ICM42688 的 I2C 通信路径。 */

/* ICM42688 registers
https://store.invensense.com/datasheets/invensense/DS-ICM-42688v1-2.pdf
*/
// Bank 0
#define ICM42688_DEVICE_CONFIG             0x11 /**< ICM42688 寄存器地址或配置值宏：ICM42688_DEVICE_CONFIG。 */
#define ICM42688_DRIVE_CONFIG              0x13 /**< ICM42688 寄存器地址或配置值宏：ICM42688_DRIVE_CONFIG。 */
#define ICM42688_INT_CONFIG                0x14 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_CONFIG。 */
#define ICM42688_FIFO_CONFIG               0x16 /**< ICM42688 寄存器地址或配置值宏：ICM42688_FIFO_CONFIG。 */
#define ICM42688_TEMP_DATA1                0x1D /**< ICM42688 寄存器地址或配置值宏：ICM42688_TEMP_DATA1。 */
#define ICM42688_TEMP_DATA0                0x1E /**< ICM42688 寄存器地址或配置值宏：ICM42688_TEMP_DATA0。 */
#define ICM42688_ACCEL_DATA_X1             0x1F /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_DATA_X1。 */
#define ICM42688_ACCEL_DATA_X0             0x20 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_DATA_X0。 */
#define ICM42688_ACCEL_DATA_Y1             0x21 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_DATA_Y1。 */
#define ICM42688_ACCEL_DATA_Y0             0x22 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_DATA_Y0。 */
#define ICM42688_ACCEL_DATA_Z1             0x23 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_DATA_Z1。 */
#define ICM42688_ACCEL_DATA_Z0             0x24 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_DATA_Z0。 */
#define ICM42688_GYRO_DATA_X1              0x25 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_DATA_X1。 */
#define ICM42688_GYRO_DATA_X0              0x26 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_DATA_X0。 */
#define ICM42688_GYRO_DATA_Y1              0x27 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_DATA_Y1。 */
#define ICM42688_GYRO_DATA_Y0              0x28 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_DATA_Y0。 */
#define ICM42688_GYRO_DATA_Z1              0x29 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_DATA_Z1。 */
#define ICM42688_GYRO_DATA_Z0              0x2A /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_DATA_Z0。 */
#define ICM42688_TMST_FSYNCH               0x2B /**< ICM42688 寄存器地址或配置值宏：ICM42688_TMST_FSYNCH。 */
#define ICM42688_TMST_FSYNCL               0x2C /**< ICM42688 寄存器地址或配置值宏：ICM42688_TMST_FSYNCL。 */
#define ICM42688_INT_STATUS                0x2D /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_STATUS。 */
#define ICM42688_FIFO_COUNTH               0x2E /**< ICM42688 寄存器地址或配置值宏：ICM42688_FIFO_COUNTH。 */
#define ICM42688_FIFO_COUNTL               0x2F /**< ICM42688 寄存器地址或配置值宏：ICM42688_FIFO_COUNTL。 */
#define ICM42688_FIFO_DATA                 0x30 /**< ICM42688 寄存器地址或配置值宏：ICM42688_FIFO_DATA。 */
#define ICM42688_APEX_DATA0                0x31 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_DATA0。 */
#define ICM42688_APEX_DATA1                0x32 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_DATA1。 */
#define ICM42688_APEX_DATA2                0x33 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_DATA2。 */
#define ICM42688_APEX_DATA3                0x34 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_DATA3。 */
#define ICM42688_APEX_DATA4                0x35 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_DATA4。 */
#define ICM42688_APEX_DATA5                0x36 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_DATA5。 */
#define ICM42688_INT_STATUS2               0x37 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_STATUS2。 */
#define ICM42688_INT_STATUS3               0x38 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_STATUS3。 */
#define ICM42688_SIGNAL_PATH_RESET         0x4B /**< ICM42688 寄存器地址或配置值宏：ICM42688_SIGNAL_PATH_RESET。 */
#define ICM42688_INTF_CONFIG0              0x4C /**< ICM42688 寄存器地址或配置值宏：ICM42688_INTF_CONFIG0。 */
#define ICM42688_INTF_CONFIG1              0x4D /**< ICM42688 寄存器地址或配置值宏：ICM42688_INTF_CONFIG1。 */
#define ICM42688_PWR_MGMT0                 0x4E /**< ICM42688 寄存器地址或配置值宏：ICM42688_PWR_MGMT0。 */
#define ICM42688_GYRO_CONFIG0              0x4F /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG0。 */
#define ICM42688_ACCEL_CONFIG0             0x50 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_CONFIG0。 */
#define ICM42688_GYRO_CONFIG1              0x51 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG1。 */
#define ICM42688_GYRO_ACCEL_CONFIG0        0x52 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_ACCEL_CONFIG0。 */
#define ICM42688_ACCEL_CONFIG1             0x53 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_CONFIG1。 */
#define ICM42688_TMST_CONFIG               0x54 /**< ICM42688 寄存器地址或配置值宏：ICM42688_TMST_CONFIG。 */
#define ICM42688_APEX_CONFIG0              0x56 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_CONFIG0。 */
#define ICM42688_SMD_CONFIG                0x57 /**< ICM42688 寄存器地址或配置值宏：ICM42688_SMD_CONFIG。 */
#define ICM42688_FIFO_CONFIG1              0x5F /**< ICM42688 寄存器地址或配置值宏：ICM42688_FIFO_CONFIG1。 */
#define ICM42688_FIFO_CONFIG2              0x60 /**< ICM42688 寄存器地址或配置值宏：ICM42688_FIFO_CONFIG2。 */
#define ICM42688_FIFO_CONFIG3              0x61 /**< ICM42688 寄存器地址或配置值宏：ICM42688_FIFO_CONFIG3。 */
#define ICM42688_FSYNC_CONFIG              0x62 /**< ICM42688 寄存器地址或配置值宏：ICM42688_FSYNC_CONFIG。 */
#define ICM42688_INT_CONFIG0               0x63 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_CONFIG0。 */
#define ICM42688_INT_CONFIG1               0x64 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_CONFIG1。 */
#define ICM42688_INT_SOURCE0               0x65 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_SOURCE0。 */
#define ICM42688_INT_SOURCE1               0x66 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_SOURCE1。 */
#define ICM42688_INT_SOURCE3               0x68 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_SOURCE3。 */
#define ICM42688_INT_SOURCE4               0x69 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_SOURCE4。 */
#define ICM42688_FIFO_LOST_PKT0            0x6C /**< ICM42688 寄存器地址或配置值宏：ICM42688_FIFO_LOST_PKT0。 */
#define ICM42688_FIFO_LOST_PKT1            0x6D /**< ICM42688 寄存器地址或配置值宏：ICM42688_FIFO_LOST_PKT1。 */
#define ICM42688_SELF_TEST_CONFIG          0x70 /**< ICM42688 寄存器地址或配置值宏：ICM42688_SELF_TEST_CONFIG。 */
#define ICM42688_WHO_AM_I                  0x75 /**< ICM42688 寄存器地址或配置值宏：ICM42688_WHO_AM_I。 */
#define ICM42688_REG_BANK_SEL              0x76 /**< ICM42688 寄存器地址或配置值宏：ICM42688_REG_BANK_SEL。 */

// Bank 1
#define ICM42688_SENSOR_CONFIG0            0x03 /**< ICM42688 寄存器地址或配置值宏：ICM42688_SENSOR_CONFIG0。 */
#define ICM42688_GYRO_CONFIG_STATIC2       0x0B /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG_STATIC2。 */
#define ICM42688_GYRO_CONFIG_STATIC3       0x0C /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG_STATIC3。 */
#define ICM42688_GYRO_CONFIG_STATIC4       0x0D /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG_STATIC4。 */
#define ICM42688_GYRO_CONFIG_STATIC5       0x0E /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG_STATIC5。 */
#define ICM42688_GYRO_CONFIG_STATIC6       0x0F /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG_STATIC6。 */
#define ICM42688_GYRO_CONFIG_STATIC7       0x10 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG_STATIC7。 */
#define ICM42688_GYRO_CONFIG_STATIC8       0x11 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG_STATIC8。 */
#define ICM42688_GYRO_CONFIG_STATIC9       0x12 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG_STATIC9。 */
#define ICM42688_GYRO_CONFIG_STATIC10      0x13 /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_CONFIG_STATIC10。 */
#define ICM42688_XG_ST_DATA                0x5F /**< ICM42688 寄存器地址或配置值宏：ICM42688_XG_ST_DATA。 */
#define ICM42688_YG_ST_DATA                0x60 /**< ICM42688 寄存器地址或配置值宏：ICM42688_YG_ST_DATA。 */
#define ICM42688_ZG_ST_DATA                0x61 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ZG_ST_DATA。 */
#define ICM42688_TMSTVAL0                  0x62 /**< ICM42688 寄存器地址或配置值宏：ICM42688_TMSTVAL0。 */
#define ICM42688_TMSTVAL1                  0x63 /**< ICM42688 寄存器地址或配置值宏：ICM42688_TMSTVAL1。 */
#define ICM42688_TMSTVAL2                  0x64 /**< ICM42688 寄存器地址或配置值宏：ICM42688_TMSTVAL2。 */
#define ICM42688_INTF_CONFIG4              0x7A /**< ICM42688 寄存器地址或配置值宏：ICM42688_INTF_CONFIG4。 */
#define ICM42688_INTF_CONFIG5              0x7B /**< ICM42688 寄存器地址或配置值宏：ICM42688_INTF_CONFIG5。 */
#define ICM42688_INTF_CONFIG6              0x7C /**< ICM42688 寄存器地址或配置值宏：ICM42688_INTF_CONFIG6。 */

// Bank 2
#define ICM42688_ACCEL_CONFIG_STATIC2      0x03 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_CONFIG_STATIC2。 */
#define ICM42688_ACCEL_CONFIG_STATIC3      0x04 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_CONFIG_STATIC3。 */
#define ICM42688_ACCEL_CONFIG_STATIC4      0x05 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_CONFIG_STATIC4。 */
#define ICM42688_XA_ST_DATA                0x3B /**< ICM42688 寄存器地址或配置值宏：ICM42688_XA_ST_DATA。 */
#define ICM42688_YA_ST_DATA                0x3C /**< ICM42688 寄存器地址或配置值宏：ICM42688_YA_ST_DATA。 */
#define ICM42688_ZA_ST_DATA                0x3D /**< ICM42688 寄存器地址或配置值宏：ICM42688_ZA_ST_DATA。 */

// Bank 4
#define ICM42688_GYRO_ON_OFF_CONFIG        0x0E /**< ICM42688 寄存器地址或配置值宏：ICM42688_GYRO_ON_OFF_CONFIG。 */
#define ICM42688_APEX_CONFIG1              0x40 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_CONFIG1。 */
#define ICM42688_APEX_CONFIG2              0x41 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_CONFIG2。 */
#define ICM42688_APEX_CONFIG3              0x42 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_CONFIG3。 */
#define ICM42688_APEX_CONFIG4              0x43 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_CONFIG4。 */
#define ICM42688_APEX_CONFIG5              0x44 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_CONFIG5。 */
#define ICM42688_APEX_CONFIG6              0x45 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_CONFIG6。 */
#define ICM42688_APEX_CONFIG7              0x46 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_CONFIG7。 */
#define ICM42688_APEX_CONFIG8              0x47 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_CONFIG8。 */
#define ICM42688_APEX_CONFIG9              0x48 /**< ICM42688 寄存器地址或配置值宏：ICM42688_APEX_CONFIG9。 */
#define ICM42688_ACCEL_WOM_X_THR           0x4A /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_WOM_X_THR。 */
#define ICM42688_ACCEL_WOM_Y_THR           0x4B /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_WOM_Y_THR。 */
#define ICM42688_ACCEL_WOM_Z_THR           0x4C /**< ICM42688 寄存器地址或配置值宏：ICM42688_ACCEL_WOM_Z_THR。 */
#define ICM42688_INT_SOURCE6               0x4D /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_SOURCE6。 */
#define ICM42688_INT_SOURCE7               0x4E /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_SOURCE7。 */
#define ICM42688_INT_SOURCE8               0x4F /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_SOURCE8。 */
#define ICM42688_INT_SOURCE9               0x50 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_SOURCE9。 */
#define ICM42688_INT_SOURCE10              0x51 /**< ICM42688 寄存器地址或配置值宏：ICM42688_INT_SOURCE10。 */
#define ICM42688_OFFSET_USER0              0x77 /**< ICM42688 寄存器地址或配置值宏：ICM42688_OFFSET_USER0。 */
#define ICM42688_OFFSET_USER1              0x78 /**< ICM42688 寄存器地址或配置值宏：ICM42688_OFFSET_USER1。 */
#define ICM42688_OFFSET_USER2              0x79 /**< ICM42688 寄存器地址或配置值宏：ICM42688_OFFSET_USER2。 */
#define ICM42688_OFFSET_USER3              0x7A /**< ICM42688 寄存器地址或配置值宏：ICM42688_OFFSET_USER3。 */
#define ICM42688_OFFSET_USER4              0x7B /**< ICM42688 寄存器地址或配置值宏：ICM42688_OFFSET_USER4。 */
#define ICM42688_OFFSET_USER5              0x7C /**< ICM42688 寄存器地址或配置值宏：ICM42688_OFFSET_USER5。 */
#define ICM42688_OFFSET_USER6              0x7D /**< ICM42688 寄存器地址或配置值宏：ICM42688_OFFSET_USER6。 */
#define ICM42688_OFFSET_USER7              0x7E /**< ICM42688 寄存器地址或配置值宏：ICM42688_OFFSET_USER7。 */
#define ICM42688_OFFSET_USER8              0x7F /**< ICM42688 寄存器地址或配置值宏：ICM42688_OFFSET_USER8。 */

#define ICM42688_ADDRESS                   0x69   // Address of ICM42688 accel/gyro when ADO = HIGH


#define AFS_2G  0x03 /**< AFS_2G 宏定义。 */
#define AFS_4G  0x02 /**< AFS_4G 宏定义。 */
#define AFS_8G  0x01 /**< AFS_8G 宏定义。 */
#define AFS_16G 0x00  // default

#define GFS_2000DPS   0x00 // default
#define GFS_1000DPS   0x01 /**< GFS_1000DPS 宏定义。 */
#define GFS_500DPS    0x02 /**< GFS_500DPS 宏定义。 */
#define GFS_250DPS    0x03 /**< GFS_250DPS 宏定义。 */
#define GFS_125DPS    0x04 /**< GFS_125DPS 宏定义。 */
#define GFS_62_5DPS   0x05 /**< GFS_62_5DPS 宏定义。 */
#define GFS_31_25DPS  0x06 /**< GFS_31_25DPS 宏定义。 */
#define GFS_15_125DPS 0x07 /**< GFS_15_125DPS 宏定义。 */

#define AODR_8000Hz   0x03 /**< AODR_8000Hz 宏定义。 */
#define AODR_4000Hz   0x04 /**< AODR_4000Hz 宏定义。 */
#define AODR_2000Hz   0x05 /**< AODR_2000Hz 宏定义。 */
#define AODR_1000Hz   0x06 // default
#define AODR_200Hz    0x07 /**< AODR_200Hz 宏定义。 */
#define AODR_100Hz    0x08 /**< AODR_100Hz 宏定义。 */
#define AODR_50Hz     0x09 /**< AODR_50Hz 宏定义。 */
#define AODR_25Hz     0x0A /**< AODR_25Hz 宏定义。 */
#define AODR_12_5Hz   0x0B /**< AODR_12_5Hz 宏定义。 */
#define AODR_6_25Hz   0x0C /**< AODR_6_25Hz 宏定义。 */
#define AODR_3_125Hz  0x0D /**< AODR_3_125Hz 宏定义。 */
#define AODR_1_5625Hz 0x0E /**< AODR_1_5625Hz 宏定义。 */
#define AODR_500Hz    0x0F /**< AODR_500Hz 宏定义。 */

#define GODR_8000Hz  0x03 /**< GODR_8000Hz 宏定义。 */
#define GODR_4000Hz  0x04 /**< GODR_4000Hz 宏定义。 */
#define GODR_2000Hz  0x05 /**< GODR_2000Hz 宏定义。 */
#define GODR_1000Hz  0x06 // default
#define GODR_200Hz   0x07 /**< GODR_200Hz 宏定义。 */
#define GODR_100Hz   0x08 /**< GODR_100Hz 宏定义。 */
#define GODR_50Hz    0x09 /**< GODR_50Hz 宏定义。 */
#define GODR_25Hz    0x0A /**< GODR_25Hz 宏定义。 */
#define GODR_12_5Hz  0x0B /**< GODR_12_5Hz 宏定义。 */
#define GODR_500Hz   0x0F /**< GODR_500Hz 宏定义。 */



#define ICM42688_ID	             0x47 /**< ICM42688 寄存器地址或配置值宏：ICM42688_ID。 */


/**
 * @brief ICM42688 原始三轴数据。
 * @note 直接来自传感器寄存器，尚未按量程分辨率换算成物理量。
 */
typedef struct {
  int16_t x; /**< x 轴原始 ADC/寄存器值。 */
  int16_t y; /**< y 轴原始 ADC/寄存器值。 */
  int16_t z; /**< z 轴原始 ADC/寄存器值。 */
} icm42688RawData_t;



/**
 * @brief ICM42688 换算后三轴物理量数据。
 * @note 加速度和角速度读取函数会按当前量程把原始值转换到 float。
 */
typedef struct {
  float x; /**< x 轴物理量。 */
  float y; /**< y 轴物理量。 */
  float z; /**< z 轴物理量。 */
} icm42688RealData_t;


#define	SPI_IMU_CS PAout(2)  //选中IMU	
//--------------------------------------------------------//
/**
 * @brief 执行 bsp  Icm42688 Init 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回执行结果或状态。
 */
int8_t bsp_Icm42688Init(void);
int8_t bsp_IcmGetTemperature(int16_t* pTemp);
int8_t bsp_IcmGetAccelerometer(icm42688RawData_t *accData);
int8_t bsp_IcmGetGyroscope(icm42688RawData_t *GyroData);
/**
 * @brief 执行 bsp  Icm Get Raw Data 功能。
 * @param accData accData 参数。
 * @param GyroData GyroData 参数。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回执行结果或状态。
 */
int8_t bsp_IcmGetRawData(icm42688RealData_t* accData, icm42688RealData_t* GyroData);
uint8_t bsp_Icm42688GetLastI2cStatus(void);
uint8_t bsp_Icm42688GetWhoAmI(void);
/**
 * @brief 执行 bsp  Icm42688 Probe Address 功能。
 * @param address address 参数。
 * @param whoAmI whoAmI 参数。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t bsp_Icm42688ProbeAddress(uint8_t address, uint8_t *whoAmI);

#endif
