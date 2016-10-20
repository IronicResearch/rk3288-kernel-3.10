#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/adc.h>
#include <linux/slab.h>

#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/iio/consumer.h>

#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

#define DELAY_JIFFIES  5

#define IGNORE  0;

static int  irq_num = -1;


struct vb_keys_button {
	int keyboard_left;
	int keyboard_right;
	int left_first;
	int right_first;
};

struct vb_switch_desc{
	int gpio;
	int irq;
	int flags;
	int code;
	char *lable;
};

struct vb_switch_platform_data {
	struct vb_switch_desc switch_desc[6];
	int num;
};

struct vb_switch_drvdata {	
	bool is_suspend;
	int num;
	int time_state;
	struct vb_switch_desc switch_desc[6];
	struct input_dev *input;
	struct work_struct work;
	struct workqueue_struct *wq;
	struct timer_list timer;
	struct vb_keys_button button;
	//spinlock_t vb_lock;
};

static int vb_switch_parse_dt(struct platform_device *pdev,
		struct vb_switch_drvdata *ddata)
{	

	// printk("%s ======== irq =%d\n",__func__,iq
	struct device_node *node = pdev->dev.of_node;
	struct device_node *child_node;
	int ret, gpio, i =0;
	u32 code;
	enum of_gpio_flags flags;
	
	//up
	gpio = of_get_named_gpio_flags(node, "chan_a", 0, &flags);
	if (gpio_is_valid(gpio)){
		ddata->switch_desc[0].gpio = gpio;
		ddata->switch_desc[0].code = 103;
		ddata->switch_desc[0].lable = "up";
	} else{
		ddata->switch_desc[0].gpio = -1;
	}

	//down
	gpio = of_get_named_gpio_flags(node, "chan_b", 0, &flags);
	if (gpio_is_valid(gpio)){
		ddata->switch_desc[1].gpio = gpio;
		ddata->switch_desc[1].code = 108;
		ddata->switch_desc[1].lable = "down";
	} else {
		ddata->switch_desc[1].gpio = -1;
	}

	//enter
	
	gpio = of_get_named_gpio_flags(node, "chan_k", 0, &flags);
	if (gpio_is_valid(gpio)){
		ddata->switch_desc[2].gpio = gpio;
		ddata->switch_desc[2].code = 28;
		ddata->switch_desc[2].lable = "enter";
	} else {
		ddata->switch_desc[2].gpio = -1;
	}

	//left
	gpio = of_get_named_gpio_flags(node, "scrl_a", 0, &flags);
	if (gpio_is_valid(gpio)){
		ddata->switch_desc[3].gpio = gpio;
		ddata->switch_desc[3].code = 105;
		ddata->switch_desc[3].lable = "left";
	} else {
		ddata->switch_desc[3].gpio = -1;
	}

	//right
	gpio = of_get_named_gpio_flags(node, "scrl_b", 0, &flags);
	if (gpio_is_valid(gpio)){
		ddata->switch_desc[4].gpio = gpio;
		ddata->switch_desc[4].code = 106;
		ddata->switch_desc[4].lable = "right";
	} else {
		ddata->switch_desc[4].gpio = -1;
	}

	//back
	gpio = of_get_named_gpio_flags(node, "scrl_k", 0, &flags);
	if (gpio_is_valid(gpio)){
		ddata->switch_desc[5].gpio = gpio;
		ddata->switch_desc[5].code = 158;
		ddata->switch_desc[5].lable = "back";
	} else {
		ddata->switch_desc[5].gpio = -1;
	}
	
	return 0;

}

