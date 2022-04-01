#pragma once

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
//#include "onic.h"

#define ONICDEV_MINOR_BASE (0)
#define ONICDEV_MINOR_COUNT (1)
#define DRV_NAME "onicdev"

#define MAGIC_CODE 0x11223344

#ifndef VM_RESERVED
#define VMEM_FLAGS (VM_IO | VM_DONTEXPAND | VM_DONTDUMP)
#else
#define VMEM_FLAGS (VM_IO | VM_RESERVED)
#endif


struct onic_cdev
{
	unsigned int magic; /* structure ID for sanity checks */
	struct class *ponicdev_class;		
	unsigned int instance; /* char device number (ONIC card-number)*/
	void *pop;		/* parent device */
	dev_t cdevno;			/* character device major:minor */
	struct cdev cdev;		/* character device embedded struct */
	struct device *sys_device;	/* sysfs device */
};

int onic_create_cdev(void *pip);
int onic_delete_cdev(void *pip);