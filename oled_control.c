#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define SPI_PATH "/dev/spidev0.0"

void setup_spi(int fd) {
    unsigned char mode = SPI_MODE_0;
    unsigned char bits = 8;
    unsigned int speed = 500000;
    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
}

void write_data(int fd, unsigned char *data, int length) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)data,
        .len = length,
        .speed_hz = 500000,
        .bits_per_word = 8,
    };
    ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

int main() {
    int fd = open(SPI_PATH, O_RDWR);
    if (fd < 0) {
        perror("Unable to open SPI device");
        return 1;
    }

    setup_spi(fd);

    unsigned char data[] = {0x00, 0xAE, 0xD5}; // Example commands for SSD1306

    write_data(fd, data, sizeof(data));

    close(fd);
    return 0;
}
