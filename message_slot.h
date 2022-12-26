#define MAJOR_NUMBER 235
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUMBER, 0, unsigned int)//Like in chardev header example code
#define MAX_MSG_SIZE 128//Max length for a message
#define MINORS_AMOUNT 256//We use register_chrdev() so we can assume that according to instrucitons
#define DEVICE_RANGE_NAME "message_slot" //For register_chrdev() use , as in chardev2 example

