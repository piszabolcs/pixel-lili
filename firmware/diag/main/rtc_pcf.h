/* rtc_pcf.h - the PCF85063 (I2C 0x51, on VCC-RTC: it runs through a PMIC
 * power-off). Boot: the system clock from it when it holds a plausible time;
 * `settime 2026-10-18T21:05:00` writes it (spec 5.6, M1.8). */
#ifndef RTC_PCF_H
#define RTC_PCF_H
#include <stdbool.h>
#include <time.h>
#include "driver/i2c_master.h"
bool pcf_rtc_init(i2c_master_bus_handle_t bus);         /* false = no chip */
bool pcf_rtc_read(struct tm *t, bool *valid);           /* valid = the oscillator-stop flag is clear */
bool pcf_rtc_write(const struct tm *t);                 /* also sets the system clock */
bool pcf_rtc_set_iso(const char *s);                    /* "YYYY-MM-DDTHH:MM:SS" (a space works too) */
void pcf_rtc_log(void);                                 /* the chip's time, the system's, the flags */
#endif
