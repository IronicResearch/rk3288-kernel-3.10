#include <linux/module.h>
#include <linux/delay.h>
//#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/async.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
//#include <mach/gpio.h>
#include <linux/irq.h>
//#include <mach/board.h>
//#include "IT7236_touchkey.h"
#include <linux/input/mt.h>

#define IT7236_FW_AUTO_UPGRADE     1// Upgrade Firmware form driver rawDATA[] array 1:Enable ; 0:Disable
#define IT7236_TOUCH_SLIDER_REGISTER_ADDRESS	0x00  // Change the address to read others RAM data
#define IT7236_TOUCH_PROXIMITY_REGISTER_ADDRESS	0x02  // Change the address to read others RAM data
#define DRIVER_VERSION	"1.0.0"

#if IT7236_FW_AUTO_UPGRADE
#include "IT7236_FW.h"
#endif

#include "it7236_touchkey.h"

#define IT7236_I2C_NAME "it7236_touchkey"
#define IT7236_IIC_SPEED 		200*1000

static int ite7236_major = 0;	// dynamic major by default
static int ite7236_minor = 0;
static struct cdev ite7236_cdev;
static struct class *ite7236_class = NULL;
static dev_t ite7236_dev;
static struct input_dev *input_dev;
static int fw_upgrade_success = 0;
static u8 wTemp[128] = {0x00};
static char config_id[10];
static unsigned short i2c_addr_bak;  

struct workqueue_struct *IT7236_wq;
struct delayed_work 	    it7236_delay_work;

static char it7236_fw_name[256] = "IT7236_FW";	
// Put the IT7236 firmware file at /etc/firmware/ and the file name is IT7236_FW



static int get_config_ver(void);



static int i2cReadFromIt7236(struct i2c_client *client, unsigned char bufferIndex, unsigned char dataBuffer[], unsigned short dataLength)
{
	int ret;
	struct i2c_msg msgs[2] = { {
		.addr = client->addr,
		.flags = 0,
		.scl_rate = IT7236_IIC_SPEED,
		.len = 1,
		.buf = &bufferIndex
		}, {
		.addr = client->addr,
		.flags = I2C_M_RD,
		.scl_rate = IT7236_IIC_SPEED,
		.len = dataLength,
		.buf = dataBuffer
		}
	};

	memset(dataBuffer, 0xFF, dataLength);
	ret = i2c_transfer(client->adapter, msgs, 2);

	return ret;
}

static int i2cWriteToIt7236(struct i2c_client *client, unsigned char bufferIndex, unsigned char const dataBuffer[], unsigned short dataLength)
{
	unsigned char buffer4Write[256];
	int ret, retry = 3;
	struct i2c_msg msgs[1] = { {
		.addr = client->addr,
		.flags = 0,
		.len = dataLength + 1,
		.scl_rate = IT7236_IIC_SPEED,
		.buf = buffer4Write
		}
	};

	if(dataLength < 256) {
		buffer4Write[0] = bufferIndex;
		memcpy(&(buffer4Write[1]), dataBuffer, dataLength);

		do {
			ret = i2c_transfer(client->adapter, msgs, 1);
			retry--;
		} while((ret != 1) && (retry > 0));

		if(ret != 1)
			printk("%s : i2c_transfer error\n", __func__);
		return ret;
	}
	else {
		printk("%s : i2c_transfer error , out of size\n", __func__);
		return -1;
	}
}

static bool waitCommandDone(void)
{
	unsigned char ucQuery = 0x00;
	unsigned int count = 0;

	do {
		if(!i2cReadFromIt7236(gl_ts->client, 0xFA, &ucQuery, 1))
			ucQuery = 0x00;
		count++;
	} while((ucQuery != 0x80) && (count < 10));

	if( ucQuery == 0x80)
		return  true;
	else
		return  false;
}

void WriteCommd(unsigned char Command)
{
	int count = 0;
	unsigned char pucBuffer[1];
	unsigned char data[1];
	unsigned char pucommdbuf[2]={0x01,0x00};
	pucommdbuf[1] = Command;

	//Page switch to 0
	pucBuffer[0] =0x00;
	i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);

	pucBuffer[0] =0x80;		
	i2cWriteToIt7236(gl_ts->client, 0xF1, pucBuffer, 1);
	
	do{
		i2cReadFromIt7236(gl_ts->client, 0xFA, data, 1);
		count++;
	} while( (data[0] != 0x80) && (count < 20));
	
	i2cWriteToIt7236(gl_ts->client, 0x40, pucommdbuf, 2);

	pucBuffer[0] =0x40;
	i2cWriteToIt7236(gl_ts->client, 0xF1, pucBuffer, 1);
	mdelay(200);
}