int left_flag = -1,right_flag = -1,up_flag = -1,down_flag = -1;
static int  vc_irq_worker(struct work_struct *work)
{
	printk("%s................\n",__func__);
	struct vb_switch_drvdata *ddata = container_of(work,struct vb_switch_drvdata ,work);
	int i = irq_num;
	 if ((i == 3) && (right_flag == 0)){
		if (!gpio_get_value(ddata->switch_desc[irq_num].gpio)){
				left_flag = -1;//clear flag
				right_flag = -1;//
				input_report_key(ddata->input,ddata->switch_desc[i].code,1); //left
				input_sync(ddata->input);
				input_report_key(ddata->input, ddata->switch_desc[i].code, 0);
				input_sync(ddata->input);
		}
	} else if ((i == 3)) {//scrl_a
		if (!gpio_get_value(ddata->switch_desc[irq_num].gpio))
			left_flag = 0;//开始右旋,scrl a 先低
	}

	if ((i == 4) && (left_flag == 0)){//scrl a 低的情况下
		if (!gpio_get_value(ddata->switch_desc[irq_num].gpio)){
				right_flag = -1;//clear flag
				left_flag = -1;
				input_report_key(ddata->input,ddata->switch_desc[i].code,1); //right 
				input_sync(ddata->input);
				input_report_key(ddata->input, ddata->switch_desc[i].code, 0);
				input_sync(ddata->input);
		}
	}else if ((i == 4)){ //scrl_b
		if (!gpio_get_value(ddata->switch_desc[irq_num].gpio))
			right_flag = 0;//左旋 scrl b 先低 
	}

/*****************************************************************************************************************/

	 if ((i == 0) && (down_flag == 0)){
		if (!gpio_get_value(ddata->switch_desc[irq_num].gpio)){
				down_flag = -1;//clear flag
				up_flag = -1;//
				input_report_key(ddata->input,ddata->switch_desc[i].code,1); //up
				input_sync(ddata->input);
				input_report_key(ddata->input, ddata->switch_desc[i].code, 0);
				input_sync(ddata->input);
		}
	} else if ((i == 0)) {//chan_a
		if (!gpio_get_value(ddata->switch_desc[irq_num].gpio))
			up_flag = 0;//开始右旋,chan a 先低
	}

	 if ((i == 1) && (up_flag == 0)){
		if (!gpio_get_value(ddata->switch_desc[irq_num].gpio)){
				down_flag = -1;//clear flag
				up_flag = -1;//
				input_report_key(ddata->input,ddata->switch_desc[i].code,1); //up
				input_sync(ddata->input);
				input_report_key(ddata->input, ddata->switch_desc[i].code, 0);
				input_sync(ddata->input);
		}
	} else if ((i == 1)) {//chan_b
		if (!gpio_get_value(ddata->switch_desc[irq_num].gpio))
			down_flag = 0;//开始右旋,chan b 先低
	}

/*************************************************************************************************************************/

	if (i == 2){
		if (!gpio_get_value(ddata->switch_desc[irq_num].gpio)){
				input_report_key(ddata->input,ddata->switch_desc[i].code,1); //up
				input_sync(ddata->input);
				input_report_key(ddata->input, ddata->switch_desc[i].code, 0);
				input_sync(ddata->input);
		}
	}



	if (i == 5){
		if (!gpio_get_value(ddata->switch_desc[irq_num].gpio)){
				input_report_key(ddata->input,ddata->switch_desc[i].code,1); //up
				input_sync(ddata->input);
				input_report_key(ddata->input, ddata->switch_desc[i].code, 0);
				input_sync(ddata->input);
		}
	}
/**************************************************************************************************************************/

	
	for(i = 0 ;i < 6 ; i++) {
			printk("%s  key   status %d \n",ddata->switch_desc[i].lable,gpio_get_value(ddata->switch_desc[i].gpio));
	}
	
/*
	}
	
	switch (irq_num){
		case 0 :
			printk("chan a  status %d\n",gpio_get_value(ddata->switch_desc[irq_num].gpio));
			break;
		case 1 :
			printk("chan b  status %d\n",gpio_get_value(ddata->switch_desc[irq_num].gpio));
			break;
		case 2 :
			break;
		case 3 :
			printk("scrl a status %d\n",gpio_get_value(ddata->switch_desc[irq_num].gpio));
			break;
		case 4 :
			printk("scrl b status %d\n",gpio_get_value(ddata->switch_desc[irq_num].gpio));
			break;
		case 5 :
			break;
	
	}
	*/
}	

static irqreturn_t  switch_irq(int irq,void *dev_id)
{
	struct vb_switch_drvdata *ddata = dev_id;

	unsigned int code,ret,i;

	if(ddata->is_suspend == true)
		return IRQ_HANDLED;

	for(i = 0 ;i < 6 ; i++) {
		if (irq == ddata->switch_desc[i].irq){
			irq_num = i;
			break;
		}
	}
	schedule_work(&ddata->work);
	return IRQ_HANDLED;

}

