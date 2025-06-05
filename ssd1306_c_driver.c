#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>         // For uint8_t
#include <fcntl.h>          // For O_RDWR
#include <unistd.h>         // For write(), close(), usleep()
#include <sys/ioctl.h>      // For ioctl()
#include <linux/i2c-dev.h>  // For I2C_SLAVE
#include <string.h>         // For memset(), memcpy()
#include <time.h>           // For nanosleep()

// =========================================================================
// Bước 1: Khai báo hằng số và biến toàn cục
// =========================================================================

// Địa chỉ I2C của SSD1306 (thường là 0x3C, kiểm tra bằng i2cdetect -y 1)
#define SSD1306_I2C_ADDR         0x3C

// Tên file device I2C (thường là /dev/i2c-1 trên Raspberry Pi)
#define I2C_BUS_PATH             "/dev/i2c-1"

// Kích thước màn hình
#define SSD1306_WIDTH            128
#define SSD1306_HEIGHT           64 // Thay đổi thành 32 nếu bạn dùng màn 128x32

#if (SSD1306_HEIGHT == 64)
#define SSD1306_PAGES            8
#elif (SSD1306_HEIGHT == 32)
#define SSD1306_PAGES            4
#else
#error "Unsupported screen height. Only 32 or 64 are supported."
#endif

#define SSD1306_BUFFER_SIZE      (SSD1306_WIDTH * SSD1306_PAGES)

// Control byte
#define SSD1306_COMMAND_MODE     0x00 // Co = 0, D/C# = 0
#define SSD1306_DATA_MODE        0x40 // Co = 0, D/C# = 1

// Fundamental Command Table
#define SSD1306_SET_CONTRAST                  0x81
#define SSD1306_DISPLAY_ALL_ON_RESUME         0xA4
#define SSD1306_DISPLAY_ALL_ON_IGNORE_RAM     0xA5
#define SSD1306_NORMAL_DISPLAY                0xA6
#define SSD1306_INVERT_DISPLAY                0xA7
#define SSD1306_DISPLAY_OFF                   0xAE
#define SSD1306_DISPLAY_ON                    0xAF

// Addressing Setting Command Table
#define SSD1306_SET_MEMORY_ADDR_MODE          0x20
#define SSD1306_SET_COLUMN_ADDR               0x21
#define SSD1306_SET_PAGE_ADDR                 0x22
// For Page Addressing Mode only
#define SSD1306_SET_PAGE_START_ADDR_CM        0xB0 // For command mode, add page number (0-7)

// Hardware Configuration Command Table
#define SSD1306_SET_DISPLAY_START_LINE_CMD    0x40 // Add line number (0-63)
#define SSD1306_SET_SEGMENT_REMAP_NORMAL      0xA0 // Column 0 maps to SEG0
#define SSD1306_SET_SEGMENT_REMAP_REVERSE     0xA1 // Column 127 maps to SEG0 (default for many modules)
#define SSD1306_SET_MULTIPLEX_RATIO           0xA8
#define SSD1306_SET_COM_OUTPUT_SCAN_DIR_NORMAL 0xC0 // Scan from COM0 to COM[N-1]
#define SSD1306_SET_COM_OUTPUT_SCAN_DIR_REMAPPED 0xC8 // Scan from COM[N-1] to COM0 (default for many modules)
#define SSD1306_SET_DISPLAY_OFFSET            0xD3
#define SSD1306_SET_COM_PINS_HW_CONFIG        0xDA

// Timing & Driving Scheme Setting Command Table
#define SSD1306_SET_DISPLAY_CLOCK_DIV         0xD5
#define SSD1306_SET_PRECHARGE_PERIOD          0xD9
#define SSD1306_SET_VCOMH_DESELECT_LEVEL      0xDB
#define SSD1306_NOP                           0xE3

// Charge Pump Command Table
#define SSD1306_CHARGE_PUMP_SETTING           0x8D
#define SSD1306_CHARGE_PUMP_ENABLE            0x14
#define SSD1306_CHARGE_PUMP_DISABLE           0x10

// Biến toàn cục
int i2c_fd = -1; // File descriptor cho bus I2C, khởi tạo là không hợp lệ
uint8_t display_buffer[SSD1306_BUFFER_SIZE]; // Bộ đệm màn hình

// =========================================================================
// Bước 2: Các hàm giao tiếp I2C
// =========================================================================

void delay_ms(long milliseconds) {
    struct timespec req, rem;
    if (milliseconds <= 0) return;
    req.tv_sec = milliseconds / 1000;
    req.tv_nsec = (milliseconds % 1000) * 1000000L;
    while (nanosleep(&req, &rem) == -1) {
        // Nếu bị ngắt, tiếp tục ngủ phần thời gian còn lại
        req = rem; 
    }
}

