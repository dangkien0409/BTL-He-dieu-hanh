#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#define SSD1306_I2C_ADDRESS 0x3C

// hàm probe  trả về int
static int ssd1306_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    pr_info("SSD1306 I2C device probed\n");

    // Gửi lệnh khởi tạo SSD1306 qua I2C
    i2c_smbus_write_byte_data(client, 0x00, 0xAE);  // Display OFF
    i2c_smbus_write_byte_data(client, 0x00, 0xA8);  // Set multiplex ratio
    i2c_smbus_write_byte_data(client, 0x00, 0x3F);  // Max multiplex
    i2c_smbus_write_byte_data(client, 0x00, 0xD3);  // Set display offset
    i2c_smbus_write_byte_data(client, 0x00, 0x00);  // No offset
    i2c_smbus_write_byte_data(client, 0x00, 0x40);  // Set display start line
    i2c_smbus_write_byte_data(client, 0x00, 0xA4);  // Display RAM content

    return 0; // Trả về 0 sau khi khởi tạo thành công
}

// hàm remove trả về void
static void ssd1306_remove(struct i2c_client *client)
{
    pr_info("SSD1306 I2C device removed\n");
}

static const struct i2c_device_id ssd1306_id[] = {
    { "ssd1306", 0 },
    { }
};

static struct i2c_driver ssd1306_driver = {
    .driver = {
        .name = "ssd1306",
    },
    .probe = ssd1306_probe,  //  int
    .remove = ssd1306_remove,  // void
    .id_table = ssd1306_id,
};

module_i2c_driver(ssd1306_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("SSD1306 OLED I2C Driver for Raspberry Pi");
MODULE_LICENSE("GPL");