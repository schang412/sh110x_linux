# OLED SH110X Linux Driver

This is an kernel module Linux driver for SH110X devices. Currently, only SH1107 is supported. The expected usage is 
to be probed through the device tree.

## How to use

Instantiate a device on the I2C bus: 
```c
i2c0: i2c@41e00000 {
    oled_screen: oled@3c {
    compatible = "sinowealth,sh110x";
    reg = <0x3c>;
    brightness = 200;
    };
};
```

To write to the screen: 
```shell
echo -n " " > /sys/bus/i2c/devices/0-003c/screen_content # this clears the screen
echo "hello world" > /sys/bus/i2c/devices/0-003c/screen_content
```

