// SPDX-License-Identifier: BSD-3-Clause

/*
 * SH110x Display Driver
 *
 * Author: Spencer Chang
 *    spencer@sycee.xyz
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/of_device.h>
#include "font.h"

#define SH1107_SLAVE_ADDR      (       0x3C )              // SH1107 OLED Slave Address
#define SH1107_MAX_COL         (         64 )              // Maximum segment
#define SH1107_MAX_PAGE        (         16 )              // Maximum line
#define MAX_LINES              ((SH1107_MAX_COL+1)/(SH110X_FONT_HEIGHT+1))

struct sh110x_data {
    struct bin_attribute bin;
    struct device *dev;
    struct i2c_client *client;

    uint8_t line_num;
    uint8_t cursor_pos;
};

/*
** Array Variable to store the letters.
*/


static int i2c_sh110x_write(struct i2c_client *client, bool is_cmd, unsigned char data) {
    unsigned char buf[2] = {0};

    if (is_cmd)
        buf[0] = 0x00; // commands have 0x0000_0000 as the address
    else 
        buf[0] = 0x40; // data has 0x0100_0000 as the address

    buf[1] = data;

    return i2c_master_send(client, buf, 2);
}

// sets the location of the cursor
static int i2c_sh1107_set_cursor(struct i2c_client *client, uint8_t line_num, uint8_t cursor_pos) {
    struct sh110x_data *sh110x = i2c_get_clientdata(client);

    if ((cursor_pos >= SH1107_MAX_PAGE) || (line_num >= MAX_LINES)) {
        dev_err(sh110x->dev, "Cursor out of bounds.");
        return 1;
    }

    sh110x->line_num   = line_num;            // Save the specified line number
    sh110x->cursor_pos = cursor_pos;          // Save the specified cursor position

    cursor_pos = SH1107_MAX_PAGE - cursor_pos - 1;
    line_num = line_num*(SH110X_FONT_HEIGHT+1) + 32;

    // set column start and end address
    i2c_sh110x_write(client, true, (0x01 << 4) | (line_num >> 4));
    i2c_sh110x_write(client, true, (0x00 << 4) | (line_num & 0xf));

    // set page address
    i2c_sh110x_write(client, true, (0xB << 4) | (cursor_pos & 0xf));
    return 0;
}

// prints character to the screen
static void i2c_sh1107_print_char(struct i2c_client *client, unsigned char c) {
    struct sh110x_data *sh110x = i2c_get_clientdata(client);
    uint8_t data_byte;
    uint8_t col_i = 0;

    if ((sh110x->cursor_pos >= SH1107_MAX_PAGE) ||
        ( c == '\n' )) {

        // go to next line
        sh110x->line_num = sh110x->line_num < MAX_LINES-1 ? sh110x->line_num + 1: 0;
        i2c_sh1107_set_cursor(client, sh110x->line_num, 0);
    }
    if ( c != '\n' ) {
        c -= 0x20;
        do {
            data_byte = sh110x_12x8[c][col_i];
            i2c_sh110x_write(client, false, data_byte);

            col_i++;
        } while (col_i < SH110X_FONT_HEIGHT);

        i2c_sh1107_set_cursor(client, sh110x->line_num, ++sh110x->cursor_pos);
    }
}

// sets the brightness of the display
static void SH1107_SetBrightness(struct i2c_client *client, uint8_t brightness) {
    i2c_sh110x_write(client, true, 0x81); // brightness addr
    i2c_sh110x_write(client, true, brightness);
}

// fills the display with the given value
static void i2c_sh110x_fill(struct i2c_client *client, unsigned char data) {
    int col, page;

    for (page = 0; page < SH1107_MAX_PAGE; page++) {
        i2c_sh1107_set_cursor(client, 0, page);
        for (col = 0; col < 128; col++) {
            i2c_sh110x_write(client, false, data);
        }
    }
    i2c_sh1107_set_cursor(client, 0, 0);
}

