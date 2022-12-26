#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "message_slot.h"//Our header


/*
This file implements the message sender capability as instructed 
*/
int main(int argc, char *argv[]){

    //First, check that we got our 3 arguments (4 in total, including the argv[0] which is reserved)
    if(argc != 4){
        printf("Error: Incorrect number of arguments for message_sender! 3 required but %d were given\n", argc-1);
        exit(1);
    }

    //Open the file (ensure success)
    int fd;
    if((fd = open(argv[1], O_WRONLY)) < 0){
        printf("Error: Failed to open the file %s\n", argv[1]);
        exit(1);
    }

    int channel_id = atoi(argv[2]);//Converting the channel id number from string to int
    //Using ioctl syscall on the channel id from argv[2] and with MSG_SLOT_CHANNEL param, as specified in instructions
    int res = ioctl(fd, MSG_SLOT_CHANNEL, channel_id);
    if(res != 0){//ioctl failed
        perror("Error: ioctl failed\n");
        exit(1);
    }

    int msg_len = strlen(argv[3]);
    //Now we will write the buffer specified in argv[3] to the file
    if(write(fd, argv[3], msg_len) != msg_len){
        perror("Error: Failed writing to the file\n");
        exit(1);
    }

    close(fd);//Closing the file

    return 0;//Done!
}