static bool fnFirmwareReinitialize(void)
{
	int count = 0;
	u8 data[1];
	char pucBuffer[1];

	pucBuffer[0] = 0x55;
	i2cWriteToIt7236(gl_ts->client, 0xFB,pucBuffer,1);
	mdelay(10);

	pucBuffer[0] = 0xFF;
	i2cWriteToIt7236(gl_ts->client, 0xF6, pucBuffer, 1);

	pucBuffer[0] = 0x64;
	i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);
	pucBuffer[0] = 0x3F;
	i2cWriteToIt7236(gl_ts->client, 0x00, pucBuffer, 1);
	pucBuffer[0] = 0x7C;
	i2cWriteToIt7236(gl_ts->client, 0x01, pucBuffer, 1);
	pucBuffer[0] = 0x00;
	i2cWriteToIt7236(gl_ts->client, 0x00, pucBuffer, 1);
	pucBuffer[0] = 0x00;
	i2cWriteToIt7236(gl_ts->client, 0x01, pucBuffer, 1);

	do{
		i2cReadFromIt7236(gl_ts->client, 0xFA, data, 1);
		count++;
	} while( (data[0] != 0x80) && (count < 20));

	pucBuffer[0] = 0x00;
	i2cWriteToIt7236(gl_ts->client, 0xF1, pucBuffer, 1);
     
    WriteCommd(0XF0); //Rest CMD
	return true;
}

