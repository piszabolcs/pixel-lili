/* powerlog.h - the M1.5 measurement: SoC / VBAT rows stamped by the RTC clock,
 * kept in NVS (the cable is out while it measures; the rows are read back
 * when it is in again). mA between rows from the 1000 mAh cell, 1 % = 10 mAh,
 * as the pocket-tank's batlog derives it. */
#ifndef POWERLOG_H
#define POWERLOG_H
#include <stdint.h>
#include <stdbool.h>
enum { PL_ACTIVE = 0, PL_DIM = 1, PL_SLEEP = 2, PL_OFF = 3, PL_WAIT = 4 };
typedef struct { uint32_t t; uint16_t mv; uint8_t pct; uint8_t st; char why[8]; } plog_row_t;   /* st: bits 0-3 state, bit 7 VBUS present */
#define PL_ROWS 96
void plog_init(void);
void plog_add(int state, const char *why);
void plog_print(void);
void plog_clear(void);
int  plog_count(void);
const plog_row_t *plog_row(int i);                 /* i = 0 oldest */
const char *plog_state_name(int st);
#endif
