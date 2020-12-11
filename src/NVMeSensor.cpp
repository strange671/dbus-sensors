/*
// Copyright (c) 2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "NVMeSensor.hpp"

#include "i2c.h"

#include "NVMeDevice.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <iostream>

static constexpr double maxReading = 127;
static constexpr double minReading = 0;

static constexpr bool DEBUG = false;

void rxMessage(uint8_t eid, void* data, void* msg, size_t len);

namespace nvmeSMBus
{

static boost::container::flat_map<int, int> busfd;
int OpenI2cDev(int i2cbus, char* filename, size_t size, int quiet)
{
    int file;

    snprintf(filename, size, "/dev/i2c/%d", i2cbus);
    filename[size - 1] = '\0';
    file = open(filename, O_RDWR);

    if (file < 0 && (errno == ENOENT || errno == ENOTDIR))
    {
        sprintf(filename, "/dev/i2c-%d", i2cbus);
        file = open(filename, O_RDWR);
    }

    if (DEBUG)
    {
        if (file < 0 && !quiet)
        {
            if (errno == ENOENT)
            {
                fprintf(stderr,
                        "Error: Could not open file "
                        "`/dev/i2c-%d' or `/dev/i2c/%d': %s\n",
                        i2cbus, i2cbus, strerror(ENOENT));
            }
            else
            {
                fprintf(stderr,
                        "Error: Could not open file "
                        "`%s': %s\n",
                        filename, strerror(errno));
                if (errno == EACCES)
                    fprintf(stderr, "Run as root?\n");
            }
        }
    }

    return file;
}

int SmbusInit(int smbus_num)
{
    int res = 0;
    char filename[20];
    //busfd[256] = {0}; // record:this need to be a class

    busfd[smbus_num] = OpenI2cDev(smbus_num, filename, sizeof(filename), 0);
    if (busfd[smbus_num] < 0)
    {

        return -1;
    }

    res = busfd[smbus_num];

    return res;
}

/*this function do not need when the fdbus been class */
void SmbusClose(int smbus_num)
{
        close(busfd[smbus_num]);
}

int SendSmbusRWBlockCmdRAW(int smbus_num, int8_t device_addr, uint8_t* tx_data,
                           uint8_t tx_len, uint8_t* rsp_data)
{
    int res, res_len;
    unsigned char Rx_buf[256] = {0};

    Rx_buf[0] = 1;

    res = i2c_read_after_write(busfd[smbus_num], device_addr, tx_len,
                               (unsigned char*)tx_data, I2C_DATA_MAX,
                               (unsigned char*)Rx_buf);

    if (res < 0)
    {
        fprintf(stderr, "Error: SendSmbusRWBlockCmdRAW failed\n");
    }

    res_len = Rx_buf[0] + 1;

    memcpy(rsp_data, Rx_buf, res_len);

    return res;
}

} // namespace nvmeSMBus
void readResponse(const std::shared_ptr<NVMeContext>& nvmeDevice)
{
    nvmeDevice->nvmeSlaveSocket.async_wait(
        boost::asio::ip::tcp::socket::wait_error,
        [nvmeDevice](const boost::system::error_code errorCode) {
            if (errorCode)
            {
                return;
            }
//            mctp_smbus_set_in_fd(nvmeMCTP::smbus,
//                                 nvmeMCTP::getInFd(nvmeDevice->rootBus));

            // through libmctp this will invoke rxMessage
//            mctp_smbus_read(nvmeMCTP::smbus);
        });

}

