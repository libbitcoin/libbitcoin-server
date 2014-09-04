#ifndef OBELISK_CLIENT_MESSAGE
#define OBELISK_CLIENT_MESSAGE

#include <czmq++/czmq.hpp>
#include <bitcoin/bitcoin.hpp>
#include <obelisk/define.hpp>

namespace obelisk {

class incoming_message
{
public:
    BCS_API bool recv(czmqpp::socket& socket);
    BCS_API const bc::data_chunk origin() const;
    BCS_API const std::string& command() const;
    BCS_API uint32_t id() const;
    BCS_API const bc::data_chunk& data() const;

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
    BCS_API outgoing_message(
        const bc::data_chunk& dest, const std::string& command,
        const bc::data_chunk& data);
    BCS_API outgoing_message(
        const incoming_message& request, const bc::data_chunk& data);
    // Default constructor provided for containers and copying.
    BCS_API outgoing_message();

    BCS_API void send(czmqpp::socket& socket) const;
    BCS_API uint32_t id() const;

private:
    bc::data_chunk dest_;
    std::string command_;
    uint32_t id_;
    bc::data_chunk data_;
};

} // namespace obelisk

#endif