static int it7236_upgrade(u8* InputBuffer, int fileSize)
{
	int i, j, k;
	int StartPage = 0;
	int registerI, registerJ;
	int page, pageres;
	int Addr;
	int nErasePage;
	int err_temp;
	u8 result = 1;
	u8 DATABuffer1[128] = {0x00};
	u8 DATABuffer2[128] = {0x00};
	u8 OutputDataBuffer[8192] = {0x00};
	u8 WriteDATABuffer[128] = {0x00};
	char pucBuffer[1];
	int retry = 0;
	int ret1, ret2;

	disable_irq_nosync(gl_ts->client->irq);

	printk("[IT7236] Start Upgrade Firmware \n");
	pucBuffer[0] = 0x55;
	i2cWriteToIt7236(gl_ts->client, 0xFB,pucBuffer,1);	
	mdelay(30);

	//Request Full Authority of All Registers
	pucBuffer[0] = 0x01;
	i2cWriteToIt7236(gl_ts->client, 0xF1, pucBuffer, 1);

	// 1. Assert Reset of MCU
	pucBuffer[0] = 0x64;
	i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);
	pucBuffer[0] = 0x04;
	i2cWriteToIt7236(gl_ts->client, 0x01, pucBuffer, 1);

	// 2. Assert EF enable & reset
	pucBuffer[0] = 0x10;
	i2cWriteToIt7236(gl_ts->client, 0x2B, pucBuffer, 1);
	pucBuffer[0] = 0x00;
	i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);

	// 3. Test Mode Enable
	pucBuffer[0] = 0x07;
	gl_ts->client->addr = 0x7F;
	err_temp = i2cWriteToIt7236(gl_ts->client, 0xF4, pucBuffer, 1);
	if(err_temp != 1){
		printk("[IT7236]%s : [%d]  xxx  error err_temp=%d \n",__func__,__LINE__,err_temp);
		return err_temp;
	}

	gl_ts->client->addr = i2c_addr_bak;

	nErasePage = fileSize/256;
	if(fileSize % 256 == 0)
		nErasePage -= 1;

	// 4. EF HVC Flow (Erase Flash)
	for( i = 0 ; i < nErasePage + 1 ; i++ ){
		// EF HVC Flow
		pucBuffer[0] = i+StartPage;
		i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);	//	Select axa of EF E/P Mode

		pucBuffer[0] = 0x00;
		i2cWriteToIt7236(gl_ts->client, 0xF9, pucBuffer, 1);	// Select axa of EF E/P Mode Fail
		pucBuffer[0] = 0xB2;
		i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);	// Select efmode of EF E/P Mode(all erase)
		pucBuffer[0] = 0x80;
		i2cWriteToIt7236(gl_ts->client, 0xF9, pucBuffer, 1);	// Pump Enable
		mdelay(10);
		pucBuffer[0] = 0xB6;
		i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF CHVPL Mode Cmd
		pucBuffer[0] = 0x00;
		i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF Standby Mode Cmd
	}

	// 5. EFPL Flow - Write EF
	for (i = 0; i < fileSize; i += 256)
	{
		pucBuffer[0] = 0x05;
		err_temp = i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF PL Mode Cmd
		if(err_temp != 1){
			printk("[IT7236]%s : [%d]  xxx  error err_temp=%d \n",__func__,__LINE__,err_temp);
			return err_temp;
		}

		//	Write EF Data - half page(128 bytes)
		for(registerI = 0 ; registerI < 128; registerI++)
		{
			if(( i + registerI ) < fileSize ) {
				DATABuffer1[registerI] = InputBuffer[i+registerI];
			}
			else {
				DATABuffer1[registerI] = 0x00;
			}
		}
		for(registerI = 128 ; registerI < 256; registerI++)
		{
			if(( i + registerI ) < fileSize ) {
				DATABuffer2[registerI - 128] = InputBuffer[i+registerI];
			}
			else {
				DATABuffer2[registerI - 128] = 0x00;
			}
		}
		registerJ = i & 0x00FF;
		page = ((i & 0x3F00)>>8) + StartPage;
		pageres = i % 256;
		retry = 0;
		do {
			pucBuffer[0] = page;
			i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);
			i2cReadFromIt7236(gl_ts->client, 0xF0, pucBuffer, 1);
			retry++;
		} while((pucBuffer[0] != page) && (retry < 10));

		/* write 256 bytes once */
		retry = 0;

		gl_ts->client->addr = 0x7F;
		do {
			ret1 = i2cWriteToIt7236(gl_ts->client, 0x00, DATABuffer1, 128);
			ret2 = i2cWriteToIt7236(gl_ts->client, 0x00 + 128, DATABuffer2, 128);
			retry++;
		} while(((ret1 * ret2) != 1) && (retry < 5));
		if(retry>4){
			printk("[IT7236] retry  %s : [%d]   \n",__func__,__LINE__);
		}
		gl_ts->client->addr = i2c_addr_bak;

		pucBuffer[0] = 0x00;
		i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF Standby Mode Cmd
		pucBuffer[0] = 0x00;
		i2cWriteToIt7236(gl_ts->client, 0xF9, pucBuffer, 1);	// Select axa of EF E/P Mode Fail
		pucBuffer[0] = 0xE2;
		i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);	// Select efmode of EF E/P Mode(all erase)
		pucBuffer[0] = 0x80;
		i2cWriteToIt7236(gl_ts->client, 0xF9, pucBuffer, 1);	// Pump Enable
		mdelay(10);
		pucBuffer[0] = 0xE6;
		i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF CHVPL Mode Cmd
		pucBuffer[0] = 0x00;
		i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF Standby Mode Cmd
	}

	// 6. Page switch to 0
	pucBuffer[0] = 0x00;
	i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);

	// 7. Write EF Read Mode Cmd
	pucBuffer[0] = 0x01;
	i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);		// Write EF Standby Mode Cmd

	// 8. Read EF Data, Compare the firmware and input data. for j loop
	for ( j = 0; j < fileSize; j+=128)
	{
		page = ((j & 0x3F00)>>8) + StartPage;					// 3F = 0011 1111, at most 32 pages
		pageres = j % 256;
		pucBuffer[0] = page;
		i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);

		gl_ts->client->addr = 0x7F;
		i2cReadFromIt7236(gl_ts->client, 0x00 + pageres, wTemp, 128); // use 0x7f to read data
		gl_ts->client->addr = i2c_addr_bak;
		// Compare Flash Data
		for( k = 0 ; k < 128 ; k++ )
		{
			if( j+k >= fileSize )
				break;

			pageres = (j + k) % 256;
			OutputDataBuffer[j+k] = wTemp[k];
			WriteDATABuffer[k] = InputBuffer[j+k];

			if(OutputDataBuffer[j+k] != WriteDATABuffer[k])
			{
				Addr = page << 8 | pageres;
				printk("Addr: %04x, Expected: %02x, Read: %02x\r\n", Addr, WriteDATABuffer[k], wTemp[k]);
				result = 0;
			}
		}
	}

	if(!result)
	{
		fnFirmwareReinitialize();
		printk("[IT7236] Failed to Upgrade Firmware\n\n");
		fw_upgrade_success = 1;
		return -1;
	}

	// 9. Write EF Standby Mode Cmd
	pucBuffer[0] = 0x00;
	i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);
	// 10. Power On Reset
	fnFirmwareReinitialize();

	printk("[IT7236] Success to Upgrade Firmware\n\n");

