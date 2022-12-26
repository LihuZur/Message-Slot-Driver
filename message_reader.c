#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "message_slot.h"//Our header


/*
This file implements the message reader capability as instructed 
*/
int main(int argc, char *argv[]){

    //First, check that we got our 2 arguments (3 in total, including the argv[0] which is reserved)
    if(argc != 3){
        printf("Error: Incorrect number of arguments for message_sender! 2 required but %d were given\n", argc-1);
        exit(1);
    }

    //Open the file (ensure success)
    int fd;
    if((fd = open(argv[1], O_RDONLY)) < 0){
        printf("Error: Failed to open the file %s\n", argv[1]);
        exit(1);
    }

    int channel_id = atoi(argv[2]);//Converting the channel id number from string to int
    //Using ioctl syscall on the channel id from argv[2] and with MSG_SLOT_CHANNEL param, as specified in instructions
    int res = ioctl(fd, MSG_SLOT_CHANNEL, channel_id);
    if(res != 0){//ioctl failed
        perror("Error: ioctl failed");
        exit(1);
    }

    //Now we will read from the file to a buffer (max 128 characters, as specified in the instructions)
    char buffer[MAX_MSG_SIZE];
    res = read(fd, buffer, MAX_MSG_SIZE);
    if(res < 0){
        perror("Error: Failed reading from the file\n");
        exit(1);
    }

    close(fd);//Closing the file

    //Writing the output to stdout (checking for success/failure)
    if(write(STDOUT_FILENO, buffer, res) != res){
        perror("Error: Failed writing to stdout\n");
        exit(1);
    }

    return 0;//Done!
}