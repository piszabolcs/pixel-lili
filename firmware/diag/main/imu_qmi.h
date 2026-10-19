/* imu_qmi.h - the QMI8658 (I2C 0x6B) for the diag: the live vector, and the
 * Wake-on-Motion whose INT1 is GPIO 17 on the 2.16 (the schematic; INT2 is
 * GPIO 21) - the M1.4 question. Accel only, gyro never on. */
#ifndef IMU_QMI_H
#define IMU_QMI_H
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#define IMU_INT1_GPIO 17
#define IMU_INT2_GPIO 21
bool imu_init(i2c_master_bus_handle_t bus);        /* soft reset, +-2 g at 31.25 Hz */
bool imu_read(int16_t a[3]);                       /* raw counts, 16384 / g */
/* WoM: the accel in its low-power 21 Hz mode, INT1 idle low, high on a change of
 * more than thr_mg (1..255, mg) after `blank` samples of settling; the pin stays
 * high until STATUS1 is read (imu_status1). */
bool imu_wom_arm(int thr_mg, int blank);
bool imu_wom_disarm(void);                         /* back to the live 31.25 Hz mode, INT1 off */
bool imu_wom_armed(void);
uint8_t imu_status1(void);                         /* reads (and clears) STATUS1: bit 2 = WoM */
uint8_t imu_statusint(void);
void imu_sleep(void);                              /* sensors off (the pocket-tank's drowse) */
#endif