//	get_config_ver();
	fw_upgrade_success = 0;
/*
	struct regulator *regulator_tp = NULL;
	regulator_tp = regulator_get(NULL,"rk818_ldo5");
	if (regulator_tp == NULL) {
		printk("%s: regulator get failed,regulator name:\n",__func__);
	}
	
	regulator_disable(regulator_tp);
	msleep(10);
	printk("7236 vcc_tp \n");
	regulator_set_voltage(regulator_tp, 3300000,3300000);
	printk("7236 vcc_tp1 \n");
	regulator_enable(regulator_tp);
	regulator_put(regulator_tp);
	*/
//	enable_irq(gl_ts->client->irq);
	return 0;
}

#if 0
static u32 get_firmware_ver_cmd(void)
{
	char pucBuffer[1];
        u8  wTemp[4];
        u32  fw_version;
		int ret;

	//Wakeup
	pucBuffer[0] = 0x55; 
	i2cWriteToIt7236(gl_ts->client, 0xFB, pucBuffer, 1); 
	mdelay(1);
	waitCommandDone();
	pucBuffer[0] = 0x00;
	ret = i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);
	pucBuffer[0] = 0x80;
	ret = i2cWriteToIt7236(gl_ts->client, 0xF1, pucBuffer, 1);
	pucBuffer[0] = 0x01;
	ret = i2cWriteToIt7236(gl_ts->client, 0x40, pucBuffer, 1);
	pucBuffer[0] = 0x01;
	ret = i2cWriteToIt7236(gl_ts->client, 0x41, pucBuffer, 1);
	pucBuffer[0] = 0x40;
	ret = i2cWriteToIt7236(gl_ts->client, 0xF1, pucBuffer, 1);
	pucBuffer[0] = 0x00;
	ret = i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);
	pucBuffer[0] = 0x80;
	ret = i2cWriteToIt7236(gl_ts->client, 0xF1, pucBuffer, 1);

	waitCommandDone();	

	ret = i2cReadFromIt7236(gl_ts->client, 0x48, wTemp, 4);
	if(ret < 0)
	{
		return ret;
	}
	fw_version = (wTemp[0] << 24) | (wTemp[1] << 16) | (wTemp[2] << 8) | (wTemp[3]);
	//memcpy(fw_version, wTemp, 4);
	printk("[IT7236] EF Flash Firmware Version :0x%02x%02x%02x%02x\n", wTemp[0], wTemp[1], wTemp[2], wTemp[3]);

	return fw_version;
}
#endif
#if IT7236_FW_AUTO_UPGRADE
static ssize_t IT7236_upgrade_auto(void)
{
	int retry = 3;
	while(((it7236_upgrade((u8 *)rawDATA,sizeof(rawDATA))) != 0) && (retry > 0)){
		retry--;
		msleep(10);
	}

	return retry;
}
#endif

//====================================================================================

