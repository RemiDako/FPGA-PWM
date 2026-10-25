

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

/* 
Info:	"Read Device Codes" starting on device-1...
Info:	ID code is: 0x0900281B
Info:	User code is: 0x000044CE
Info:	Status code is: 0x0001F020
Info:	Cost 0.6 second(s)
*/


typedef struct{
    uint8_t command[4];
    uint8_t len;
}gowin_command;

#define READ_ID_CODE 0x11
#define USER_CODE 0x13
#define STATUS_CODE 0x41
gowin_command COMMAND_READ_ID_CODE={{0x11,0x00,0x00,0x00},4};
gowin_command COMMAND_USER_CODE={{0x13,0x00,0x00,0x00},4};
gowin_command COMMAND_STATUS_CODE={{0x41,0x00,0x00,0x00},4};

int transfer(int _fd,uint8_t *send, uint8_t *recv, unsigned len);
uint32_t get_codes(int fd,uint8_t code);
uint32_t get_all_codes(int fd);

#define GPIO_VAL(gpio_num)      "/sys/class/gpio/gpio" gpio_num "/value"
#define CS_VAL_DEVICE                  GPIO_VAL("144")

void set_gpio(char * device,char * level){
    int fd = open(device, O_WRONLY);
    if (fd == -1) {
        printf("Unable to open %s\n", device);
    }

    if (write(fd, level, 1) != 1) {
        printf("Error writing to /sys/class/gpio/gpio144/value\n");
    }
    close(fd);
}
void set_cs(char * target){
    set_gpio(CS_VAL_DEVICE,target);
}

void give_some_sclk_cycles(){
    uint8_t buf[3]={0x00,0x00,0x00};
    // just for making gowin fpga happy
    int fdd = open("/dev/spidev0.1", O_RDWR);
    transfer(fdd,buf,buf,3);
    close(fdd);
}

int transfer(int _fd,uint8_t *send, uint8_t *recv, unsigned len)
{
    int result=0;
	if ((send == 0) && (recv == 0)) {
		return -1;
	}

	struct spi_ioc_transfer spi_transfer={
        .tx_buf = (uintptr_t)send,
        .rx_buf = (uintptr_t)recv,
        .len = len,
        .speed_hz = 30000,
        .bits_per_word = 8,
    };
    
	result = ioctl(_fd, SPI_IOC_MESSAGE(1), &spi_transfer);
	if (result != (int)len) {
        printf("spi transfer error\n");
        printf("send len:%d result:%d\n", len,result);
	}

    return 0;
}


uint32_t get_codes(int fd,uint8_t code){
    uint8_t buf[8]={code};
    uint32_t result=0;
    give_some_sclk_cycles();
    transfer(fd,buf,buf,8);
    give_some_sclk_cycles();
    printf("get code:0x%x  :",code);
    result=(uint32_t) buf[4]<<24 |(uint32_t) buf[5] <<16 |(uint32_t) buf[6] <<8 | buf[7];
    printf("0x%x \n",result);
    return result;
}

int write_enable(int fd){
    uint8_t buf[2]={0x15};
    give_some_sclk_cycles();
    int result= transfer(fd,buf,buf,2); 
    give_some_sclk_cycles();
    return result;

}

int write_something(int fd){
    uint8_t buf[3]={0xff,0xff,0xff};
    give_some_sclk_cycles();
    int ret = transfer(fd,buf,buf,3);
    give_some_sclk_cycles();
    return ret;
}

// int gpio_export(){
//     int fd = open("/sys/class/gpio/export", O_WRONLY);
//     if (fd == -1) {
//         printf("Unable to open /sys/class/gpio/export\n");
//     }

//     if (write(fd, "144", 3) != 3) {
//         printf("Error to export 144\n");
//     }
//     close(fd);

//     fd = open("/sys/class/gpio/gpio144/direction", O_WRONLY);
//     if (fd == -1) {
//         printf("Unable to open /sys/class/gpio/gpio144/direction\n");

//     }

