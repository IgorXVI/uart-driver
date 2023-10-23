#include <linux/module.h>
#include <linux/init.h>
#include <linux/serdev.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

/* Meta Information */
MODULE_LICENSE("GPL");

/* Buffer for data */
static char global_buffer[255];
static int global_buffer_head = 0;
static int global_buffer_size = 0;
static int last_global_buffer_size = 0;

/* Variables for device and device class */
static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

#define DRIVER_NAME "serdevechodriver"
#define DRIVER_CLASS "SerdevEchoModuleClass"

/**
 * @brief Read data out of the buffer
 */
static ssize_t driver_read(struct file *File, char *user_buffer, size_t count, loff_t *offset)
{
	printk("serdev_echo file - read was called!\n");

	if (global_buffer_size == last_global_buffer_size)
	{
		return 0;
	}

	/* Copy data to user */
	copy_to_user(user_buffer, global_buffer, global_buffer_size);

	last_global_buffer_size = global_buffer_size;

	return global_buffer_size;
}

/**
 * @brief This function is called, when the device file is opened
 */
static int driver_open(struct inode *device_file, struct file *instance)
{
	printk("serdev_echo file - open was called!\n");

	last_global_buffer_size = 0;

	return 0;
}

/**
 * @brief This function is called, when the device file is opened
 */
static int driver_close(struct inode *device_file, struct file *instance)
{
	printk("serdev_echo file - close was called!\n");

	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
	.read = driver_read};

/* Declate the probe and remove functions */
static int serdev_echo_probe(struct serdev_device *serdev);
static void serdev_echo_remove(struct serdev_device *serdev);

static struct of_device_id serdev_echo_ids[] = {
	{
		.compatible = "brightlight,echodev",
	},
	{/* sentinel */}};
MODULE_DEVICE_TABLE(of, serdev_echo_ids);

static struct serdev_device_driver serdev_echo_driver = {
	.probe = serdev_echo_probe,
	.remove = serdev_echo_remove,
	.driver = {
		.name = "serdev-echo",
		.of_match_table = serdev_echo_ids,
	},
};

/**
 * @brief Callback is called whenever a character is received
 */
static int serdev_echo_recv(struct serdev_device *serdev, const unsigned char *buffer, size_t size)
{
	printk("serdev_echo - Received %ld bytes with \"%s\"\n", size, buffer);

	char *last_char_ptr = buffer + size - 1;
	char last_char = (char)(*last_char_ptr);

	if (global_buffer_size >= 255)
	{
		global_buffer_head = 0;
	}

	global_buffer[global_buffer_head] = last_char;

	if (global_buffer_size == global_buffer_head)
	{
		global_buffer_size++;
	}

	global_buffer_head++;

	return size;
}

static const struct serdev_device_ops serdev_echo_ops = {
	.receive_buf = serdev_echo_recv,
};

/**
 * @brief This function is called on loading the driver
 */
static int serdev_echo_probe(struct serdev_device *serdev)
{
	int status;
	printk("serdev_echo - Now I am in the probe function!\n");

	serdev_device_set_client_ops(serdev, &serdev_echo_ops);
	status = serdev_device_open(serdev);
	if (status)
	{
		printk("serdev_echo - Error opening serial port!\n");
		return -status;
	}

	serdev_device_set_baudrate(serdev, 9600);
	serdev_device_set_flow_control(serdev, false);
	serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);

	return 0;
}

/**
 * @brief This function is called on unloading the driver
 */
static void serdev_echo_remove(struct serdev_device *serdev)
{
	printk("serdev_echo - Now I am in the remove function\n");
	serdev_device_close(serdev);
}

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init my_init(void)
{
	printk("Hello, Kernel!\n");

	/* Allocate a device nr */
	if (alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0)
	{
		printk("Device Nr. could not be allocated!\n");
		return -1;
	}
	printk("read_write - Device Nr. Major: %d, Minor: %d was registered!\n", my_device_nr >> 20, my_device_nr && 0xfffff);

	/* Create device class */
	if ((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL)
	{
		printk("Device class can not be created!\n");
		goto ClassError;
	}

	/* create device file */
	if (device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL)
	{
		printk("Can not create device file!\n");
		goto FileError;
	}

	/* Initialize device file */
	cdev_init(&my_device, &fops);

	/* Regisering device to kernel */
	if (cdev_add(&my_device, my_device_nr, 1) == -1)
	{
		printk("Registering of device to kernel failed!\n");
		goto AddError;
	}

	printk("serdev_echo - Loading the driver...\n");
	if (serdev_device_driver_register(&serdev_echo_driver))
	{
		printk("serdev_echo - Error! Could not load driver\n");
		return -1;
	}

	return 0;
AddError:
	device_destroy(my_class, my_device_nr);
FileError:
	class_destroy(my_class);
ClassError:
	unregister_chrdev_region(my_device_nr, 1);
	return -1;
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit my_exit(void)
{
	printk("serdev_echo - Unload driver");
	serdev_device_driver_unregister(&serdev_echo_driver);

	cdev_del(&my_device);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class);
	unregister_chrdev_region(my_device_nr, 1);
	printk("Goodbye, Kernel\n");
}

module_init(my_init);
module_exit(my_exit);