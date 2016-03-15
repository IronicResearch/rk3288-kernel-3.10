/*
 * AXP  suspend Control Abstraction
 *
 * Copyright (C) RK Company
 *
 */
#ifndef _RK_AXP_SUSPEND_H
#define _RK_AXP_SUSPEND_H
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/notifier.h>

struct  axp_device{
	struct notifier_block fb_notif;
	int(*axp_suspend)(void);
	int(*axp_resume)(void);
	struct mutex ops_lock;
};

static inline int fb_notifier_callback(struct notifier_block *self,
				unsigned long action, void *data)
{
	struct axp_device *axp;
	struct fb_event *event = data;
	int blank_mode = *((int *)event->data);
	int ret = 0;

	axp = container_of(self, struct axp_device, fb_notif);

	//printk("%s.....lin=%d axp->status=%x,blank_mode=%x\n",__func__,__LINE__,axp->status,blank_mode);

	mutex_lock(&axp->ops_lock);

	if (action == FB_EARLY_EVENT_BLANK) {
		switch (blank_mode) {
		case FB_BLANK_UNBLANK:
			break;
		default:
			ret = axp->axp_suspend();
			break;
		}
	}
	else if (action == FB_EVENT_BLANK) {
		switch (blank_mode) {
		case FB_BLANK_UNBLANK:
			axp->axp_resume();
			break;
		default:
			break;
		}
	}
	mutex_unlock(&axp->ops_lock);

	if (ret < 0)
	{
		printk("AXP_notifier_callback error action=%x,blank_mode=%x\n",(int)action,blank_mode);
		return ret;
	}

	return NOTIFY_OK;
}

static inline int axp_register_fb(struct axp_device *axp)
{
	memset(&axp->fb_notif, 0, sizeof(axp->fb_notif));
	axp->fb_notif.notifier_call = fb_notifier_callback;
	mutex_init(&axp->ops_lock);

	return fb_register_client(&axp->fb_notif);
}

static inline void axp_unregister_fb(struct axp_device *axp)
{
	fb_unregister_client(&axp->fb_notif);
}
#endif
