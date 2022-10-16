SUMMARY = "I2C SH110X Display Driver Kernel Module"
SECTION = "kernel"
LICENSE = "MIT"

LIC_FILES_CHKSUM = " \
    file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit module

SRC_URI = " \
    file://i2c-sh110x \
"

S = "${WORKDIR}/i2c-sh110x"
