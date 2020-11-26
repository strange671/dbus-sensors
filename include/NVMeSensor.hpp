#pragma once

#if HAVE_LIBMCTP_SMBUS
#include <libmctp-smbus.h>
#endif
#include <libmctp.h>
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

struct NVMeSMBusContext //: std::enable_shared_from_this<NVMeSMBusContext>
{
    int busfd;

    // link NVMeContext

    virtual ~NVMeSMBusContext();
};

struct NVMeContext : NVMeMCTPContext, NVMeSMBusContext, std::enable_shared_from_this<NVMeContext>//, NVMeMCTPContext
{
    NVMeContext(boost::asio::io_service& io, int rootBus);

    virtual ~NVMeContext();

    virtual void pollNVMeDevices();
    virtual void close();

    int rootBus; // Root bus for this drive

//    boost::asio::deadline_timer scanTimer;

//    boost::asio::deadline_timer mctpResponseTimer;
//    boost::asio::ip::tcp::socket nvmeSlaveSocket;

//    std::list<std::shared_ptr<NVMeMCTPContext>> NVMeMCTPContext;
    std::list<std::shared_ptr<NVMeSensor>> sensors; // used as a poll queue

};

using NVMEMap = boost::container::flat_map<int, std::shared_ptr<NVMeContext>>;

int verifyIntegrity(uint8_t* msg, size_t len);

namespace nvmeMCTP
{
void init(void);
#if HAVE_LIBMCTP_SMBUS
//typedef void (*mctp_rx_fn)(uint8_t src_eid, void *data, void *msg, size_t len, bool tag_owner, uint8_t tag, void *msg_binding_private);

int mctp_smbus_register_bus(struct mctp_binding_smbus *smbus, struct mctp *mctp, mctp_eid_t eid);
int mctp_set_rx_all(struct mctp *mctp, mctp_rx_fn fn, void *data);
struct mctp_binding_smbus *mctp_smbus_init(void);
#else
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
return 0;
}
#endif
}

namespace nvmeSMBus
{
void init(void);
}

NVMEMap& getNVMEMap(void);
