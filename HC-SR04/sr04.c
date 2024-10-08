#include<linux/init.h>
#include<linux/delay.h>
#include<linux/workqueue.h>
#include<linux/module.h>
#include<linux/wait.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/err.h>
#include<linux/interrupt.h>
#include<linux/time.h>
#include<linux/gpio.h>
#define ECHO 536 // gpio-536 (GPIO-24) in /sys/kernel/debug/info 
#define ECHO_LABEL "GPIO_24"
#define TRIG 535 // GPIO 23 // gpio-535 (GPIO-23) in /sys/kernel/debug/info 
#define TRIG_LABEL "GPIO_23"
static wait_queue_head_t waitqueue; //waitqueue for wait and wakeup

int IRQ_NO; //variabe for storing echo pin irq
_Bool echo_status; //for checking ECHO pin status, needed for identifying RISING/FALLING
dev_t dev = 0;

uint64_t sr04_send_ts, sr04_recv_ts, duration;
/* start of IRQ Handler */

static irqreturn_t echo_irq_triggered(int irq, void *dev_id) {
	echo_status = (_Bool)gpio_get_value(ECHO);
	if(echo_status == 1) {
		_printk("ECHO INTERRUPT\n");
	} else {
		sr04_recv_ts = ktime_get_ns();
		_printk("SUCCEED TO GET sr04_recv_ts%llu\n", sr04_recv_ts);
		duration = sr04_recv_ts-sr04_send_ts;
		wake_up_interruptible(&waitqueue);
	}
		
	return IRQ_HANDLED;
}


/* -- start of function prototype */
 struct class *sr04_class;
 struct cdev sr04_cdev;

static int __init sr04_driver_init(void);
int sr04_driver_open(struct inode *inode, struct file *file) ;
int sr04_driver_release(struct inode *inode, struct file *file) ;
static void __exit sr04_driver_exit(void);

ssize_t sr04_read(struct file *file, char __user *buf, size_t len, loff_t * off);

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


	//gpio availability check
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
		gpio_free(TRIG); //if the program has executed until now, trig is available, and requested succesfully
		gpio_free(ECHO);
		return -1;
	}

	IRQ_NO = gpio_to_irq(ECHO); // GPIO pin as interrupt pin
	if(request_irq(IRQ_NO, echo_irq_triggered, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "hc-sr04", (void *) echo_irq_triggered)) { //request irq function is for measurement... see the top of the code.
		_printk("cannot register Irq...");
		free_irq(IRQ_NO, (void *) echo_irq_triggered);
	}

	gpio_direction_output(TRIG,0);
	gpio_direction_input(ECHO);
	init_waitqueue_head(&waitqueue);
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
	free_irq(IRQ_NO, (void *) echo_irq_triggered);
	gpio_free(ECHO);
	gpio_free(TRIG);
	device_destroy(sr04_class,dev);
	class_destroy(sr04_class);
	cdev_del(&sr04_cdev);
	unregister_chrdev_region(dev,1);
	_printk( "SR04 Dev. driver removed." );
}


ssize_t sr04_read(struct file *file, char __user *buf, size_t len, loff_t * off) {
	gpio_set_value(TRIG,1);
	sr04_send_ts = ktime_get_ns();
	_printk("sr04_send_ts: %llu\n", sr04_send_ts);
	wait_event_interruptible(waitqueue,echo_status == 0); //wait for interrupt pin
	gpio_set_value(TRIG,0);
	if(duration<=0) { //if duration is invalid
		_printk("SR04 Distance measurement: failed to get ECHO.. : duration is %llu\n", duration);
		return 0;
	} else {
		char dist[5];
		memset(dist,0,sizeof(dist));
		sprintf(dist, "%llu", duration*85/10000000);
		_printk("duration : %llu\n", duration);
		int copied_bytes=copy_to_user(buf,dist,5);  //returning value as character
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