// initializes the display to some default values
static int sh1107_display_init(struct i2c_client *client) {
    msleep(100);

    // display off
    i2c_sh110x_write(client, true, 0xAE);

    // page addressing mode
    i2c_sh110x_write(client, true, 0x20);

    // set brightness
    SH1107_SetBrightness(client, 0x2f);

    // set output scan direction
    i2c_sh110x_write(client, true, 0xc0);

    // set multiplex ratio
    i2c_sh110x_write(client, true, 0xa8);
    i2c_sh110x_write(client, true, 0x7f);

    // set display clock freq -- match fosc (POR)
    i2c_sh110x_write(client, true, 0xd5);
    i2c_sh110x_write(client, true, (0x5 << 4) | (0x1 & 0xf));

    // pre-charge/discharge period -- 2 DCLK (POR)
    i2c_sh110x_write(client, true, 0xd9);
    i2c_sh110x_write(client, true, 0x22);

    // vcom deselect level -- 0.77 (POR)
    i2c_sh110x_write(client, true, 0xdb);
    i2c_sh110x_write(client, true, 0x35);

    // set cursor to top left
    i2c_sh1107_set_cursor(client, 0, 0);

    // display on, non-reversed display
    i2c_sh110x_write(client, true, 0xa4);
    i2c_sh110x_write(client, true, 0xa6);

    i2c_sh110x_write(client, true, 0xAF);

    i2c_sh110x_fill(client, 0x00);

    return 0;
}

static ssize_t i2c_sh110x_bin_write(struct file *filp, struct kobject *kobj,
        struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
    struct sh110x_data *sh110x;
    int i;

    sh110x = dev_get_drvdata(kobj_to_dev(kobj));

    if (count == 1 && buf[0] == ' ') {
        i2c_sh110x_fill(sh110x->client, 0x00);
        return count;
    }

    for(i = off; i < count; i++) {
        i2c_sh1107_print_char(sh110x->client, buf[i]);
    }

    return count;
}

static const struct of_device_id i2c_sh110x_of_match[] = {
    { .compatible = "sinowealth,sh110x" },
    {}
};
MODULE_DEVICE_TABLE(of, i2c_sh110x_of_match);

// probed entry point
static int i2c_sh110x_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    int ret;
    u8 brightness;
    bool screen_inverted;
    struct sh110x_data *sh110x;

    // we may have been probed for a number of reasons, if a device tree probed us we should read it
    const struct of_device_id *of_match;
    of_match = of_match_device(i2c_sh110x_of_match, &client->dev);
    if (of_match) {
        of_property_read_u8(client->dev.of_node, "brightness", &brightness);
        screen_inverted = of_property_read_bool(client->dev.of_node, "inverted");
    }

    // initialize our private data structure
    sh110x = devm_kzalloc(&client->dev, sizeof(struct sh110x_data), GFP_KERNEL);
    if (!sh110x)
        return -ENOMEM;

    sh110x->dev = &client->dev;
    sh110x->line_num = 0;
    sh110x->cursor_pos = 0;
    sh110x->client = client;
    i2c_set_clientdata(client, sh110x);

    dev_info(&client->dev, "Initializing %s on bus %s", client->name, client->adapter->name);

    // initialize the device driver
    sh1107_display_init(client);
    i2c_sh1107_set_cursor(client, 0,0);

    if (!brightness)
        i2c_sh110x_write(client, true, brightness);

    if (screen_inverted)
        i2c_sh110x_write(client, true, 0xa7);

    // create a sysfs file to write/read the data
    sysfs_bin_attr_init(&sh110x->bin);
    sh110x->bin.attr.name = "screen_content";
    sh110x->bin.attr.mode = S_IWUSR;
    sh110x->bin.write = i2c_sh110x_bin_write;
    sh110x->bin.size = 0;

    ret = sysfs_create_bin_file(&client->dev.kobj, &sh110x->bin);
    if (ret)
        return ret;

    return 0;
}

static int i2c_sh110x_remove(struct i2c_client *client)
{
    struct sh110x_data *sh110x = i2c_get_clientdata(client);

    sysfs_remove_bin_file(&client->dev.kobj, &sh110x->bin);

    i2c_sh1107_set_cursor(client, 0, 0);
    i2c_sh110x_fill(client, 0x00);

    // display off
    i2c_sh110x_write(client, true, 0xAE);

    return 0;
}

static const struct i2c_device_id i2c_sh110x_id[] = {
    { "SH110X-OLED", 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, i2c_sh110x_id);

static struct i2c_driver i2c_sh110x_driver = {
    .driver = {
        .name = "i2c-sh110x",
        .of_match_table = i2c_sh110x_of_match,
    },
    .probe = i2c_sh110x_probe,
    .remove = i2c_sh110x_remove,
    .id_table = i2c_sh110x_id,
};
module_i2c_driver(i2c_sh110x_driver);

MODULE_AUTHOR("Spencer Chang <spencer@sycee.xyz");
MODULE_DESCRIPTION("SH110X I2C Driver");
MODULE_LICENSE("GPL");