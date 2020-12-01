#pragma once

#include <libmctp-smbus.h>
#include <libmctp.h>
#include <NVMeDevice.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <sensor.hpp>

class NVMeSensor : public Sensor
{
  public:
    NVMeSensor(sdbusplus::asio::object_server& objectServer,
               boost::asio::io_service& io,
               std::shared_ptr<sdbusplus::asio::connection>& conn,
               const std::string& sensorName,
               std::vector<thresholds::Threshold>&& _thresholds,
               const std::string& sensorConfiguration, const int busNumber);
    virtual ~NVMeSensor();

    NVMeSensor& operator=(const NVMeSensor& other) = delete;

    int bus;

  private:
    sdbusplus::asio::object_server& objServer;

    void checkThresholds(void) override;
};

struct NVMeMCTPContext //: std::enable_shared_from_this<NVMeMCTPContext>
{
    NVMeMCTPContext(boost::asio::io_service& io);
    boost::asio::deadline_timer scanTimer;

    boost::asio::deadline_timer mctpResponseTimer;
    boost::asio::ip::tcp::socket nvmeSlaveSocket;

    virtual ~NVMeMCTPContext();
};

struct NVMeContext : NVMeMCTPContext, NVMeSMBusContext, std::enable_shared_from_this<NVMeContext>//, NVMeMCTPContext
{
    NVMeContext(boost::asio::io_service& io, int rootBus);

    virtual ~NVMeContext();

    virtual void pollNVMeDevices();
    virtual void close();

    int rootBus; // Root bus for this drive

    std::list<std::shared_ptr<NVMeSensor>> sensors; // used as a poll queue

};

using NVMEMap = boost::container::flat_map<int, std::shared_ptr<NVMeContext>>;

int verifyIntegrity(uint8_t* msg, size_t len);

namespace nvmeMCTP
{
//this is add for missing when building. 
void init(void);
typedef uint8_t mctp_eid_t;
typedef void (*mctp_rx_fn)(uint8_t src_eid, void *data,void *msg, size_t len);
struct mctp;

#if HAVE_LIBMCTP_SMBUS
struct mctp *mctp_init(void);
int mctp_smbus_register_bus(struct mctp_binding_smbus *smbus, struct mctp *mctp, mctp_eid_t eid);
int mctp_set_rx_all(struct mctp *mctp, mctp_rx_fn fn, void *data);
struct mctp_binding_smbus *mctp_smbus_init(void);
int mctp_smbus_open_in_bus(struct mctp_binding_smbus *smbus, int in_bus);
int mctp_smbus_open_out_bus(struct mctp_binding_smbus *smbus, int out_bus);
int mctp_smbus_set_in_fd(struct mctp_binding_smbus *smbus, int fd);
int mctp_smbus_set_out_fd(struct mctp_binding_smbus *smbus, int fd);
int mctp_smbus_get_out_fd(struct mctp_binding_smbus *smbus);
int mctp_smbus_get_in_fd(struct mctp_binding_smbus *smbus);
int mctp_smbus_read(struct mctp_binding_smbus *smbus);
uint32_t crc32c(uint8_t *buf, int len);
int nvmeMessageTransmit(struct mctp& mctp, struct nvme_mi_msg_request& req);
#else
struct mctp *mctp_init(void)
{
return NULL;
}
int mctp_smbus_register_bus(struct mctp_binding_smbus *smbus, struct mctp *mctp, mctp_eid_t eid)
{
return -ENOTSUP;
}
int mctp_set_rx_all(struct mctp *mctp, mctp_rx_fn fn, void *data)
{
return -ENOTSUP;
}
struct mctp_binding_smbus *mctp_smbus_init(void)
{
return NULL;
}
int mctp_smbus_open_in_bus(struct mctp_binding_smbus *smbus, int in_bus)
{
return -ENOTSUP;
}
int mctp_smbus_open_out_bus(struct mctp_binding_smbus *smbus, int out_bus)
{
return -ENOTSUP;
}
int mctp_smbus_set_in_fd(struct mctp_binding_smbus *smbus, int fd)
{
return -ENOTSUP;
}
int mctp_smbus_set_out_fd(struct mctp_binding_smbus *smbus, int fd)
{
return -ENOTSUP;
}
int mctp_smbus_get_out_fd(struct mctp_binding_smbus *smbus)
{
return -ENOTSUP;
}
int mctp_smbus_get_in_fd(struct mctp_binding_smbus *smbus)
{
return -ENOTSUP;
}
int mctp_smbus_read(struct mctp_binding_smbus *smbus)
{
return -ENOTSUP;
}
uint32_t crc32c(uint8_t *buf, int len)
{
return -ENOTSUP;
}
static inline int nvmeMessageTransmit(struct mctp& mctp, struct nvme_mi_msg_request& req)
{
return -ENOTSUP;
}
#endif
} // namespace nvmeMCTP

NVMEMap& getNVMEMap(void);
