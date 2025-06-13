#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define I2C_DEV "/dev/i2c-1"
#define SSD1306_ADDR 0x3C

// Font 5x7 ASCII cho một số ký tự cần thiết
uint8_t font5x7[][5] = {
// ' ' 33
{0x00,0x00,0x00,0x00,0x00}, // space
// '1' 49
{0x00,0x00,0x7F,0x00,0x00}, // '1'
// '2' 50
{0x62,0x51,0x49,0x49,0x46}, // '2'
// '3' 51
{0x22,0x41,0x49,0x49,0x36}, // '3'
// '4' 52
{0x18,0x14,0x12,0x7F,0x10}, // '4'
// '5' 53
{0x2F,0x49,0x49,0x49,0x31}, // '5'
// 'H' 72
{0x7F,0x08,0x08,0x08,0x7F}, // 'H'
};

int get_font_index(char c) {
switch(c) {
case ' ': return 0;
case '1': return 1;
case '2': return 2;
case '3': return 3;
case '4': return 4;
case '5': return 5;
case 'H': return 6;
default: return 0;
}
}

void ssd1306_send_command(int fd, uint8_t cmd) {
uint8_t buffer[2] = {0x00, cmd};
write(fd, buffer, 2);
}

void ssd1306_send_data(int fd, uint8_t data) {
uint8_t buffer[2] = {0x40, data};
write(fd, buffer, 2);
}

void ssd1306_init(int fd) {
ssd1306_send_command(fd, 0xAE); // Display OFF
ssd1306_send_command(fd, 0xA8); ssd1306_send_command(fd, 0x3F);
ssd1306_send_command(fd, 0xD3); ssd1306_send_command(fd, 0x00);
ssd1306_send_command(fd, 0x40);
ssd1306_send_command(fd, 0xA1); ssd1306_send_command(fd, 0xC8);
ssd1306_send_command(fd, 0xDA); ssd1306_send_command(fd, 0x12);
ssd1306_send_command(fd, 0x81); ssd1306_send_command(fd, 0x7F);
ssd1306_send_command(fd, 0xA4); ssd1306_send_command(fd, 0xA6);
ssd1306_send_command(fd, 0xD5); ssd1306_send_command(fd, 0x80);
ssd1306_send_command(fd, 0x8D); ssd1306_send_command(fd, 0x14);
ssd1306_send_command(fd, 0xAF); // Display ON
}

void ssd1306_clear(int fd) {
for (int page = 0; page < 8; page++) {
ssd1306_send_command(fd, 0xB0 + page); // Set page address
ssd1306_send_command(fd, 0x00); // Set lower column
ssd1306_send_command(fd, 0x10); // Set higher column
for (int col = 0; col < 128; col++) {
ssd1306_send_data(fd, 0x00);
}
}
}

void ssd1306_draw_char(int fd, uint8_t page, uint8_t col, char c) {
int idx = get_font_index(c);
ssd1306_send_command(fd, 0xB0 + page); // Set page
ssd1306_send_command(fd, col & 0x0F); // Lower col
ssd1306_send_command(fd, 0x10 | (col >> 4)); // Higher col


for (int i = 0; i < 5; i++) {
    ssd1306_send_data(fd, font5x7[idx][i]);
}
ssd1306_send_data(fd, 0x00); // space between chars
}

void ssd1306_draw_string(int fd, uint8_t page, uint8_t col, const char *str) {
while (*str && col < 128 - 6) {
ssd1306_draw_char(fd, page, col, *str++);
col += 6;
}
}

void ssd1306_start_scroll_left(int fd, uint8_t start_page, uint8_t end_page) {
ssd1306_send_command(fd, 0x27); // Left horizontal scroll
ssd1306_send_command(fd, 0x00); // Dummy
ssd1306_send_command(fd, start_page); // Start page
ssd1306_send_command(fd, 0x00); // Scroll speed (frame interval)
ssd1306_send_command(fd, end_page); // End page
ssd1306_send_command(fd, 0x00); // Dummy
ssd1306_send_command(fd, 0xFF); // Dummy
ssd1306_send_command(fd, 0x2F); // Activate scroll
}

void ssd1306_stop_scroll(int fd) {
ssd1306_send_command(fd, 0x2E);
}

int main() {
int fd = open(I2C_DEV, O_RDWR);
if (fd < 0) {
perror("Failed to open I2C device");
return 1;
}


if (ioctl(fd, I2C_SLAVE, SSD1306_ADDR) < 0) {
    perror("Failed to set I2C address");
    close(fd);
    return 1;
}

ssd1306_init(fd);
ssd1306_clear(fd);

ssd1306_draw_string(fd, 3, 0, "HHH12345"); // hiển thị tại page 3

sleep(2); // chờ 2 giây
ssd1306_start_scroll_left(fd, 3, 3); // cuộn dòng page 3

sleep(10); // cuộn trong 10 giây
ssd1306_stop_scroll(fd);

close(fd);
return 0;
}
