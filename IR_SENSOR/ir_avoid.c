#include<linux/module.h>
#include<linux/init.h>
#include<linux/module.h>
#include<linux/cdev.h>
#include<linux/gpio.h>
#include<linux/device.h>


#define ON_AVOID 537
#define L_AVOID 521
#define R_AVOID 522

dev_t dev = 0;

static struct class *ir_class;
static struct cdev ir_cdev;
_Bool is_on = 1;
static int __init ir_driver_init(void);
static void __exit ir_driver_exit(void);

static int ir_open(struct inode *inode, struct file *file);
static int ir_release(struct inode *inode, struct file *file);
static ssize_t ir_read(struct file *file, char __user *buf, size_t len, loff_t *off);
static ssize_t ir_on_off(struct file *file, const char *buf, size_t len, loff_t *off);

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = ir_read,
	.write = ir_on_off,
	.open = ir_open,
	.release = ir_release,
};

static int ir_open(struct inode *inode, struct file *file) {
	_printk("IR Sensor driver opened\n");
	return 0;
}

static int ir_release(struct inode *inode, struct file *file) {
	_printk("IR Sensor driver closed\n");
	return 0;
}

static ssize_t ir_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
	_Bool gpio_state_left =  0, gpio_state_right = 0;
	char cur[6];
	memset(cur,0,sizeof(cur));
	gpio_state_left = gpio_get_value(L_AVOID);
	gpio_state_right = gpio_get_value(R_AVOID);
	// CAUTION : ( 1 : no blue light, 0: blue light is on )


	/************************
	 *	L | R           *
	 *	1 | 1 => NONE   *
	 *	0 | 0 => BOTH   *
	 *      1 | 0 => RIGHT  *
	 *	0 | 1 => LEFT   *
	 ***********************/
	if(gpio_state_left && gpio_state_right) {
		sprintf(cur,"NONE");
	} else if (!(gpio_state_left^gpio_state_right)) {
		sprintf(cur, "BOTH");
	} else if(gpio_state_left) {
		sprintf(cur, "RIGHT");
	}
	else {
		sprintf(cur,"LEFT");
	}
	int err = copy_to_user(buf,cur,6);
	if(err>0) {
		_printk("not all the bytes has been copied. failed : %d bytes",err );
	}
		
	return 6;
}


	
static ssize_t ir_on_off(struct file *file, const char *buf, size_t len, loff_t *off) {
	char status[4];
	int err = copy_from_user(status, buf, 4);
	if(err>0) {
		_printk("not all the bytes has been copied. failed : %d bytes", err);
	}
	if(!strcmp(status,"ON")) {
		gpio_set_value(ON_AVOID,1);
	} else if (!strcmp(status, "OFF")) {
		gpio_set_value(ON_AVOID,0);
	} else {
		_printk("ERROR: Unknown command, select between ON/OFF\n");
	}
	return 4;
}

static int __init ir_driver_init(void) {
	int major = MAJOR(dev);
	int minor = MINOR(dev);
	dev = MKDEV(major,minor);
	if((alloc_chrdev_region(&dev,minor,1,"ir_avoid")) < 0) {
		_printk("Cannot allocate major number");
		goto _unreg;
	}
	_printk("Major = %d, Minor = %d\n", major, minor);
	cdev_init(&ir_cdev, &fops);
	if((cdev_add(&ir_cdev,dev,1))<0) {
		goto _err_cdev_create;
	}
	ir_class = class_create("ir_device");
	if(IS_ERR(ir_class)) {
		_printk("Cannot create IR class\n");
		goto _err_class_create;
	}

	if(IS_ERR(device_create(ir_class, NULL, dev, NULL, "ir_device"))) {
		_printk("Cannot create IR device");
		goto _err_dev_create;
	}

	if(!gpio_is_valid(ON_AVOID)) {
		_printk("FATAL: ON_AVOID is not valid, cannot turn on IR!");
		goto _err_dev_create;
	}

	if(!gpio_is_valid(L_AVOID)) {
		_printk("L_AVOID is not valid\n");
		goto _err_dev_create;
	}
	if(!gpio_is_valid(R_AVOID)) {
		_printk("R_AVOID is not valid\n");
		goto _err_dev_create;
	}
	if(gpio_request(ON_AVOID, "ON_AVOID") < 0) {
		_printk("ON_AVOID is not valid\n");
		goto _err_gpio_on_avoid;
	}
	if(gpio_request(L_AVOID, "L_AVOID") < 0) {
		_printk("L_AVOID is not valid\n");
		goto _err_gpio_l;
	}
	if(gpio_request(R_AVOID, "R_AVOID") < 0) {
		_printk("R_AVOID is not valid\n");
		goto _err_gpio_r;
	}
	gpio_direction_output(ON_AVOID,1);
	gpio_direction_input(L_AVOID);
	gpio_direction_input(R_AVOID);
	return 0;

	

	_err_class_create:
		class_destroy(ir_class);
		cdev_del(&ir_cdev);
		goto _unreg;
	_err_cdev_create:
		cdev_del(&ir_cdev);
		goto _unreg;
	_err_gpio_on_avoid:
		gpio_free(ON_AVOID);
	_err_gpio_r:
		gpio_free(R_AVOID);	
		goto _err_gpio_l;
	_err_gpio_l:
		gpio_free(ON_AVOID);
		gpio_free(L_AVOID);
		goto _err_dev_create;
	_err_dev_create:	
		device_destroy(ir_class,dev);
		class_destroy(ir_class);
		cdev_del(&ir_cdev);
		goto _unreg;
	_unreg:
		unregister_chrdev_region(dev,1);
	return -1;
}

static void __exit ir_driver_exit(void){
	gpio_set_value(ON_AVOID,0);
	gpio_free(L_AVOID);
	gpio_free(R_AVOID);
	gpio_free(ON_AVOID);
	device_destroy(ir_class,dev);
	class_destroy(ir_class);
	cdev_del(&ir_cdev);
	unregister_chrdev_region(dev,1);
	_printk("IR Driver Removed\n");
}

module_init(ir_driver_init);
module_exit(ir_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yunjin Lee <gzblues61@gmail.com>");
MODULE_VERSION("0.01");