static long ite7236_ioctl(struct file *filp, unsigned int cmd,unsigned long arg)
{
	int retval = 0;
	int i;
	unsigned char buffer[MAX_BUFFER_SIZE];
	struct ioctl_cmd168 data;
	struct i2c_client * i2c_client;
	memset(&data, 0, sizeof(struct ioctl_cmd168));
	
	printk("[IT7236] ite7236_ioctl cmd =%d\n",cmd);
	i2c_client=gl_ts->client;

	switch (cmd) {
	case IOCTL_SET:
		printk("[IT7236] : =IOCTL_SET=\n");
		disable_irq_nosync(gl_ts->client->irq);
		if (!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}

		if ( copy_from_user(&data, (int __user *)arg, sizeof(struct ioctl_cmd168)) ) {
			retval = -EFAULT;
			goto done;
		}
		
		for (i = 0; i < data.length; i++) {
			buffer[i] = (unsigned char) data.buffer[i];
		}
        
		retval = i2cWriteToIt7236( i2c_client,
				(unsigned char) data.bufferIndex,
				buffer,
				(unsigned char)data.length );
		break;

	case IOCTL_GET:
		printk("[IT7236] : =IOCTL_GET=\n");
		disable_irq_nosync(gl_ts->client->irq);
		if (!access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}

		if (!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}

		if ( copy_from_user(&data, (int __user *)arg, sizeof(struct ioctl_cmd168)) ) {
			retval = -EFAULT;
			goto done;
		}

		retval = i2cReadFromIt7236(i2c_client,
				(unsigned char) data.bufferIndex,
				(unsigned char*) buffer,
				(unsigned char) data.length);

		for (i = 0; i < data.length; i++) {
			data.buffer[i] = (unsigned short) buffer[i];
		}

		if ( copy_to_user((int __user *)arg, &data, sizeof(struct ioctl_cmd168)) ) {
			retval = -EFAULT;
			goto done;
		}
		break;

	default:
		retval = -ENOTTY;
		break;
	}

done:
	enable_irq(gl_ts->client->irq);

	return (retval);
}

static int ite7236_open(struct inode *inode, struct file *filp)
{
	int i;
	struct ioctl_cmd168 *dev;
	printk("[IT7236] Open Device\n");
	dev = kzalloc(sizeof(struct ioctl_cmd168), GFP_KERNEL);
	if (dev == NULL) {
		return -ENOMEM;
	}

	/* initialize members */
	//rwlock_init(&dev->lock);
	for (i = 0; i < MAX_BUFFER_SIZE; i++) {
		dev->buffer[i] = 0xFF;
	}

	filp->private_data = dev;

	return 0; /* success */
}

static int ite7236_close(struct inode *inode, struct file *filp)
{
	struct ioctl_cmd168 *dev = filp->private_data;
	printk("[IT7236] Close Device\n");
	if (dev) {
		kfree(dev);
	}

	return 0; /* success */
}


#if 1
static int get_config_ver(void)
{
	char pucBuffer[1];
	int ret;

	//Wakeup
	pucBuffer[0] = 0x55; 
	i2cWriteToIt7236(gl_ts->client, 0xFB, pucBuffer, 1); 
	mdelay(1);
	waitCommandDone();

	// 1. Request Full Authority of All Registers
	pucBuffer[0] = 0x01;
	i2cWriteToIt7236(gl_ts->client, 0xF1, pucBuffer, 1);

	// 2. Assert Reset of MCU
	pucBuffer[0] = 0x64;
	i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);
	pucBuffer[0] = 0x04;
	i2cWriteToIt7236(gl_ts->client, 0x01, pucBuffer, 1);

	// 3. Assert EF enable & reset
	pucBuffer[0] = 0x10;
	i2cWriteToIt7236(gl_ts->client, 0x2B, pucBuffer, 1);
	pucBuffer[0] = 0x00;
	i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);

	// 4. Test Mode Enable
	pucBuffer[0] = 0x07;
	gl_ts->client->addr = 0x7F;
	i2cWriteToIt7236(gl_ts->client, 0xF4, pucBuffer, 1);
	gl_ts->client->addr = i2c_addr_bak;

	pucBuffer[0] = 0x04;
	i2cWriteToIt7236(gl_ts->client, 0xF0, pucBuffer, 1);

	// 6. Write EF Read Mode Cmd
	pucBuffer[0] = 0x01;
	i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);

	gl_ts->client->addr = 0x7F;
	ret = i2cReadFromIt7236(gl_ts->client, 0x00, wTemp, 10);
	gl_ts->client->addr = i2c_addr_bak;

	// 7. Write EF Standby Mode Cmd
	pucBuffer[0] = 0x00;
	i2cWriteToIt7236(gl_ts->client, 0xF7, pucBuffer, 1);

	// 8. Power On Reset
	fnFirmwareReinitialize();

	memcpy(config_id, wTemp+6, 4);
	printk("[IT7236] EF Flash Firmware Version : %#x, %#x, %#x, %#x\n", config_id[0], config_id[1], config_id[2], config_id[3]);

	return ret;
}
#endif 

