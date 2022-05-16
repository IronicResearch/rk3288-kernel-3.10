/*
 * Base driver for X-Powers AXP
 *
 * Copyright (C) 2013 X-Powers, Ltd.
 *  Zhang Donglu <zhangdonglu@x-powers.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
//#include <mach/irqs.h>
#include <linux/power_supply.h>
#include <linux/apm_bios.h>
#include <linux/apm-emulation.h>
#include <linux/module.h>

#include "axp-cfg.h"
#include "axp-mfd.h"
//#include "axp809-regu.h"

/* Reverse engineered partly from Platformx drivers */
enum axp_regls{

	rtcldo,
	aldo1,
	aldo2,
	aldo3,
	dldo1,
	dldo2,
	eldo1,
	eldo2,
	eldo3,
	dc5ldo,

	dcdc1,
	dcdc2,
	dcdc3,
	dcdc4,
	dcdc5,
	gpioldo0,
	gpioldo1,
	sw0,
};

/* The values of the various regulator constraints are obviously dependent
 * on exactly what is wired to each ldo.  Unfortunately this information is
 * not generally available.  More information has been requested from Xbow
 * but as of yet they haven't been forthcoming.
 *
 * Some of these are clearly Stargate 2 related (no way of plugging
 * in an lcd on the IM2 for example!).
 */

static struct regulator_consumer_supply rtcldo_data[] = {
		{
			.supply = "rtcldo",
		},
	};


static struct regulator_consumer_supply aldo1_data[] = {
		{
			.supply = "vcc_18",    ///vcc_18
		},
	};

static struct regulator_consumer_supply aldo2_data[] = {
		{
			.supply = "aldo2",   ///vdd_10
		},
	};

static struct regulator_consumer_supply aaldo2_data[] = {
		{
			.supply = "aldo3",   ///vcca_codec 3.3v
		},
	};

static struct regulator_consumer_supply dldo1_data[] = {
		{
			.supply = "dldo1",  //vcc_sd 3.0v
		},
	};


static struct regulator_consumer_supply dldo2_data[] = {
		{
			.supply = "dldo2", //dldo2 1.8v
		},
	};

static struct regulator_consumer_supply eldo1_data[] = {
		{
			.supply = "eldo1",  //vccio_pmu 3.3v
		},
	};


static struct regulator_consumer_supply eldo2_data[] = {
		{
			.supply = "eldo2",  //vcc_tp 3.3v
		},
	};

static struct regulator_consumer_supply ealdo2_data[] = {
		{
			.supply = "eldo3",   //vccio_sd 3.0v
		},
	};

static struct regulator_consumer_supply dc5ldo_data[] = {
		{
			.supply = "dc5ldo",    //DC5LDO 1.2v
		},
	};

static struct regulator_consumer_supply gpioldo0_data[] = {
		{
			.supply = "gpio_ldo0",   //VDD10_LCD 1.0v
		},
	};

static struct regulator_consumer_supply gpioldo1_data[] = {
		{
			.supply = "gpio_ldo1",   //VCC18_LCD 1.8V
		},
	};

static struct regulator_consumer_supply dcdc1_data[] = {
		{
			.supply = "xxxSdcdc1",     //vcc_io  3.3v
		},
	};

static struct regulator_consumer_supply dcdc2_data[] = {
		{
			.supply = "vdd_arm",   //vdd_cpu_fb vcc_20 2.0v
		},
	};

static struct regulator_consumer_supply dcdc3_data[] = {
		{
			.supply = "dcdc3",    //vdd_cpu_fb vccio_lcd  3.3v
		},
	};

static struct regulator_consumer_supply dcdc4_data[] = {
		{
			.supply = "dcdc4",    //vcclog 1.0v   x
		},
	};

static struct regulator_consumer_supply dcdc5_data[] = {
		{
			//.supply = "DCDC5",   //vcc_ddr  1.2v
			.supply = "xxdcdc5",
		},
	};

static struct regulator_consumer_supply sw0_data[] = {
		{
			.supply = "sw0",  
		},
	};