/* it's for nvme-mi use, block it and test build */
/*
int nvmeMessageTransmit(mctp& mctp, nvme_mi_msg_request& req)
{
    std::array<uint8_t, NVME_MI_MSG_BUFFER_SIZE> messageBuf = {};

    req.header.flags |= NVME_MI_HDR_MESSAGE_TYPE_MI_COMMAND
                        << NVME_MI_HDR_FLAG_MSG_TYPE_SHIFT;
    req.header.message_type =
        NVME_MI_MESSAGE_TYPE | NVME_MI_MCTP_INTEGRITY_CHECK;

    uint32_t integrity = 0;
    size_t msgSize = NVME_MI_MSG_REQUEST_HEADER_SIZE + req.request_data_len +
                     sizeof(integrity);

    if (sizeof(messageBuf) < msgSize)
    {
        return EXIT_FAILURE;
    }

    messageBuf[0] = req.header.message_type;
    messageBuf[1] = req.header.flags;
    // Reserved bytes 2-3

    messageBuf[4] = req.header.opcode;
    // reserved bytes 5-7
    messageBuf[8] = req.header.dword0 & 0xff;
    messageBuf[9] = (req.header.dword0 >> 8) & 0xff;
    messageBuf[10] = (req.header.dword0 >> 16) & 0xff;
    messageBuf[11] = (req.header.dword0 >> 24) & 0xff;

    messageBuf[12] = req.header.dword1 & 0xff;
    messageBuf[13] = (req.header.dword1 >> 8) & 0xff;
    messageBuf[14] = (req.header.dword1 >> 16) & 0xff;
    messageBuf[15] = (req.header.dword1 >> 24) & 0xff;

    std::copy_n(req.request_data, req.request_data_len,
                messageBuf.data() +
                    static_cast<uint8_t>(NVME_MI_MSG_REQUEST_HEADER_SIZE));

    msgSize = NVME_MI_MSG_REQUEST_HEADER_SIZE + req.request_data_len;
    integrity = nvmeMCTP::crc32c(messageBuf.data(),
                       NVME_MI_MSG_REQUEST_HEADER_SIZE + req.request_data_len);
    messageBuf[msgSize] = integrity & 0xff;
    messageBuf[msgSize + 1] = (integrity >> 8) & 0xff;
    messageBuf[msgSize + 2] = (integrity >> 16) & 0xff;
    messageBuf[msgSize + 3] = (integrity >> 24) & 0xff;
    msgSize += sizeof(integrity);

    return mctp_message_tx(&mctp, 0, messageBuf.data(), msgSize);
}

int verifyIntegrity(uint8_t* msg, size_t len)
{
    uint32_t msgIntegrity = {0};
    if (len < NVME_MI_MSG_RESPONSE_HEADER_SIZE + sizeof(msgIntegrity))
    {
        std::cerr << "Not enough bytes for nvme header and trailer\n";
        return -1;
    }

    msgIntegrity = (msg[len - 4]) + (msg[len - 3] << 8) + (msg[len - 2] << 16) +
                   (msg[len - 1] << 24);

    uint32_t calculateIntegrity = nvmeMCTP::crc32c(msg, len - sizeof(msgIntegrity));
    if (msgIntegrity != calculateIntegrity)
    {
        std::cerr << "CRC mismatch. Got=" << msgIntegrity
                  << " Expected=" << calculateIntegrity << "\n";
        return -1;
    }
    return 0;
}

void readAndProcessNVMeSensor(const std::shared_ptr<NVMeContext>& nvmeDevice)
{
    struct nvme_mi_msg_request requestMsg = {};
    requestMsg.header.opcode = NVME_MI_OPCODE_HEALTH_STATUS_POLL;
    requestMsg.header.dword0 = 0;
    requestMsg.header.dword1 = 0;
    int mctpResponseTimeout = 1;
    if (nvmeDevice->sensors.empty())
    {
        return;
    }

    std::shared_ptr<NVMeSensor>& sensor = nvmeDevice->sensors.front();

    // setup the timeout timer
    nvmeDevice->mctpResponseTimer.expires_from_now(
        boost::posix_time::seconds(mctpResponseTimeout));

    nvmeDevice->mctpResponseTimer.async_wait(
        [sensor, nvmeDevice](const boost::system::error_code errorCode) {
            if (errorCode)
            {
                // timer cancelled successfully
                return;
            }

            sensor->incrementError();

            // cycle it back
            nvmeDevice->sensors.pop_front();
            nvmeDevice->sensors.emplace_back(sensor);

            nvmeDevice->nvmeSlaveSocket.cancel();
        });

    readResponse(nvmeDevice);

    if (DEBUG)
    {
        std::cout << "Sending message to read data from Drive on bus: "
                  << sensor->bus << " , rootBus: " << nvmeDevice->rootBus
                  << " device: " << sensor->name << "\n";
    }
    mctp_smbus_set_out_fd(nvmeMCTP::smbus, nvmeMCTP::getOutFd(sensor->bus));
    int rc = nvmeMessageTransmit(*nvmeMCTP::mctp, requestMsg);

    if (rc != 0)
    {
        std::cerr << "Error sending request message to NVMe device\n";
    }
}
*/
static double getTemperatureReading(int8_t reading)
{

    if (reading == static_cast<int8_t>(0x80) ||
        reading == static_cast<int8_t>(0x81))
    {
        // 0x80 = No temperature data or temperature data is more the 5 s
        // old 0x81 = Temperature sensor failure
        return maxReading;
    }

    return reading;
}

