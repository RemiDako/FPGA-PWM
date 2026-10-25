

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>

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
#define DEFAULT_CS_HIGH_CLOCK_DEVICE "/dev/spidev0.3"
gowin_command COMMAND_READ_ID_CODE={{0x11,0x00,0x00,0x00},4};
gowin_command COMMAND_USER_CODE={{0x13,0x00,0x00,0x00},4};
gowin_command COMMAND_STATUS_CODE={{0x41,0x00,0x00,0x00},4};

static int cs_high_clock_fd = -1;

static int transfer_raw(int _fd,uint8_t *send, uint8_t *recv, unsigned len);
int transfer(int _fd,uint8_t *send, uint8_t *recv, unsigned len);
int get_codes(int fd,uint8_t code,uint32_t *value);
int get_all_codes(int fd,uint32_t *status);

static void delay_after_spi_transfer(void){
    struct timespec remaining = {
        .tv_sec = 0,
        .tv_nsec = 20 * 1000 * 1000
    };

    while (nanosleep(&remaining, &remaining) == -1) {
        if (errno != EINTR) {
            perror("Unable to delay after SPI transfer");
            return;
        }
    }
}

static int transfer_raw(int _fd,uint8_t *send, uint8_t *recv, unsigned len)
{
    if (_fd < 0 || len == 0 || (send == NULL && recv == NULL)) {
        errno = EINVAL;
        perror("Invalid SPI transfer");
        return -1;
    }

	struct spi_ioc_transfer spi_transfer={
        .tx_buf = (uintptr_t)send,
        .rx_buf = (uintptr_t)recv,
        .len = len,
        .speed_hz = 50000,
        .bits_per_word = 8,
    };
    
    int result = ioctl(_fd, SPI_IOC_MESSAGE(1), &spi_transfer);
    if (result == -1) {
        int saved_errno = errno;
        perror("SPI transfer failed");
        fprintf(stderr, "transfer length: %u bytes\n", len);
        if (saved_errno == EMSGSIZE) {
            fprintf(stderr,
                    "Increase the spidev bufsiz setting to at least %u bytes "
                    "and retry; this bitstream must be sent in one SPI message.\n",
                    len);
        }
        errno = saved_errno;
        return -1;
    }
    // delay_after_spi_transfer();

    if ((unsigned int)result != len) {
        fprintf(stderr, "Incomplete SPI transfer: sent %d of %u bytes\n",
                result, len);
        errno = EIO;
        return -1;
    }

    return 0;
}

static int clock_spi_with_cs_high(int fd){
    if (cs_high_clock_fd < 0 || fd < 0 || cs_high_clock_fd == fd) {
        errno = EINVAL;
        perror("Invalid SPI device for CS-high clocks");
        return -1;
    }

    uint8_t clocks = 0xFF;
    // int result = transfer_raw(cs_high_clock_fd, &clocks, &clocks, 1);
    return transfer_raw(cs_high_clock_fd, &clocks, &clocks, 1);//result;
}

int transfer(int _fd,uint8_t *send, uint8_t *recv, unsigned len)
{
    // if (clock_spi_with_cs_high(_fd) == -1) {
    //     return -1;
    // }
    if (transfer_raw(_fd, send, recv, len) == -1) {
        return -1;
    }
    // int ret = clock_spi_with_cs_high(_fd);
    return 0;//ret;
}