//     if (write(fd, "out", 3) != 3) {
//         printf("Error writing to /sys/class/gpio/gpio144/direction\n");
//     }
//     close(fd);    

//     return 0;
// }

uint8_t file_buffer[352732]={0x3B};

int write_file(int fd, FILE* fp){
    int num=0;
    int i=0;
    int mode,ret;
    num=fread(file_buffer+1,1,352732,fp);
    printf("file size:%d\n",num);
    
    //transfer(fd,file_buffer,file_buffer,4096); 

    //////////////// give_some_fucking_sclk_cycles();

    //fd = open("/dev/spidev0.3", O_RDWR);

    //gpio_export();

    //set_cs("0");
    // control cs manually, use a fake spidev to output MOSI
    
    num+=1;
/*
    int SIZE=4095;
    for(i=0;i<num/SIZE;++i){
        printf("cycle:%d\n",i);
        transfer(fd,file_buffer+i*SIZE,file_buffer,SIZE); 
    }
    printf("last cycle num:%d\n",num%SIZE);
    */
    //transfer(fd,file_buffer+i*SIZE,file_buffer,num%SIZE);  
    give_some_sclk_cycles();
    transfer(fd,file_buffer,file_buffer,num);  
    give_some_sclk_cycles();
    //set_cs("1");

    return 0;
} 

int rewrite(int fd){
    uint8_t buf[2]={0x12};//3c
    // give_some_fucking_sclk_cycles();
    return transfer(fd,buf,buf,2);   
}

int erase_sram(int fd){
    uint8_t buf[2]={0x05};
    // give_some_fucking_sclk_cycles();
    return transfer(fd,buf,buf,2);       
}

int write_disable(int fd){
    uint8_t buf[2]={0x3A};

    int result=transfer(fd,buf,buf,2);     
    give_some_fucking_sclk_cycles();

    return result;
}

uint32_t get_all_codes(int fd){
    // print all code, return status code
    get_codes(fd,READ_ID_CODE);
    get_codes(fd,USER_CODE); 
    return get_codes(fd,STATUS_CODE);
}


int main(int argc,char **argv){
    if (argc != 3){
        printf("error, please set the input filename and spidevice");
        return -1;
    }


    FILE *fp=0;
    int ret,mode,fd;
    ret=mode=fd=0;

    char *filename=argv[2];
    char *device=argv[1];
    printf("open datafile:%s\n",filename);
    fp = fopen(filename,"rb");

    printf("open spidevice:%s\n",device);
    fd = open(device, O_RDWR);
    if (fd == -1) {
        perror("Unable to open SPI device");
        fclose(fp);
        return 1;
    }
    uint8_t spi_mode = SPI_MODE_0;
    if (ioctl(fd, SPI_IOC_WR_MODE, &spi_mode) == -1) {
        perror("Unable to set SPI mode");
        close(fd);
        fclose(fp);
        return 1;
    }


    // gpio_export();

    // give_some_fucking_sclk_cycles();

    // set_cs("0");
    // write_something(fd);


    // set_cs("1");
    



    // fd = open(device, O_RDWR);
    // printf("open spidevice:%s\n",device);

    // mode = SPI_MODE_0;
    // ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);

	// if (ret == -1)
	// 	printf("can't set spi mode\n");

	// ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	// if (ret == -1)
	// 	printf("can't get spi mode\n");

    // if(mode!=SPI_MODE_0){
    //     printf("can't set spi mode!\n");
    // }
   

    // erase_sram(fd);
    give_some_fucking_sclk_cycles();
    sleep(1);
    rewrite(fd);
    // give_some_fucking_sclk_cycles();
    give_some_fucking_sclk_cycles();
    get_all_codes(fd);

    write_enable(fd);

    get_all_codes(fd);

    write_file(fd,fp);

    get_all_codes(fd);

    write_disable(fd);

    uint32_t status=0;
    status=get_all_codes(fd);

    close(fd);
    fclose(fp);

    if(status & (1<<13)){
        return 0;
    }else{
        return 1;
    }
}