/* also bock this to test build*/
/*
void rxMessage(uint8_t eid, void*, void* msg, size_t len)
{
    struct nvme_mi_msg_response_header header
    {};
    int inFd = mctp_smbus_get_in_fd(nvmeMCTP::smbus);
    int rootBus = nvmeMCTP::getRootBus(inFd);

    NVMEMap& nvmeMap = getNVMEMap();
    auto findMap = nvmeMap.find(rootBus);
    if (findMap == nvmeMap.end())
    {
        std::cerr << "Unable to lookup root bus " << rootBus << "\n";
        return;
    }
    std::shared_ptr<NVMeContext>& self = findMap->second;
    if (msg == nullptr)
    {
        std::cerr << "Bad message received\n";
        return;
    }

    if (len <= 0)
    {
        std::cerr << "Received message not long enough\n";
        return;
    }

    if (DEBUG)
    {
        std::cout << "Eid from the received messaged: " << eid << "\n";
    }

    uint8_t* messageData = static_cast<uint8_t*>(msg);

    if ((*messageData & NVME_MI_MESSAGE_TYPE_MASK) != NVME_MI_MESSAGE_TYPE)
    {
        std::cerr << "Got unknown type message_type="
                  << (*messageData & NVME_MI_MESSAGE_TYPE_MASK) << "\n";
        return;
    }

    if (len < NVME_MI_MSG_RESPONSE_HEADER_SIZE + sizeof(uint32_t))
    {
        std::cerr << "Not enough bytes for NVMe header and trailer\n";
        return;
    }

    if (verifyIntegrity(messageData, len) != 0)
    {
        std::cerr << "Verification of message integrity failed\n";
        return;
    }

    header.message_type = messageData[0];
    header.flags = messageData[1];
    header.status = messageData[4];

    if (header.status == NVME_MI_HDR_STATUS_MORE_PROCESSING_REQUIRED)
    {
        return;
    }

    if (header.status != NVME_MI_HDR_STATUS_SUCCESS)
    {
        std::cerr << "Command failed with status= " << header.status << "\n";
        return;
    }

    messageData += NVME_MI_MSG_RESPONSE_HEADER_SIZE;
    size_t messageLength =
        len - NVME_MI_MSG_RESPONSE_HEADER_SIZE - sizeof(uint32_t);
    if (((header.flags >> NVME_MI_HDR_FLAG_MSG_TYPE_SHIFT) &
         NVME_MI_HDR_FLAG_MSG_TYPE_MASK) != NVME_MI_HDR_MESSAGE_TYPE_MI_COMMAND)
    {
        std::cerr << "Not MI type comamnd\n";
        return;
    }

    if (messageLength < NVME_MI_HEALTH_STATUS_POLL_MSG_MIN)
    {
        std::cerr << "Got improperly sized health status poll\n";
        return;
    }

    std::shared_ptr<NVMeSensor> sensorInfo = self->sensors.front();
    if (DEBUG)
    {
        std::cout << "Temperature Reading: "
                  << getTemperatureReading(messageData[5])
                  << " Celsius for device " << sensorInfo->name << "\n";
    }

    sensorInfo->updateValue(getTemperatureReading(messageData[5]));

    if (DEBUG)
    {
        std::cout << "Cancelling the timer now\n";
    }

    // move to back of scan queue
    self->sensors.pop_front();
    self->sensors.emplace_back(sensorInfo);

    self->mctpResponseTimer.cancel();
}
*/
NVMeContext::NVMeContext(boost::asio::io_service& io, int rootBus) :
    rootBus(rootBus) , scanTimer(io), nvmeSlaveSocket(io), mctpResponseTimer(io)
{
//    nvmeSlaveSocket.assign(boost::asio::ip::tcp::v4(),
//                           nvmeMCTP::getInFd(rootBus));
}  // NVMeMCTPContext