int ssd1306_open_i2c() {
    i2c_fd = open(I2C_BUS_PATH, O_RDWR);
    if (i2c_fd < 0) {
        perror("Failed to open I2C bus");
        return -1;
    }
    if (ioctl(i2c_fd, I2C_SLAVE, SSD1306_I2C_ADDR) < 0) {
        perror("Failed to set I2C slave address");
        close(i2c_fd);
        i2c_fd = -1;
        return -1;
    }
    printf("I2C bus %s opened and slave address 0x%X set.\n", I2C_BUS_PATH, SSD1306_I2C_ADDR);
    return 0;
}

void ssd1306_close_i2c() {
    if (i2c_fd >= 0) {
        close(i2c_fd);
        i2c_fd = -1;
        printf("I2C bus closed.\n");
    }
}

// Gửi một byte lệnh duy nhất
int ssd1306_send_single_command(uint8_t command) {
    if (i2c_fd < 0) {
        fprintf(stderr, "Error: I2C bus not open for sending command.\n");
        return -1;
    }
    uint8_t buffer[2];
    buffer[0] = SSD1306_COMMAND_MODE;
    buffer[1] = command;
    if (write(i2c_fd, buffer, 2) != 2) {
        perror("Failed to write single command to I2C");
        return -1;
    }
    return 0;
}

// Gửi một lệnh có một tham số
int ssd1306_send_command_1param(uint8_t command, uint8_t param1) {
    if (i2c_fd < 0) return -1;
    uint8_t buffer[3];
    buffer[0] = SSD1306_COMMAND_MODE;
    buffer[1] = command;
    buffer[2] = param1;
    if (write(i2c_fd, buffer, 3) != 3) {
        perror("Failed to write command with 1 param to I2C");
        return -1;
    }
    return 0;
}

// Gửi một lệnh có hai tham số
int ssd1306_send_command_2params(uint8_t command, uint8_t param1, uint8_t param2) {
    if (i2c_fd < 0) return -1;
    uint8_t buffer[4];
    buffer[0] = SSD1306_COMMAND_MODE;
    buffer[1] = command;
    buffer[2] = param1;
    buffer[3] = param2;
    if (write(i2c_fd, buffer, 4) != 4) {
        perror("Failed to write command with 2 params to I2C");
        return -1;
    }
    return 0;
}

// Gửi một chuỗi các byte dưới dạng lệnh (ví dụ: một chuỗi khởi tạo dài)
// Buffer đầu vào commands KHÔNG chứa control byte 0x00. Hàm này sẽ thêm nó vào.
int ssd1306_send_command_sequence(const uint8_t *commands, int len) {
    if (i2c_fd < 0) {
        fprintf(stderr, "Error: I2C bus not open for sending command sequence.\n");
        return -1;
    }
    if (len <= 0 || commands == NULL) {
        fprintf(stderr, "Error: Invalid arguments for send_command_sequence.\n");
        return -1;
    }

    uint8_t *buffer = (uint8_t *)malloc(len + 1);
    if (buffer == NULL) {
        perror("Failed to allocate memory for command sequence buffer");
        return -1;
    }

    buffer[0] = SSD1306_COMMAND_MODE;
    memcpy(buffer + 1, commands, len);

    if (write(i2c_fd, buffer, len + 1) != (len + 1)) {
        perror("Failed to write command sequence to I2C");
        free(buffer);
        return -1;
    }
    free(buffer);
    return 0;
}


int ssd1306_send_data(const uint8_t *data, int len) {
    if (i2c_fd < 0) {
        fprintf(stderr, "Error: I2C bus not open for sending data.\n");
        return -1;
    }
    if (len <= 0 || data == NULL) {
        fprintf(stderr, "Error: Invalid arguments for send_data.\n");
        return -1;
    }

    uint8_t *buffer = (uint8_t *)malloc(len + 1);
    if (buffer == NULL) {
        perror("Failed to allocate memory for data buffer");
        return -1;
    }

    buffer[0] = SSD1306_DATA_MODE;
    memcpy(buffer + 1, data, len);

    // Gửi dữ liệu. I2C có giới hạn về kích thước gói (thường là 32 byte cho write_i2c_block_data trên RPi,
    // nhưng write() trực tiếp có thể xử lý nhiều hơn, tùy thuộc vào driver).
    // Để an toàn, có thể chia thành nhiều lần ghi nếu len lớn.
    // Tuy nhiên, với 1024 byte, write một lần thường vẫn ổn.
    if (write(i2c_fd, buffer, len + 1) != (len + 1)) {
        perror("Failed to write data to I2C");
        free(buffer);
        return -1;
    }
    free(buffer);
    return 0;
}

