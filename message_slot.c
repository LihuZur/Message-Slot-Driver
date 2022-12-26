//This file implements the message slot IPC mechanism, using the examples in chardev2
//from the recitation 6 code for help

//Recitation 6 code- allow access to kernel-level
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

//Recitation 6 code- required includes
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/

#include <linux/slab.h>//for kmalloc

#include "message_slot.h"//Our header

MODULE_LICENSE("GPL");

//DEFINING DATA STRUCTURES TO SAVE OUR DATA


//Channel definition. Each channel has a unique id and saves up to 1 message
typedef struct channel{
    long channel_id;
    char *msg;//Not setting the max size initially. We will use kmalloc() in message_slot.c for each new message
    int msg_len;
    struct channel* next;//Pointer to the next channel of the same minor number
} channel;

//An array of minor_entry structures will be our data structure, and each index represents a minor
//number between 0 and 255, and stores a linked list of its channels
typedef struct minor_entry{
    channel *head;
} minor_entry;


//A struct for saving the minor number, channel and channel id associated with a file descriptor.
//a pointer to an fd_channel_data struct will be set as the file's private_data param
//in the built-in struct file, and we can access it when necceassary from fd -> private_data
typedef struct fd_channel_data{
    int minor;
    long channel_id;
    channel *channel;//Pointer to the channel itself which is connected to this file desctiptor
    char set;//Determines if the channel was already initialized (0- not set, 1- set)
} fd_channel_data;


//The data structure to store our channels and messages according to the minor indices in the array
minor_entry devices[MINORS_AMOUNT];


//DEVICE FUNCTIONS IMPLEMENTATION

//Implementing the device_open function
//Returns 0 on success, and error values in failure
static int device_open(struct inode *inode, struct file *file){
    int minor = iminor(inode);//Getting the minor number from the given inode
    fd_channel_data *data = (fd_channel_data *)kmalloc(sizeof(fd_channel_data), GFP_KERNEL);//Using the required flag
    if(data == NULL){//kmalloc() failed
        return -ENOMEM;//Cannot allocate memory for data
    }

    //Setting the data fields for future functions use
    data -> minor = minor;
    data -> set = 0;
    //Finally, data is ready to be stored in the file's field.
    file -> private_data = (void *)data;

    return 0;//Done!
}

//Implementing the device_ioctl function (same signature as in chardev2 code example)
//Returns 0 on success, and error values in failure
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param){
    channel *p1;//Will be used to traverse the list if needed
    channel *new;//Will be used to store a new node if we will have to create one
    fd_channel_data *data = (fd_channel_data *)(file -> private_data);//Retrieving the file descriptor's saved data
    channel *head = devices[data -> minor].head;//Finding the head of the relevant linked list
    
    //Illegal input params
    if(ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0){
        return -EINVAL;
    }

    //If the linked list for current minor number is still empty
    if(head == NULL){
        head = kmalloc(sizeof(channel), GFP_KERNEL);
        //If allocation failed
        if(head == NULL){
            return -ENOMEM;
        }

        //Setting the head fields accordingly
        head -> msg_len = 0;
        head -> channel_id = ioctl_param;
        head -> next = NULL;

        //Storing the changes in our data structure
        devices[data -> minor].head = head;

        //Setting the file data accordingly
        data -> channel = head;
        data -> channel_id = ioctl_param;
        data -> set = 1;

        return 0;//Done!
    }

    //Linked list for this minor number already exists and has at least 1 node inside
    else{
        p1 = head;
        
        //Searching to see if a node with this channel id exists, checking all nodes except last
        while(p1-> next != NULL){
            if(p1 -> channel_id == ioctl_param){
                //Node already exists, edit the data accordingly
                data -> channel = p1;
                data -> channel_id = ioctl_param;
                data -> set = 1;
                return 0;//Done!
            }

            p1 = p1 -> next;
        }

        //Checking the last node (did it separately so we can keep the pointer to the current last)
        if(p1 -> channel_id == ioctl_param){
                //Node already exists, edit the data accordingly
                data -> channel = p1;
                data -> channel_id = ioctl_param;
                data -> set = 1;
                return 0;//Done!
            }
        
        //Node does not exist in the list, we need to create it
        new = kmalloc(sizeof(channel), GFP_KERNEL);//Allocating space for this new node

        //If allocation failed
        if(new == NULL){
            return -ENOMEM;
        }

        //Setting the new node fields
        new -> msg_len = 0;
        new -> channel_id = ioctl_param;
        new -> next = NULL;

        //Setting the file data accordingly
        data -> channel = new;
        data -> channel_id = ioctl_param;
        data -> set = 1;

        //Putting the new node at the end of the list
        p1 -> next = new;

        return 0;//Done!
    }

}

