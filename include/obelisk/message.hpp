#ifndef OBELISK_CLIENT_MESSAGE
#define OBELISK_CLIENT_MESSAGE

#include <czmq++/czmq.hpp>
#include <bitcoin/types.hpp>

namespace obelisk {

namespace bc = libbitcoin;

class incoming_message
{
public:
    bool recv(czmqpp::socket& socket);
    const bc::data_chunk origin() const;
    const std::string& command() const;
    const uint32_t id() const;
    const bc::data_chunk& data() const;

private:
    bc::data_chunk origin_;
    std::string command_;
    uint32_t id_;
    bc::data_chunk data_;
};

class outgoing_message
{
public:
    // Empty dest = unspecified destination.
    outgoing_message(
        const bc::data_chunk& dest, const std::string& command,
        const bc::data_chunk& data);
    outgoing_message(
        const incoming_message& request, const bc::data_chunk& data);
    // Default constructor provided for containers and copying.
    outgoing_message();

    void send(czmqpp::socket& socket) const;
    const uint32_t id() const;

private:
    bc::data_chunk dest_;
    std::string command_;
    uint32_t id_;
    bc::data_chunk data_;
};

} // namespace obelisk

#endif