static  int  vb_switch_probe(struct platform_device *pdev)
{

	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct vb_switch_drvdata *ddata;
	struct input_dev *input; 
	int i,irq,error = 0;
	int ret = 0;
	int gpio1_value,gpio2_value;
	int st;

	ddata = devm_kzalloc(dev,sizeof(struct vb_switch_drvdata),GFP_KERNEL);
	input = input_allocate_device();
	if (!ddata || !input) {
		ret = -ENOMEM;
		goto error1;
	}  
	platform_set_drvdata(pdev, ddata);

	input->name = "rk30-vb-switch";
	input->phys = "gpio-switch/input4";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	ddata->input = input;
	//ddata->switch_desc = switch_desc;
	ddata->num = 2;
	ddata->is_suspend = false;

	//	set_bit(EV_KEY, input->evbit);
	error = vb_switch_parse_dt(pdev, ddata);

	INIT_WORK(&ddata->work, vc_irq_worker);


	for(i = 0 ;i < 6 ; i++) {
		input_set_capability(ddata->input, EV_KEY, ddata->switch_desc[i].code);
		//printk("%s gpio is %d,irq is %d code is %d\n",ddata->switch_desc[i].lable,ddata->switch_desc[i].gpio,irq,ddata->switch_desc[i].code);
		
		ret = devm_gpio_request(dev,ddata->switch_desc[i].gpio,"ddata->switch_desc[i].lable");
		if(ret < 0) {
			printk("request %s gpio failed\n",ddata->switch_desc[i].lable);
		}

		ret = gpio_direction_input(ddata->switch_desc[i].gpio);
	//	printk("%s gpio init status %d\n",ddata->switch_desc[i].lable,gpio_get_value(ddata->switch_desc[i].gpio));
		if(ret <0) {
			printk("direction input  %s gpio failed\n",ddata->switch_desc[i].lable);
		}

		irq = gpio_to_irq(ddata->switch_desc[i].gpio);
		ddata->switch_desc[i].irq = irq;
		if(irq < 0) {
			printk("%s get irq failed\n",ddata->switch_desc[i].lable);
			gpio_free(ddata->switch_desc[i].gpio);
			goto error2;
		}  
		printk("%s gpio is %d,irq is %d code is %d\n",ddata->switch_desc[i].lable,ddata->switch_desc[i].gpio,irq,ddata->switch_desc[i].code);

		ret = request_irq(irq,switch_irq,IRQF_TRIGGER_FALLING /*| IRQF_TRIGGER_RISING*/,"vb",ddata);
		//ret = request_irq(irq,switch_irq,IRQF_TRIGGER_LOW,"vb",ddata);
		if(ret < 0) {
			printk("%s claim irq failed\n",ddata->switch_desc[i].lable);
			free_irq(irq, ddata);
			goto error2;
		}
	}

	ret = input_register_device(ddata->input);
	if(ret) {
		printk("register input device failed %d \n",ret);
		goto error3;
	}
	return 0;
error3:
	input_free_device(ddata->input);
error2:
error1:
	printk("allocate failed\n");

	return ret;

}

static int  vb_switch_remove(struct platform_device *pdev)
{
	/*
	   struct vb_switch_platform_data *pdata = pdev->dev.platform_data;
	   struct vb_switch_drvdata *ddata = platform_get_drvdata(pdev);
	   int irq;
	   device_init_wakeup(&pdev->dev, 0);
	   irq = gpio_to_irq(pdata->gpio);
	   free_irq(irq, ddata);
	   cancle_work_sync(ddata->work);
	   gpio_free(pdata->gpio);

	   input_unregister_device(ddata->input);

*/
	return 0;
}

#ifdef CONFIG_PM
static const int switch_suspend(struct device *dev)
{
	return 0;
}
static const int switch_resume(struct device *dev)
{
	return 0;
}

static  const struct dev_pm_ops keys_pm_ops = {
	.suspend	= switch_suspend,
	.resume		= switch_resume,
};
#endif

static const struct of_device_id vbswitch_match[] = {
	{ .compatible = "rockchip,vbswitch", .data = NULL},
	{},
};

static struct platform_driver vb_switch_driver = {
	.probe		= vb_switch_probe,
	.remove		= vb_switch_remove,
	.driver		= {
		.name	= "rk30-vb-switch",
		.owner	= THIS_MODULE,
		.of_match_table = vbswitch_match,
#ifdef CONFIG_PM
		.pm	= &keys_pm_ops,
#endif
	}
};

module_platform_driver(vb_switch_driver);
MODULE_LICENSE("GPL");
