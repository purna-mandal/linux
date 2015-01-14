#ifndef __PIC32_OCMP_H__
#define __PIC32_OCMP_H__
#include <linux/types.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/clkdev.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <asm/mips-boards/generic.h>
#include <asm/mach-pic32/common.h>
#include <asm/mach-pic32/pic32.h>

/* OC Modes */
enum {
PIC32_OCM_NONE,             /* disabled, but continues to draw current */
PIC32_OCM_TRANSITION_HIGH,  /* single match mode, OCx transition LOW -> HIGH */
PIC32_OCM_TRANSITION_LOW,   /* single match mode, OCx transition HIGH -> LOW */
PIC32_OCM_TRANSITION_TOGGLE,/* single match mode, OCx toggles from INIT state */
PIC32_OCM_PULSE_ONE,          /* dual match mode, generates one pulse */
PIC32_OCM_PULSE_CONTINUOUS,   /* dual match mode, generates continuous pulse */
PIC32_OCM_PWM_DISABLED_FAULT, /* PWM mode, fault detection disabled */
PIC32_OCM_PWM_ENABLED_FAULT,  /* PWM mode, fault detection enabled */
PIC32_OCM_MAX,
};

struct pic32_ocmp;
struct pic32_pb_timer;
struct pic32_ocmp *pic32_oc_request_by_node(struct device_node *np);
int pic32_oc_free(struct pic32_ocmp *oc);
int pic32_oc_set_time_base(struct pic32_ocmp *oc, struct pic32_pb_timer *timer);
int pic32_oc_settime(struct pic32_ocmp *oc, int mode, uint64_t timeout_nsec);
int pic32_oc_start(struct pic32_ocmp *oc);
int pic32_oc_stop(struct pic32_ocmp *oc);
#endif