//Implementing the device_read function (same signature as in chardev2 code example)
//Returns the number of bytes read on success, and an error case otherwise
static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset){
    fd_channel_data *data = (fd_channel_data *)(file -> private_data);//Retrieving the file descriptor's saved data
    int msg_len;

    //Checking validity of the channel 
    if((data -> set) == 0 || buffer == NULL){//Adding the case that the input buffer itself is NULL to this error case
        return -EINVAL;
    }

    msg_len = data -> channel -> msg_len;

    //Checking that there is indeed a message to read (in particular, that the msg_len isn't 0)
    if(msg_len == 0){
        return -EWOULDBLOCK;
    }

    //Checking that the provided buffer is big enough to store the data inside the channel
    if(msg_len > length){
        return -ENOSPC;
    }

    //After we ensured that the message can be copied successfully to the buffer, we can copy it
    //using the copy_to_user function which copies the data from kernel space to user space
    //This function returns 0 on success and otherwise non-zero
    if(copy_to_user(buffer, data -> channel -> msg, msg_len) == 0){
        return msg_len;
    }

    //Failure in copy_to _user()
    return -EFAULT;
}

//Implementing the device_write function (same signature as in chardev2 code example)
//Returns the number of bytes read on success, and an error case otherwise
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset){
    char *p1;//A temporary buffer to keep the recieved message
    fd_channel_data *data = (fd_channel_data *)(file -> private_data);//Retrieving the file descriptor's saved data

    //Checking validity of the channel 
    if((data -> set) == 0 || buffer == NULL){//Adding the case that the input buffer itself is NULL to this error case
        return -EINVAL;
    }

    //Checking for valid message length
    if(length <= 0 || length > MAX_MSG_SIZE){
        return -EMSGSIZE;
    }

    p1 = (char *)kmalloc(sizeof(char) * length, GFP_KERNEL);
    if(p1 == NULL){//Allocation failed
            return -ENOMEM;
        }

    if(copy_from_user(p1, buffer, length) == 0){//Trying to copy to the buffer we allocated
        kfree(data -> channel -> msg);
        data -> channel -> msg = p1;//Setting the channel message accordingly
        data -> channel -> msg_len = length;//After writing successfully, update the file's msg_len accordingly
        return length;
    }
    
    //Failure in copy_from _user()
    //PREVIOUS MESSAGE WAS NOT CORRUPTED!
    return -EIO;
}

//Implementing the device_release function (same signature as in chardev2 code example)
//All we need to do here is free our memory allocation for for the file data
static int device_release(struct inode* inode, struct file*  file){
    kfree(file -> private_data);//Free the kmalloc() allocation for the file data which was allocated in device_open
    return 0;
}

//FOPS STRUCT DEFINITION- SAME AS IN CHARDEV2


struct file_operations Fops = {
  .owner	      = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  .release        = device_release,
};


//REGISTER AND UNREGISTER DEVICE IMPLEMENTATION


//Driver initializer function (register the character device)
static int __init simple_init(void){
    //Like in chardev2 we will define rc variable and use register_chrdev()
    int i;
    int rc = -1;
    rc = register_chrdev( MAJOR_NUMBER, DEVICE_RANGE_NAME, &Fops );

    //Failed to register the device (same as chardev2)
    if (rc < 0)
	{
		printk(KERN_ERR "%s registraion failed for  %d\n",DEVICE_RANGE_NAME, MAJOR_NUMBER);
		return rc;
	}

    //Now we need to initialize out data structure array (with NULL as head of every linked list)
    for(; i<MINORS_AMOUNT; i++){
        devices[i].head = NULL;
    }

    return 0;//Done!
}

//Driver unregisteration functions
//Responsible to free all memory allocations in the data structure we created
static void __exit simple_cleanup(void){
    channel *p1, *head;
    int i = 0;

    //Free the linked list at each index (minor)
    for(;i<MINORS_AMOUNT; i++){
        head = devices[i].head;
        while(head != NULL){
            //Need to free p1 without losing the pointer to the next node
            p1 = head;
            head = head -> next;
            kfree(p1 -> msg);
            kfree(p1);
        }
    }

    //After all allocations are freed, unregister the device (same as in chardev2 example)
    unregister_chrdev(MAJOR_NUMBER, DEVICE_RANGE_NAME);
}


module_init(simple_init);
module_exit(simple_cleanup);