void NVMeContext::pollNVMeDevices()
{
    scanTimer.expires_from_now(boost::posix_time::seconds(1));
    scanTimer.async_wait(
        [self{shared_from_this()}](const boost::system::error_code errorCode) {
            if (errorCode == boost::asio::error::operation_aborted)
            {
                return; // we're being canceled
            }
            else if (errorCode)
            {
                std::cerr << "Error:" << errorCode.message() << "\n";
                return;
            }
            else
            {
//                readAndProcessNVMeSensor(self);
            }

            self->pollNVMeDevices();
        });
}

void NVMeContext::close()
{
//    scanTimer.cancel();
//    mctpResponseTimer.cancel();
//    nvmeSlaveSocket.cancel();
    nvmeSMBus::SmbusClose(rootBus);
}

NVMeContext::~NVMeContext()
{
    close();
}

NVMeSMBusContext::~NVMeSMBusContext()
{
}

NVMeSensor::NVMeSensor(sdbusplus::asio::object_server& objectServer,
                       boost::asio::io_service&,
                       std::shared_ptr<sdbusplus::asio::connection>& conn,
                       const std::string& sensorName,
                       std::vector<thresholds::Threshold>&& _thresholds,
                       const std::string& sensorConfiguration,
                       const int busNumber) :
    Sensor(boost::replace_all_copy(sensorName, " ", "_"),
           std::move(_thresholds), sensorConfiguration,
           "xyz.openbmc_project.Configuration.NVMe", maxReading, minReading,
           conn, PowerState::on),
    objServer(objectServer), bus(busNumber)
{
    sensorInterface = objectServer.add_interface(
        "/xyz/openbmc_project/sensors/temperature/" + name,
        "xyz.openbmc_project.Sensor.Value");

    if (thresholds::hasWarningInterface(thresholds))
    {
        thresholdInterfaceWarning = objectServer.add_interface(
            "/xyz/openbmc_project/sensors/temperature/" + name,
            "xyz.openbmc_project.Sensor.Threshold.Warning");
    }
    if (thresholds::hasCriticalInterface(thresholds))
    {
        thresholdInterfaceCritical = objectServer.add_interface(
            "/xyz/openbmc_project/sensors/temperature/" + name,
            "xyz.openbmc_project.Sensor.Threshold.Critical");
    }
    association = objectServer.add_interface(
        "/xyz/openbmc_project/sensors/temperature/" + name,
        association::interface);

    setInitialProperties(conn);
}

NVMeSensor::~NVMeSensor()
{
    // close the input dev to cancel async operations
    objServer.remove_interface(thresholdInterfaceWarning);
    objServer.remove_interface(thresholdInterfaceCritical);
    objServer.remove_interface(sensorInterface);
    objServer.remove_interface(association);
}

void NVMeSensor::checkThresholds(void)
{
    thresholds::checkThresholds(this);
}
