#include <linux/module.h>
#include <linux/spi/spi.h>

static int spi_dev_probe(struct spi_device *spi) {
    pr_info("SPI device probed: %s\n", spi->modalias);
    return 0;
}

static int spi_dev_remove(struct spi_device *spi) {
    pr_info("SPI device removed\n");
    return 0;
}

static const struct spi_device_id spi_dev_id[] = {
    { "spi_device", 0 },
    { }
};

static struct spi_driver spi_dev_driver = {
    .driver = {
        .name = "spi_device_driver",
        .owner = THIS_MODULE,
    },
    .probe = spi_dev_probe,
    .remove = spi_dev_remove,
    .id_table = spi_dev_id,
};

module_spi_driver(spi_dev_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("SPI Device Driver");
MODULE_LICENSE("GPL");
