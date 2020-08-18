#pragma once

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace smbus
{

class Smbus
{
  public:
    Smbus(){};

    int openI2cDev(int i2cbus, char* filename, size_t size, int quiet);

    int smbusInit(int smbus_num);

    void smbusClose(int smbus_num);

};

} // namespace smbus