// =========================================================================
// Bước 3: Hàm khởi tạo màn hình
// =========================================================================
int ssd1306_init() {
    // Chuỗi lệnh khởi tạo này khá chuẩn cho nhiều màn hình SSD1306 128x64.
    // Tham khảo datasheet để tùy chỉnh nếu cần.
    ssd1306_send_single_command(SSD1306_DISPLAY_OFF);               // 0xAE

    ssd1306_send_command_1param(SSD1306_SET_DISPLAY_CLOCK_DIV, 0x80); // 0xD5, 0x80
    
    ssd1306_send_command_1param(SSD1306_SET_MULTIPLEX_RATIO, SSD1306_HEIGHT - 1); // 0xA8, 0x3F (cho 64) hoặc 0x1F (cho 32)
    
    ssd1306_send_command_1param(SSD1306_SET_DISPLAY_OFFSET, 0x00);   // 0xD3, 0x00
    
    ssd1306_send_single_command(SSD1306_SET_DISPLAY_START_LINE_CMD | 0x00); // 0x40 | 0x00 (start line 0)
    
    ssd1306_send_command_1param(SSD1306_CHARGE_PUMP_SETTING, SSD1306_CHARGE_PUMP_ENABLE); // 0x8D, 0x14
    
    ssd1306_send_command_1param(SSD1306_SET_MEMORY_ADDR_MODE, 0x00); // 0x20, 0x00 (Horizontal Addressing Mode)
                                                                  // 0x02 for Page Addressing Mode (default)

    // Remap (điều chỉnh nếu hình ảnh bị lật hoặc đảo ngược)
    ssd1306_send_single_command(SSD1306_SET_SEGMENT_REMAP_REVERSE);  // 0xA1 (hoặc 0xA0)
    ssd1306_send_single_command(SSD1306_SET_COM_OUTPUT_SCAN_DIR_REMAPPED); // 0xC8 (hoặc 0xC0)

#if (SSD1306_HEIGHT == 64)
    ssd1306_send_command_1param(SSD1306_SET_COM_PINS_HW_CONFIG, 0x12); // 0xDA, 0x12
#elif (SSD1306_HEIGHT == 32)
    ssd1306_send_command_1param(SSD1306_SET_COM_PINS_HW_CONFIG, 0x02); // 0xDA, 0x02 (kiểm tra datasheet nếu là 128x32)
#endif

    ssd1306_send_command_1param(SSD1306_SET_CONTRAST, 0xCF);         // 0x81, 0xCF (độ tương phản, thử giá trị khác)
    
    ssd1306_send_command_1param(SSD1306_SET_PRECHARGE_PERIOD, 0xF1); // 0xD9, 0xF1 (hoặc 0x22 nếu dùng VCC ngoài)
    
    ssd1306_send_command_1param(SSD1306_SET_VCOMH_DESELECT_LEVEL, 0x40); // 0xDB, 0x40 (hoặc 0x30)

    ssd1306_send_single_command(SSD1306_DISPLAY_ALL_ON_RESUME);     // 0xA4 (hiển thị nội dung từ RAM)
    
    ssd1306_send_single_command(SSD1306_NORMAL_DISPLAY);            // 0xA6
    
    // Xóa bộ đệm trước khi bật màn hình
    memset(display_buffer, 0x00, SSD1306_BUFFER_SIZE);
    // Gửi bộ đệm trống lên màn hình (không bắt buộc nhưng tốt)
    // ssd1306_display_buffer(); // Sẽ được định nghĩa ở Bước 4

    ssd1306_send_single_command(SSD1306_DISPLAY_ON);                // 0xAF

    printf("SSD1306 initialized.\n");
    return 0;
}

// =========================================================================
// Bước 4: Các hàm tiện ích (clear, display, draw_pixel)
// =========================================================================
void ssd1306_clear_buffer() {
    memset(display_buffer, 0x00, SSD1306_BUFFER_SIZE);
}

void ssd1306_fill_buffer(uint8_t pattern) {
    memset(display_buffer, pattern, SSD1306_BUFFER_SIZE);
}

