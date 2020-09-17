#include <NVMeSensor.hpp>
#include <dbus/connection.hpp>
#include <nlohmann/json.hpp>

#include "util.hpp"

#include <cstring>
#include <stdexcept>
#include <stdplus/raw.hpp>
#include <string>
#include <vector>
#include <fstream>

#include "gtest/gtest.h"

namespace phosphor
{
namespace nvme
{

TEST(getNVMeInfobyBusID, TestgetNVMeInfobyBusID)
{

    int busID = 0;

    bool getData = getNVMeInfobyBusID(busID, phosphor::nvme::Nvme::NVMeData& nvmeData);
    EXPECT_THROW(getData, std::runtime_error);
    EXPECT_EQ(true, getData);
}

} // namespace nvme
} // namespace phosphor