int vcc_tp_reinit = 0;
extern struct input_dev *vb_input_dev ;
int it7236_flag = 0;
unsigned int touch_value = 0, touch_value1= 0;
static void Read_Point(struct IT7236_tk_data *ts)
{
	unsigned char pucSliderBuffer[4];
		//TODO:You can change the IT7236_TOUCH_DATA_REGISTER_ADDRESS to others register address.
	i2cReadFromIt7236(gl_ts->client, IT7236_TOUCH_SLIDER_REGISTER_ADDRESS, pucSliderBuffer, 4);
//	i2cReadFromIt7236(gl_ts->client, IT7236_TOUCH_PROXIMITY_REGISTER_ADDRESS, pucProximityBuffer, 2); 
	printk("[IT7236] %s : slider Buffer =%d \t  proximity = %d....%d \n",__func__,(int)pucSliderBuffer[1],(int)pucSliderBuffer[2],(int)pucSliderBuffer[3]);
//	printk("[IT7236] %s : slider Buffer =%d \t \n",__func__,(int)pucSliderBuffer[1]*(int)(255/60));
//	printk("[IT7236] %s : slider Buffer =%d \t \n",__func__,(int)pucSliderBuffer[1]);

	// for key_mouse	
/*
	touch_value1 = (int)pucSliderBuffer[1];
	if((int)pucSliderBuffer[1] != 255  && touch_value1 != touch_value ){
	//	input_report_abs(vb_input_dev, ABS_X, (int)pucSliderBuffer[1]*(int)(255/60));  
		
		input_report_key(vb_input_dev,BTN_5,(int)pucSliderBuffer[1]*(int)(255/60));
		input_sync(vb_input_dev);
		touch_value = (int)pucSliderBuffer[1];
		printk("it7236 press\n");
		it7236_flag = 1;
	}
	
	
	else if(((int)pucSliderBuffer[1] == 255) && (it7236_flag == 1)){
	//	input_report_rel(dev, REL_X, (int)pucSliderBuffer[1]*(int)(255/60)); 
	//	input_sync(input_dev);
		touch_value = (int)pucSliderBuffer[1];
		input_report_key(vb_input_dev,BTN_5,5);
		input_sync(vb_input_dev);
		printk("it7236 release\n");
		it7236_flag = 0;
	}
*/
	
 // for vb_switch	
 
 
 /*
	if((int)pucSliderBuffer[1] != 255){
		input_report_key(vb_input_dev,BTN_LEFT,1); 
		input_sync(vb_input_dev);
		printk("it7236 press\n");
		it7236_flag = 1;
	}else if(((int)pucSliderBuffer[1] == 255) && (it7236_flag == 1)){
		input_report_key(vb_input_dev,BTN_LEFT,0); 
		input_sync(vb_input_dev);
		printk("it7236 release\n");
		it7236_flag = 0;
	}
*/
//for  single

	if((int)pucSliderBuffer[1] != 255){
	//	input_report_key(input_dev, BTN_TOUCH, 1);
	/*
		int tmp = 0;
		touch_value1 = (int)pucSliderBuffer[1]*(int)(255/60);//记录本次值
		if (touch_value1>=touch_value){//触摸条不稳定 按在同一个点时值会有一点波动
			if(touch_value1 - touch_value >= 12)
				tmp = 1;//波动比较大
			else
				tmp = 2;
		}else{
			if(touch_value - touch_value1 >= 12)
				tmp = 1;//波动比较大
			else
				tmp = 2;
		}
		*/
	//	if(tmp == 1){
			input_report_abs(input_dev, ABS_X, (int)pucSliderBuffer[1]*(int)(255/60)); 
			input_sync(input_dev);
			touch_value = (int)pucSliderBuffer[1]*(int)(255/60);//记录上次值
			it7236_flag = 1;
	//	}
		printk("it7236 press\n");
		
	}else if(((int)pucSliderBuffer[1] == 255) && (it7236_flag == 1)){
	//	input_report_key(input_dev, BTN_TOUCH, 0);
	//	input_sync(input_dev);
		input_report_abs(input_dev, ABS_Y, touch_value);//避免逻辑问题 释放时报另一个轴
		input_sync(input_dev);
		printk("it7236 release\n");
		it7236_flag = 0;
	}


//for mul
/*
	if((int)pucSliderBuffer[1] != 255){
		input_mt_slot(input_dev, 0);
		//input_report_abs(input_dev, ABS_MT_TRACKING_ID, 0);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, 3);
		input_report_abs(input_dev, ABS_MT_POSITION_X, (int)pucSliderBuffer[1]*(int)(255/60));
		input_report_abs(input_dev, ABS_MT_POSITION_Y, (int)pucSliderBuffer[1]*(int)(255/60));
		input_report_abs(input_dev, ABS_MT_WIDTH_MAJOR, 1);
		//input_mt_sync(input_dev);
		input_sync(input_dev);
		printk("it7236 press\n");
		it7236_flag = 1;
	}else if(((int)pucSliderBuffer[1] == 255) && (it7236_flag == 1)){
			input_mt_slot(input_dev, 0);
		//	input_report_abs(input_dev, ABS_MT_TRACKING_ID, -1);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
		printk("it7236 release\n");
		it7236_flag = 0;
	}
*/
	
//	printk("[IT7236] %s : Buffer =%d..%d \n",__func__,(int)pucProximityBuffer[0],(int)pucProximityBuffer[1]);

	//TODO : Add your Code here. pucBuffer[] include the data.

//	enable_irq(gl_ts->client->irq);
//	input_sync(input_dev);

}

