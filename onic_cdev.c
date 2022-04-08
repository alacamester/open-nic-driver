#include "onic_cdev.h"
#include "onic.h"
#include <linux/pci.h> 

static int onic_cdev_open(struct inode *inode, struct file *file);
static int onic_cdev_close(struct inode *inode, struct file *file);
static long onic_cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t onic_cdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t onic_cdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);
static int onic_cdev_mmap(struct file *file, struct vm_area_struct *vma);


static const struct file_operations onic_fops = {
	.owner = THIS_MODULE,
	.open = onic_cdev_open,
	.release = onic_cdev_close,
	.read = onic_cdev_read,
	.write = onic_cdev_write,
	.unlocked_ioctl = onic_cdev_ioctl,
	.mmap = onic_cdev_mmap,
};

static int onic_cdev_open(struct inode *inode, struct file *file)
{
	struct onic_cdev *poc;

	/* pointer to containing structure of the character device inode */
	poc = container_of(inode->i_cdev, struct onic_cdev, cdev);
	
//	printk("ONIC cdev open, Magic: %X", poc->magic);
	if (poc->magic != MAGIC_CODE) return -EINVAL;
	
	/* create a reference to our char device in the opened file */
	file->private_data = poc;
	return 0;
}

static int onic_cdev_close(struct inode *inode, struct file *file)
{
//	printk("ONIC cdev close");
	return 0;
}

static long onic_cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	return 0;
}

static ssize_t onic_cdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	
	return 0;
}

static ssize_t onic_cdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{

	return 0;
}

static int onic_cdev_mmap(struct file *file, struct vm_area_struct *vma)
{
	int rc;
	struct onic_cdev *poc = (struct onic_cdev *)file->private_data;
	struct onic_private *priv = (struct onic_private *)poc->pop;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;

	off = vma->vm_pgoff << PAGE_SHIFT;
	// BAR 2 physical address 
	phys = pci_resource_start(priv->pdev, 2);
	vsize = vma->vm_end - vma->vm_start;
	// complete resource 
	psize = pci_resource_len(priv->pdev, 2);

	if (vsize > psize)
		return -EINVAL;
	//
	// pages must not be cached as this would result in cache line sized
	// accesses to the end point
	//
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	//
	// prevent touching the pages (byte access) for swap-in,
	// and prevent the pages from being swapped out
	//
	vma->vm_flags |= VMEM_FLAGS;
	// make MMIO accessible to user space 
	rc = io_remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT, vsize, vma->vm_page_prot);
	if (rc)
		return -EAGAIN;
	return 0;
}


int onic_create_cdev(void *pip)
{
	int rc;
	int major, minor;
	struct onic_private *priv = (struct onic_private *)pip;
	struct onic_hardware *hw = &priv->hw;
	struct onic_cdev *pocdev = &priv->oc;
	char cname[16];

	/* initialize user character device */
	pocdev->sys_device = NULL;
	pocdev->ponicdev_class = NULL;
	pocdev->magic = MAGIC_CODE;
	pocdev->instance = PCI_SLOT(priv->pdev->devfn);

	if (!hw->addr) 
	{
		printk("ONIC-cdev(%i): no user logic BAR detected\n", pocdev->instance);
		return 1;
	}

	rc = alloc_chrdev_region(&pocdev->cdevno, ONICDEV_MINOR_BASE, ONICDEV_MINOR_COUNT, DRV_NAME);
	major = MAJOR(pocdev->cdevno);

	minor = 0;

	pocdev->cdevno = MKDEV(major, minor);

	cdev_init(&pocdev->cdev, &onic_fops);

	pocdev->cdev.owner = THIS_MODULE;
	pocdev->pop = (void *)priv;

	/* bring character device live */
	rc = cdev_add(&pocdev->cdev, pocdev->cdevno, ONICDEV_MINOR_COUNT);
	if (rc < 0)
	{
		printk("ONIC-cdev(%i) cdev_add() = %d\n", pocdev->instance, rc);
		unregister_chrdev_region(pocdev->cdevno, ONICDEV_MINOR_COUNT);
		return 1;
	}
	/* create device on our class */
	scnprintf(cname, sizeof(cname)-1, "%s%i", DRV_NAME, pocdev->instance);
	pocdev->ponicdev_class = class_create(THIS_MODULE, cname);
	pocdev->sys_device = device_create(pocdev->ponicdev_class, &priv->pdev->dev, pocdev->cdevno, NULL, cname);
	if (!pocdev->sys_device)
	{
		printk("ONIC-cdev(%i) device_create failed\n", pocdev->instance);
		cdev_del(&pocdev->cdev);
		unregister_chrdev_region(pocdev->cdevno, ONICDEV_MINOR_COUNT);
		if (pocdev->ponicdev_class)
		{
//			class_unregister(pocdev->ponicdev_class);
			class_destroy(pocdev->ponicdev_class);
			pocdev->ponicdev_class = NULL;
		}
		return 1;
	}
	printk("ONIC-cdev(%i) created\n", pocdev->instance);
	return 0;

}

int onic_delete_cdev(void *pip)
{
	struct onic_private *priv = (struct onic_private *)pip;
	struct onic_cdev *pocdev = &priv->oc;

	if (!pocdev->sys_device) return 1;

	device_destroy(pocdev->ponicdev_class, pocdev->cdevno);
	cdev_del(&pocdev->cdev);
	unregister_chrdev_region(pocdev->cdevno, 1);
	if (pocdev->ponicdev_class)
	{
//		class_unregister(pocdev->ponicdev_class);
		class_destroy(pocdev->ponicdev_class);
		pocdev->ponicdev_class = NULL;
	}
	printk("ONIC-cdev(%i) removed\n", pocdev->instance);
//	pocdev->sys_device = NULL;
	return 0;
}