int get_codes(int fd,uint8_t code,uint32_t *value){
    uint8_t buf[8] = {code, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    if (transfer(fd,buf,buf,8) == -1) {
        return -1;
    }
    printf("get code:0x%x  :",code);
    *value = (uint32_t)buf[4] << 24 | (uint32_t)buf[5] << 16 |
             (uint32_t)buf[6] << 8 | buf[7];
    printf("0x%x \n", *value);
    return 0;
}

int write_enable(int fd){
    uint8_t buf[2]={0x15, 0xFF};
    int result= transfer(fd,buf,buf,2);
    return result;
}

int write_something(int fd){
    uint8_t buf[4]={0};
    return transfer(fd,buf,buf,4);
}


uint8_t file_buffer[43959] = {0x3B};

int write_file(int fd, FILE* fp){
    // size_t num = fread(file_buffer + 1, 1, sizeof(file_buffer) - 1, fp);
    // if (ferror(fp)) {
    //     perror("Unable to read bitstream");
    //     return -1;
    // }
    // if (num == sizeof(file_buffer) - 1 && fgetc(fp) != EOF) {
    //     fprintf(stderr, "Bitstream exceeds the %u-byte buffer limit\n",
    //             (unsigned int)(sizeof(file_buffer) - 1));
    //     return -1;
    // }
    // printf("file size:%u\n", (unsigned int)num);
    
    // return transfer(fd, file_buffer, file_buffer, (unsigned int)num + 1);

    int num=0;
    int i=0;
    int mode,ret;
    num=fread(file_buffer+1,1,43959,fp);
    printf("file size:%d\n",num);
    num+=1;
    transfer(fd,file_buffer,file_buffer,num);  
    return 0;
} 

int rewrite(int fd){
    uint8_t buf[2] = {0x3C, 0xFF};
    int result= transfer(fd,buf,buf,2);
    return result; 
}

int erase_sram(int fd){
    uint8_t buf[2]={0x05, 0xFF};
    int result= transfer(fd, buf, buf, 2);
    return result;       
}

int write_disable(int fd){
    uint8_t buf[2]={0x3A, 0xFF};
    int result= transfer(fd,buf,buf,2);
    return result;
}

int get_all_codes(int fd,uint32_t *status){
    // print all code, return status code
    uint32_t value;
    if (get_codes(fd, READ_ID_CODE, &value) == -1 ||
        get_codes(fd, USER_CODE, &value) == -1 ||
        get_codes(fd, STATUS_CODE, status) == -1) {
        return -1;
    }
    return 0;
}

void give_some_fucking_sclk_cycles(){
    uint8_t buf=0xff;
    // just for making gowin fpga happy
    int fd = open("/dev/spidev0.3", O_RDWR);
    transfer(fd,&buf,&buf,1);
    close(fd);
    
}


int main(int argc,char **argv){
    if (argc != 3 && argc != 4){
        fprintf(stderr,
                "Usage: %s <spidevice> <bitstream> [dummy-clock-spidevice]\n",
                argv[0]);
        return 1;
    }

    FILE *fp=0;
    int fd;
    const char *clock_device = argc == 4
        ? argv[3]
        : DEFAULT_CS_HIGH_CLOCK_DEVICE;

    char *filename=argv[2];
    char *device=argv[1];
    fp = fopen(filename,"rb");
    if (fp == NULL) {
        perror("Unable to open bitstream");
        return 1;
    }

    printf("open datafile:%s\n",filename);

    fd = open(device, O_RDWR);
    printf("open spidevice:%s\n",device);
    if (fd == -1) {
        perror("Unable to open SPI device");
        fclose(fp);
        return 1;
    }

    cs_high_clock_fd = open(clock_device, O_RDWR);
    if (cs_high_clock_fd == -1) {
        perror("Unable to open dummy-clock SPI device");
        close(cs_high_clock_fd);
        cs_high_clock_fd = -1;
        close(fd);
        fclose(fp);
        return 1;
    }
    printf("open dummy-clock spidevice:%s\n", clock_device);

    uint8_t spi_mode = SPI_MODE_0;
    if (ioctl(fd, SPI_IOC_WR_MODE, &spi_mode) == -1) {
        perror("Unable to set SPI mode");
        close(cs_high_clock_fd);
        close(fd);
        fclose(fp);
        return 1;
    }
    if (ioctl(fd, SPI_IOC_RD_MODE, &spi_mode) == -1) {
        perror("Unable to read SPI mode");
        close(cs_high_clock_fd);
        close(fd);
        fclose(fp);
        return 1;
    }
    if ((spi_mode & (SPI_CPOL | SPI_CPHA)) != SPI_MODE_0) {
        fprintf(stderr, "SPI clock mode mismatch: read back mode 0x%02x\n",
                spi_mode);
        close(cs_high_clock_fd);
        close(fd);
        fclose(fp);
        return 1;
    }

    uint8_t clock_mode = SPI_MODE_0;
    if (ioctl(cs_high_clock_fd, SPI_IOC_WR_MODE, &clock_mode) == -1) {
        perror("Unable to set dummy-clock SPI mode");
        close(cs_high_clock_fd);
        close(fd);
        fclose(fp);
        return 1;
    }
    if (ioctl(cs_high_clock_fd, SPI_IOC_RD_MODE, &clock_mode) == -1) {
        perror("Unable to read SPI mode");
        close(cs_high_clock_fd);
        close(fd);
        fclose(fp);
        return 1;
    }
    if ((clock_mode & (SPI_CPOL | SPI_CPHA)) != SPI_MODE_0) {
        fprintf(stderr, "SPI clock mode mismatch: read back mode 0x%02x\n",
                clock_mode);
        close(cs_high_clock_fd);
        close(fd);
        fclose(fp);
        return 1;
    }

    
    uint32_t status = 0;
    if (get_codes(fd, READ_ID_CODE, &status) == -1) {
        return -1;
    }
    
    // if (erase_sram(fd) == -1) {
    //     goto spi_error;
    // }
    // sleep(1);

    if (rewrite(fd) == -1) {
        goto spi_error;
    }

    give_some_fucking_sclk_cycles();
    if (write_enable(fd) == -1) {
        goto spi_error;
    }
    // give_some_fucking_sclk_cycles();

    // if (write_file(fd,fp) == -1) {
    //     goto spi_error;
    // }
    if (get_all_codes(fd, &status) == -1) {
        goto spi_error;
    }

    if (write_disable(fd) == -1) {
        goto spi_error;
    }


    if (get_all_codes(fd, &status) == -1) {
        goto spi_error;
    }

    close(fd);
    fclose(fp);

    if(status & (1<<13)){
        return 0;
    }else{
        return 1;
    }

spi_error:
    close(cs_high_clock_fd);
    cs_high_clock_fd = -1;
    close(fd);
    fclose(fp);
    return 1;
}