static irqreturn_t IT7236_tk_work_func(int irq, void *dev_id)
{
	disable_irq_nosync(gl_ts->client->irq);
	Read_Point(gl_ts);
//	enable_irq(gl_ts->client->irq);
	return IRQ_HANDLED;
}

static void it7236_timer_work(struct work_struct *work){
	
	Read_Point(gl_ts);
	queue_delayed_work(IT7236_wq ,&it7236_delay_work , msecs_to_jiffies(100));
}
static int IT7236_tk_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct IT7236_tk_data *ts;
	int ret = 0;


	printk("[IT7236] i2c  enter probe\n");
	printk("IT7236 i2c  Driver Version : %s",DRIVER_VERSION);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk("[IT7236] : IT7236_tk_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_check_functionality_failed;
	}

	ts->client = client;
	ts->input_dev = input_dev;
	i2c_set_clientdata(client, ts);
	gl_ts = ts;

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1;
	ts->early_suspend.suspend = IT7236_tk_early_suspend;
	ts->early_suspend.resume = IT7236_tk_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
//for poll

	IT7236_wq = create_workqueue("IT7236_wq");

	if (!IT7236_wq)
		goto err_check_functionality_failed;

	INIT_DELAYED_WORK(&it7236_delay_work, it7236_timer_work);
	
	queue_delayed_work(IT7236_wq ,&it7236_delay_work , msecs_to_jiffies(5000));


 	// for int
	/*
	struct device_node *np = client->dev.of_node;
	unsigned long irq_flags;
	ts->irq_gpio = of_get_named_gpio_flags(np, "irq-gpio", 0, (enum of_gpio_flags *)&irq_flags);
	gl_ts->client->irq = gpio_to_irq(ts->irq_gpio);
	if (ts->irq_gpio) {
		printk("[IT7236] : irq = %d , gpio = %d\n",gpio_to_irq(ts->irq_gpio), ts->irq_gpio);
		ret = request_irq(gl_ts->client->irq,
				   IT7236_tk_work_func,IRQF_TRIGGER_FALLING,
				   IT7236_I2C_NAME, ts);
		if (ret == 0){
			ts->use_irq = 1;
		}
		else {
			dev_err(&client->dev, "[IT7236] : Request IRQ Failed\n");
			goto err_request_irq;  
		}
	}else{
		printk("[IT7236] : FAIL irq = %d , gpio = %d\n",gpio_to_irq(ts->irq_gpio), ts->irq_gpio);
	}
*/	



	i2c_addr_bak = gl_ts->client->addr;
	#if IT7236_FW_AUTO_UPGRADE
	IT7236_upgrade_auto();
	#endif
//	get_config_ver();

	return 0;

err_request_irq:
	destroy_workqueue(ts->tk_workqueue);
err_check_functionality_failed:
	return ret;

}


static int IT7236_tk_remove(struct i2c_client *client) {
	return 0;
}




static const struct i2c_device_id IT7236_tk_id[] = {
	{ IT7236_I2C_NAME, 0 },
	{ }
};

static struct of_device_id it7236_match_table[] = {
	{ .compatible = "ite,7236",},
	{ },
};



static struct i2c_driver IT7236_tk_driver = {
		.class = I2C_CLASS_HWMON,
		.probe = IT7236_tk_probe,
		.remove = IT7236_tk_remove,
		.id_table = IT7236_tk_id,
		.driver = {
		.name = IT7236_I2C_NAME,
		.of_match_table = it7236_match_table,
		},
};