void ssd1306_display_buffer() {
    if (i2c_fd < 0) {
        fprintf(stderr, "Error: I2C not initialized for display_buffer.\n");
        return;
    }

    // Thiết lập lại con trỏ cột và trang cho Horizontal Addressing Mode
    // Lệnh này nói rằng chúng ta sẽ ghi vào toàn bộ vùng nhớ màn hình
    ssd1306_send_command_2params(SSD1306_SET_COLUMN_ADDR, 0, SSD1306_WIDTH - 1);
    ssd1306_send_command_2params(SSD1306_SET_PAGE_ADDR, 0, SSD1306_PAGES - 1);

    // Gửi toàn bộ buffer dữ liệu
    if (ssd1306_send_data(display_buffer, SSD1306_BUFFER_SIZE) != 0) {
        fprintf(stderr, "Error sending display buffer to SSD1306\n");
    }
    // printf("Buffer displayed.\n"); // Bỏ comment nếu muốn thấy log này
}

void ssd1306_draw_pixel(int x, int y, int color) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) {
        return; // Ngoài màn hình
    }

    int page = y / 8;
    int bit_offset_in_page = y % 8;
    // Trong Horizontal Addressing Mode, buffer được sắp xếp theo từng byte cho mỗi cột, tuần tự qua các trang
    int byte_index_in_buffer = x + (page * SSD1306_WIDTH);

    if (byte_index_in_buffer >= SSD1306_BUFFER_SIZE || byte_index_in_buffer < 0) { // Kiểm tra an toàn
        fprintf(stderr, "Pixel index out of bounds: x=%d, y=%d -> index=%d\n", x, y, byte_index_in_buffer);
        return;
    }

    if (color) { // Bật pixel (trắng)
        display_buffer[byte_index_in_buffer] |= (1 << bit_offset_in_page);
    } else { // Tắt pixel (đen)
        display_buffer[byte_index_in_buffer] &= ~(1 << bit_offset_in_page);
    }
}

// =========================================================================
// Bước 5: Hàm main để thử nghiệm
// =========================================================================
int main() {
    printf("Starting SSD1306 C driver test...\n");

    if (ssd1306_open_i2c() != 0) {
        return 1;
    }

    if (ssd1306_init() != 0) {
        fprintf(stderr, "Failed to initialize SSD1306.\n");
        ssd1306_close_i2c();
        return 1;
    }
    
    // Xóa buffer sau khi init (init có thể đã làm nhưng để chắc chắn)
    ssd1306_clear_buffer();
    ssd1306_display_buffer(); // Hiển thị màn hình trống
    delay_ms(500); // Chờ 0.5 giây

    // 1. Xóa màn hình (đổ màu đen vào buffer và hiển thị)
    printf("Clearing screen (setting buffer to 0x00)...\n");
    ssd1306_clear_buffer();
    ssd1306_display_buffer();
    delay_ms(1000); // Chờ 1 giây

    // 2. Vẽ một vài pixel
    printf("Drawing pixels...\n");
    ssd1306_draw_pixel(0, 0, 1);                            // Góc trên trái
    ssd1306_draw_pixel(SSD1306_WIDTH - 1, 0, 1);            // Góc trên phải
    ssd1306_draw_pixel(0, SSD1306_HEIGHT - 1, 1);           // Góc dưới trái
    ssd1306_draw_pixel(SSD1306_WIDTH - 1, SSD1306_HEIGHT - 1, 1); // Góc dưới phải
    ssd1306_draw_pixel(SSD1306_WIDTH / 2, SSD1306_HEIGHT / 2, 1); // Điểm giữa

    // Vẽ đường chéo từ (0,0) đến (HEIGHT-1, HEIGHT-1)
    for (int i = 0; i < SSD1306_HEIGHT; ++i) {
        if (i < SSD1306_WIDTH) { // Đảm bảo không vẽ ra ngoài nếu màn hình không vuông
             ssd1306_draw_pixel(i, i, 1);
        }
    }
    ssd1306_display_buffer(); // Hiển thị các pixel đã vẽ
    delay_ms(2000); // Chờ 2 giây

    // 3. Đổ đầy màn hình (tất cả pixel màu trắng)
    printf("Filling screen (setting buffer to 0xFF)...\n");
    ssd1306_fill_buffer(0xFF); // 0xFF để bật tất cả các pixel
    ssd1306_display_buffer();
    delay_ms(2000); // Chờ 2 giây

    // 4. Xóa lại màn hình
    printf("Clearing screen again...\n");
    ssd1306_clear_buffer();
    ssd1306_display_buffer();
    delay_ms(1000);

    // Tùy chọn: Tắt màn hình khi kết thúc
    // printf("Turning display OFF.\n");
    // ssd1306_send_single_command(SSD1306_DISPLAY_OFF);

    ssd1306_close_i2c();
    printf("SSD1306 C driver test finished.\n");
    return 0;
}