static struct regulator_init_data axp_regl_init_data[] = {
	[rtcldo] = {
		.constraints = {
			.name = "rtcldo",
			.min_uV =  AXP_LDO1_MIN,
			.max_uV =  AXP_LDO1_MAX,
		},
		.num_consumer_supplies = ARRAY_SIZE(rtcldo_data),
		.consumer_supplies = rtcldo_data,
	},
	[aldo1] = {
		.constraints = {
			.name = "ALDO1",
			.min_uV = AXP_LDO2_MIN,
			.max_uV = AXP_LDO2_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_LDO2_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(aldo1_data),
		.consumer_supplies = aldo1_data,
	},
	[aldo2] = {
		.constraints = {
			.name = "ALDO2",
			.min_uV =  AXP_LDO3_MIN,
			.max_uV =  AXP_LDO3_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_LDO3_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(aldo2_data),
		.consumer_supplies = aldo2_data,
	},
	[aldo3] = {
		.constraints = {
			.name = "axp_aldo3",
			.min_uV = AXP_LDO4_MIN,
			.max_uV = AXP_LDO4_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_LDO4_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(aaldo2_data),
		.consumer_supplies = aaldo2_data,
	},
	[dldo1] = {
		.constraints = {
			.name = "axp_dldo1",
			.min_uV = AXP_LDO5_MIN,
			.max_uV = AXP_LDO5_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_LDO5_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(dldo1_data),
		.consumer_supplies = dldo1_data,
	},
	[dldo2] = {
		.constraints = {
			.name = "axp_dldo2",
			.min_uV = AXP_LDO6_MIN,
			.max_uV = AXP_LDO6_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_LDO6_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(dldo2_data),
		.consumer_supplies = dldo2_data,
	},
	[eldo1] = {
		.constraints = {
			.name = "axp_eldo1",
			.min_uV = AXP_LDO9_MIN,
			.max_uV = AXP_LDO9_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_LDO9_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(eldo1_data),
		.consumer_supplies = eldo1_data,
	},
	[eldo2] = {
		.constraints = {
			.name = "axp_eldo2",
			.min_uV = AXP_LDO10_MIN,
			.max_uV = AXP_LDO10_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_LDO10_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(eldo2_data),
		.consumer_supplies = eldo2_data,
	},
	[eldo3] = {
		.constraints = {
			.name = "axp_eldo3",
			.min_uV =  AXP_LDO11_MIN,
			.max_uV =  AXP_LDO11_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_LDO11_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(ealdo2_data),
		.consumer_supplies = ealdo2_data,
	},
	[dc5ldo] = {
		.constraints = {
			.name = "DC5LDO",
			.min_uV = AXP_LDO12_MIN,
			.max_uV = AXP_LDO12_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_LDO12_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(dc5ldo_data),
		.consumer_supplies = dc5ldo_data,
	},
	[dcdc1] = {
		.constraints = {
			.name = "DCDC1",
			.min_uV = AXP_DCDC1_MIN,
			.max_uV = AXP_DCDC1_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_DCDC1_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(dcdc1_data),
		.consumer_supplies = dcdc1_data,
	},
	[dcdc2] = {
		.constraints = {
			.name = "DCDC2",
			.min_uV = AXP_DCDC2_MIN,
			.max_uV = AXP_DCDC2_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_DCDC2_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(dcdc2_data),
		.consumer_supplies = dcdc2_data,
	},
	[dcdc3] = {
		.constraints = {
			.name = "DCDC3",
			.min_uV = AXP_DCDC3_MIN,
			.max_uV = AXP_DCDC3_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_DCDC3_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(dcdc3_data),
		.consumer_supplies = dcdc3_data,
	},
	[dcdc4] = {
		.constraints = {
			.name = "DCDC4",
			.min_uV = AXP_DCDC4_MIN,
			.max_uV = AXP_DCDC4_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_DCDC4_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(dcdc4_data),
		.consumer_supplies = dcdc4_data,
	},
	[dcdc5] = {
		.constraints = {
			.name = "DCDC5",
			.min_uV = AXP_DCDC5_MIN,
			.max_uV = AXP_DCDC5_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
#if 0//defined (CONFIG_KP_OUTPUTINIT)
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				.uV = AXP_DCDC5_VALUE * 1000,
				.enabled = 1,
			}
#endif
		},
		.num_consumer_supplies = ARRAY_SIZE(dcdc5_data),
		.consumer_supplies = dcdc5_data,
	},
	[gpioldo0] = {
		.constraints = {
			.name = "GPIO_LDO0",
			.min_uV = GPIO_LDO0_MIN,
			.max_uV = GPIO_LDO0_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(gpioldo0_data),
		.consumer_supplies = gpioldo0_data,
	},
	[gpioldo1] = {
		.constraints = {
			.name = "GPIO_LDO1",
			.min_uV = GPIO_LDO1_MIN,
			.max_uV = GPIO_LDO1_MAX,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(gpioldo1_data),
		.consumer_supplies = gpioldo1_data,
	},
	[sw0] = {
		.constraints = {
			.name = "SW0",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(sw0_data),
		.consumer_supplies = sw0_data,
	},
};

static struct axp_funcdev_info axp_regldevs[] = {
	{
		.name = "axp809-regulator",
		.id = AXP_ID_LDO1,
		.platform_data = &axp_regl_init_data[rtcldo],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDO2,
		.platform_data = &axp_regl_init_data[aldo1],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDO3,
		.platform_data = &axp_regl_init_data[aldo2],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDO4,
		.platform_data = &axp_regl_init_data[aldo3],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDO5,
		.platform_data = &axp_regl_init_data[dldo1],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDO6,
		.platform_data = &axp_regl_init_data[dldo2],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDO9,
		.platform_data = &axp_regl_init_data[eldo1],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDO10,
		.platform_data = &axp_regl_init_data[eldo2],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDO11,
		.platform_data = &axp_regl_init_data[eldo3],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDO12,
		.platform_data = &axp_regl_init_data[dc5ldo],
	}, 
	{
		.name = "axp809-regulator",
		.id = AXP_ID_DCDC1,
		.platform_data = &axp_regl_init_data[dcdc1],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_DCDC2,
		.platform_data = &axp_regl_init_data[dcdc2],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_DCDC3,
		.platform_data = &axp_regl_init_data[dcdc3],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_DCDC4,
		.platform_data = &axp_regl_init_data[dcdc4],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_DCDC5,
		.platform_data = &axp_regl_init_data[dcdc5],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDOIO0,
		.platform_data = &axp_regl_init_data[gpioldo0],
	}, {
		.name = "axp809-regulator",
		.id = AXP_ID_LDOIO1,
		.platform_data = &axp_regl_init_data[gpioldo1],
	},
};

static struct power_supply_info battery_data ={
		.name ="PTI PL336078",
		.technology = POWER_SUPPLY_TECHNOLOGY_LION,
		.voltage_max_design = CHGVOL,
		.voltage_min_design = SHUTDOWNVOL,
		.energy_full_design = BATCAP,
		.use_for_apm = 1,
};


static struct axp_supply_init_data axp_sply_init_data = {
	.battery_info = &battery_data,
	.chgcur = STACHGCUR,
	.chgvol = CHGVOL,
	.chgend = ENDCHGRATE,
	.chgen = CHGEN,
	.sample_time = ADCFREQ,
	.chgpretime = CHGPRETIME,
	.chgcsttime = CHGCSTTIME,
};

static struct axp_funcdev_info axp_splydev[]={
   	{
   		.name = "axp809-supplyer",
		.id = AXP_ID_SUPPLY,
		.platform_data = &axp_sply_init_data,
    },
};
/*
static struct axp_funcdev_info axp_gpiodev[]={
   	{
		.name = "axp809-gpio",
   		.id = AXP_ID_GPIO,
    },
};*/

static struct axp_platform_data axp_pdata = {
	.num_regl_devs = ARRAY_SIZE(axp_regldevs),
	.num_sply_devs = ARRAY_SIZE(axp_splydev),
	//.num_gpio_devs = ARRAY_SIZE(axp_gpiodev),
	.regl_devs = axp_regldevs,
	.sply_devs = axp_splydev,
	//.gpio_devs = axp_gpiodev,
	.gpio_base = 0,
};

   /*×¢²áÉè±¸ */
static struct i2c_board_info __initdata axp_mfd_i2c_board_info[] = {
	{
		.type = "axp_mfd",
		.addr = AXP_DEVICES_ADDR,
		.platform_data = &axp_pdata,
		.irq = AXP_IRQNO,
	},
};

static int __init axp_board_init(void)
{
        return i2c_register_board_info(AXP_I2CBUS, axp_mfd_i2c_board_info, ARRAY_SIZE(axp_mfd_i2c_board_info));
}
arch_initcall(axp_board_init);

MODULE_DESCRIPTION("X-powers axp board");
MODULE_AUTHOR("Criss");
MODULE_LICENSE("GPL");
