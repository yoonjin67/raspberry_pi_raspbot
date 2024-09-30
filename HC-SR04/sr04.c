#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/module.h>
#include<linux/kdev_t.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/slab.h>
#include<linux/uaccess.h>
#include<linux/err.h>
#include<linux/delay.h>
#include<linux/timer.h>
#include<linux/interrupt.h>
#include<linux/gpio.h>
#define ECHO 24
#define ECHO_LABEL "GPIO_24"
#define TRIG 23
#define TRIG_LABEL "GPIO_23"

int echoPinIrq;

dev_t dev = 0;

uint64_t sr04_send_ts, sr04_recv_ts;

/* -- start of function prototype */
 struct class *sr04_class;
 struct cdev sr04_cdev;

static int __init sr04_driver_init(void);
int sr04_driver_open(struct inode *inode, struct file *file) ;
int sr04_driver_release(struct inode *inode, struct file *file) ;
static void __exit sr04_driver_exit(void);

ssize_t sr04_read(struct file *file, char __user *buf, size_t len, loff_t * off);
static irqreturn_t irq_measure_distance(int irq, void *dev_id) {
	static unsigned long flags = 0;
	local_irq_save(flags);
	sr04_recv_ts = ktime_get_ns();
	gpio_set_value(TRIG,0);
	local_irq_restore(flags);
	return IRQ_HANDLED;
}

/* -- end of function prototype -- */

struct file_operations fops = {
	.owner	= THIS_MODULE,
	.read	= sr04_read,
	.open	= sr04_driver_open,
	.release = sr04_driver_release,
};

int sr04_driver_open(struct inode *inode, struct file *file) {
	return 0;
}
int sr04_driver_release(struct inode *inode, struct file *file) {
	return 0;
}

static int __init sr04_driver_init(void) {
	if(alloc_chrdev_region(&dev, 0, 1, "sr04")<0) { /* NOTE: DEV_T ALLOC */
		_printk("Cannot allocate major number, \n find comment \"NOTE: DEV_T ALLOC \"\n, \
			Quitting without driver ins...\n");
		return -1;
	}
	_printk("Major = %d, Minor = %d", MAJOR(dev),MINOR(dev));
	cdev_init(&sr04_cdev,&fops);
	if((cdev_add(&sr04_cdev,dev,1)) < 0) { /* NOTE: ADDING CDEV */
		_printk("Cannot add cdev: find comment \"NOTE: ADDING CDEV\"\n,\
			 Quitting without driver ins...\n");\
		goto class_error;
	}
	if(IS_ERR(sr04_class = class_create("sr04_class"))) { /*NOTE: CREATING DEV CLASS */
		_printk("Cannot create class structure,\n \
			  find comment \"NOTE: CREATING DEV CLASS\",\
			  Quitting without driver ins..\n");
		goto class_error;
	}

	if(IS_ERR(device_create(sr04_class, NULL, dev, NULL, "sr04"))) {
		_printk("Cannot create the device,\n find comment \"NOTE: DEV CREATION\", \nQuitting without driver ins...\n");
		goto device_creation_error;
	}

	if(!gpio_is_valid(ECHO)) {
		_printk("SR04 ECHO PIN IS NOT WORKING\n");
	}
	if(!gpio_is_valid(TRIG)) {
		_printk("SR04 TRIG PIN IS NOT WORKING\n");
	}

	if(gpio_request(TRIG,TRIG_LABEL)<0) {
		_printk("ERROR ON TRIG REQUEST");
		gpio_free(TRIG);
		return -1;
	}
	if(gpio_request(ECHO,ECHO_LABEL)<0) {
		_printk("ERROR ON ECHO REQUEST");
		gpio_free(ECHO);
		return -1;
	}

	echoPinIrq = gpio_to_irq(ECHO);
	pr_info("ECHO PIN irqNumber = %d", echoPinIrq);

	if(request_irq(echoPinIrq,
			(void *) irq_measure_distance, 
			IRQF_TRIGGER_RISING,
			"sr04",
			NULL)) {
		_printk("cannot register echo pin IRQ...");
		gpio_free(ECHO);

	}


	gpio_direction_output(TRIG,0);
	gpio_direction_input(ECHO);
	_printk("SR04 Dev. Driver inserted.");
	return 0;


class_error:
	class_destroy(sr04_class);
	cdev_del(&sr04_cdev);
	unregister_chrdev_region(dev,1);
	_printk("SR04 Dev. Driver failed");
	return -1;

device_creation_error:
	device_destroy(sr04_class,dev);
	class_destroy(sr04_class);
	cdev_del(&sr04_cdev);
	unregister_chrdev_region(dev,1);
	_printk("SR04 Dev. Driver failed");
	return -1;
}

static void __exit sr04_driver_exit(void) {
	free_irq(echoPinIrq,NULL);
	gpio_free(ECHO);
	gpio_free(TRIG);
	device_destroy(sr04_class,dev);
	class_destroy(sr04_class);
	cdev_del(&sr04_cdev);
	unregister_chrdev_region(dev,1);
	_printk( "SR04 Dev. driver removed." );
}


ssize_t sr04_read(struct file *file, char __user *buf, size_t len, loff_t * off) {
	sr04_send_ts = ktime_get_ns();
	sr04_recv_ts = 0;
	gpio_set_value(TRIG,1);
	msleep_interruptible(500);

	int duration = sr04_recv_ts-sr04_send_ts;
	if(duration<0) {
		_printk("SR04 Distance measurement: failed to get ECHO..");
		return 0;
	} else {
		char dist[5];
		memset(dist,0,sizeof(char));
		sprintf(dist, "%d", (int) ((340*duration)/1000000000)/2);
		int copied_bytes=copy_to_user(buf,dist,5);
		if(copied_bytes<0) {
			_printk("Distance hasn't copied to user...");
		}
		return sizeof(dist);
	}
	return 0;
}


module_init(sr04_driver_init);
module_exit(sr04_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yunjin Lee <gzblues61@gmail.com>");
MODULE_DESCRIPTION("HC-SR04");
MODULE_VERSION("0.01");