struct file_operations ite7236_fops = {
	.owner = THIS_MODULE,
	.open = ite7236_open,
	.release = ite7236_close,
	.unlocked_ioctl = ite7236_ioctl,
};

static int __init IT7236_tk_init(void)
{
	dev_t dev = MKDEV(ite7236_major, 0);
	int ret = 0;
	int err;
	struct device *class_dev = NULL;

	err = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
	if (err) {
		printk("[IT7236] IT7236 cdev can't get major number\n");
		goto err_alloc_dev;
	}

	ite7236_major = MAJOR(dev);

	/*allocate the character device*/
	cdev_init(&ite7236_cdev, &ite7236_fops);
	ite7236_cdev.owner = THIS_MODULE;
	ite7236_cdev.ops = &ite7236_fops;
	err = cdev_add(&ite7236_cdev, MKDEV(ite7236_major, ite7236_minor), 1);
	if(err) {
		goto err_add_dev;
	}

	/*register class*/
	ite7236_class = class_create(THIS_MODULE, DEVICE_NAME);
	if(IS_ERR(ite7236_class)) {
		printk("[IT7236]: failed in creating class.\n");
		goto err_create_class;
	}


	ite7236_dev = MKDEV(ite7236_major, ite7236_minor);
	class_dev = device_create(ite7236_class, NULL, ite7236_dev, NULL, DEVICE_NAME);
	if(class_dev == NULL) {
		printk("[IT7236]: failed IT7236 in creating device.\n");
		goto err_create_dev;
	}

	printk("=========================================\n");
	printk("register IT7236 cdev, major: %d, minor: %d \n", ite7236_major, ite7236_minor);
	printk("=========================================\n");

	/*alloc input device*/
	input_dev = input_allocate_device();
	if (input_dev == NULL) {
		err = -ENOMEM;
		printk("[IT7236]: failed to allocate input device\n");
		goto err_alloc_input_dev;
	}

	input_dev->name = IT7236_I2C_NAME;
	input_dev->phys = "I2C";
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0002;
	input_dev->id.version =  0x0003;
	
// for single

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) | BIT_MASK(EV_SYN);

	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
//	__set_bit(BTN_TOOL_PEN, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	
	input_set_abs_params(input_dev, ABS_X, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, 255, 0, 0);


//for mt 
/*
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_REP, input_dev->evbit);
	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	__set_bit(MT_TOOL_FINGER, input_dev->keybit);
	input_mt_init_slots(input_dev, (1+1), 0);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	input_set_abs_params(input_dev,ABS_MT_POSITION_X, 0, 255, 0, 0);
	input_set_abs_params(input_dev,ABS_MT_POSITION_Y, 0, 255, 0, 0);
	input_set_abs_params(input_dev,ABS_MT_TOUCH_MAJOR, 0, 4, 0, 0);
	input_set_abs_params(input_dev,ABS_MT_WIDTH_MAJOR, 0, 3, 0, 0);
*/	


	
	
	err = input_register_device(input_dev);
	if(err) {
		printk("[IT7236]: device register error\n");
		goto dev_reg_err;
	}

//	msleep(10000);
	return i2c_add_driver(&IT7236_tk_driver);


dev_reg_err:
	input_free_device(input_dev);
err_alloc_input_dev:
	device_destroy(ite7236_class, ite7236_dev);
err_create_dev:
	class_destroy(ite7236_class);
err_create_class:
	cdev_del(&ite7236_cdev);
err_add_dev:
err_alloc_dev:
	return ret;
	if (IT7236_wq)
	destroy_workqueue(IT7236_wq);
}

static void __exit IT7236_tk_exit(void)
{
	dev_t dev = MKDEV(ite7236_major, ite7236_minor);
	device_destroy(ite7236_class, ite7236_dev);
	class_destroy(ite7236_class);
	cdev_del(&ite7236_cdev);
	unregister_chrdev_region(dev, 1);
	i2c_del_driver(&IT7236_tk_driver);
	if (IT7236_wq)
	destroy_workqueue(IT7236_wq);
}

module_init( IT7236_tk_init);
module_exit( IT7236_tk_exit);

MODULE_DESCRIPTION("ITE IT723x TouchKey Driver");
MODULE_LICENSE("GPL");
