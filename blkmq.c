#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>


#define DEVMAJOR 240
#define DEVNAME  "myblkmq"

typedef struct sblkdev_device_s
{
    sector_t capacity;      // Device size in bytes
    u8 * data;              // The data aray. u8 - 8 bytes
    atomic_t open_counter;  // How many openers
    struct blk_mq_tag_set tag_set;
    struct request_queue * queue; // For mutual exclusion

    struct gendisk * disk;        // The gendisk structure
} sblkdev_device_t;

sblkdev_device_t *_sblkdev_device;


static blk_status_t nvme_queue_rq (struct blk_mq_hw_ctx * hctx, const struct blk_mq_queue_data * bd)
{
    blk_status_t status = BLK_STS_OK;

#if 0   
    struct request * rq = bd-> rq;

    blk_mq_start_request (rq);

    // we can't use that thread
    {
        unsigned int nr_bytes = 0;

        if (do_simple_request (rq, & nr_bytes)! = SUCCESS)
            status = BLK_STS_IOERR;

        printk (KERN_WARNING "sblkdev: request process% d bytes  n", nr_bytes);

#if 0 // proprietary module
        blk_mq_end_request (rq, status);
#else // can set real processed bytes count
        if (blk_update_request (rq, status, nr_bytes)) // GPL-only symbol
            BUG ();
        __blk_mq_end_request (rq, status);
#endif
    }


#endif    
    return BLK_STS_OK; // always return ok
}



static struct blk_mq_ops _mq_ops = {
    .queue_rq = nvme_queue_rq,
};


static int sblkdev_add_device (void)
{
    int status;

    sblkdev_device_t * dev = kzalloc (sizeof (sblkdev_device_t), GFP_KERNEL);
    if (dev == NULL) {
        printk (KERN_WARNING "sblkdev: unable to allocate% ld bytes  n", sizeof (sblkdev_device_t));
        return -ENOMEM;
    }
    
    _sblkdev_device = dev;

    do {
      // status = sblkdev_allocate_buffer (dev);
      //  if (status)
      //      break;

        {
            struct request_queue * queue;

            dev-> tag_set.cmd_size = 64;
            dev-> tag_set.driver_data = dev;

            queue = blk_mq_init_sq_queue (&dev-> tag_set, &_mq_ops, 128, BLK_MQ_F_SHOULD_MERGE | BLK_MQ_F_SG_MERGE);
            if (IS_ERR (queue)) {
                ret = PTR_ERR (queue);
                printk (KERN_WARNING "sblkdev: unable to allocate and initialize tag set  n");
                break;
            }
            dev-> queue = queue;
        }
        dev-> queue-> queuedata = dev;

        {// configure disk
            struct gendisk * disk = alloc_disk (1); // only one partition
            if (disk == NULL) {
                printk (KERN_WARNING "sblkdev: Failed to allocate disk  n");
                ret = -ENOMEM;
                break;
            }

            // disk-> flags | = GENHD_FL_NO_PART_SCAN; // only one partition
            // disk-> flags | = GENHD_FL_EXT_DEVT;
            disk-> flags | = GENHD_FL_REMOVABLE;

            disk-> major = DEVMAJOR;
            disk-> first_minor = 0;
            disk-> fops = & _fops;
            disk-> private_data = dev;
            disk-> queue = dev-> queue;
            sprintf (disk-> disk_name, "sblkdev% d", 0);
            set_capacity (disk, dev-> capacity);

            dev-> disk = disk;
            add_disk (disk);
        }

        printk (KERN_WARNING "sblkdev: simple block device was created  n");
    } while (false);

    if (ret) {
        sblkdev_remove_device ();
        printk (KERN_WARNING "sblkdev: Failed add block device  n");
    }

    return ret;
}


static void sblkdev_remove_device (void)
{
    sblkdev_device_t * dev = _sblkdev_device;
    if (dev) {
        if (dev-> disk)
            del_gendisk (dev-> disk);

        if (dev-> queue) {
            blk_cleanup_queue (dev-> queue);
            dev-> queue = NULL;
        }

        if (dev-> tag_set.tags)
            blk_mq_free_tag_set (& dev-> tag_set);

        if (dev-> disk) {
            put_disk (dev-> disk);
            dev-> disk = NULL;
        }

        //sblkdev_free_buffer (dev);

        kfree (dev);
        _sblkdev_device = NULL;

        printk (KERN_WARNING "sblkdev: simple block device was removed  n");
    }
}


static int __init sblkdev_init (void)
{
	int status;
	
	status = register_blkdev (DEVMAJOR, DEVNAME);
	if (status <= 0){
		printk(KERN_WARNING "sblkdev: unable to get major numbern");
		return -EBUSY;
	}

	status = sblkdev_add_device();
	if (status)
		unregister_blkdev(DEVMAJOR, DEVNAME);

	return status;
}

static void __exit sblkdev_exit(void)
{
	sblkdev_remove_device();

	if (DEVMAJOR > 0)
		unregister_blkdev (DEVMAJOR, DEVNAME);
}

module_init(sblkdev_init);
module_exit(sblkdev_exit);
