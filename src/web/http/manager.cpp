/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <stdio.h>
#ifdef WIN32
    #include <Winsock2.h>
#endif

#include "http.hpp"
#include "utilities.hpp"
#include "manager.hpp"

#ifdef WITH_MBEDTLS
extern "C"
{
// Random data generator used by mbedtls for SSL.
int https_random(void*, uint8_t* buffer, size_t length);
}
#endif

namespace libbitcoin {
namespace server {
namespace http {

manager::manager(bool ssl, event_handler handler,
    boost::filesystem::path document_root)
#ifdef WITH_MBEDTLS
  : ssl_(ssl),
#else
  : ssl_(false),
#endif
    listening_(false),
    user_data_(nullptr),
    running_(false),
    handler_(handler),
    document_root_(document_root),
    connections_{},
    maximum_incoming_frame_length_(1 << 19), // 512 KB
    high_water_mark_(1 << 21), // 2 MB
    backlog_(8)
{
#ifndef WITH_MBEDTLS
    BITCOIN_ASSERT_MSG(!ssl, "Secure HTTP requires MBEDTLS library.");
#endif
}

manager::~manager()
{
    running_ = false;
    listening_ = false;
    connections_.clear();

#ifdef WIN32
    WSACleanup();
#endif
}

bool manager::initialize()
{
#ifdef WIN32
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        return false;
#endif
    return true;
}

size_t manager::maximum_incoming_frame_length()
{
    return maximum_incoming_frame_length_;
}

void manager::set_maximum_incoming_frame_length(size_t length)
{
    maximum_incoming_frame_length_ = length;
    for (auto& connection: connections_)
        connection->set_maximum_incoming_frame_length(length);
}

size_t manager::high_water_mark()
{
    return high_water_mark_;
}

// May truncate any previously buffered write data on each connection.
void manager::set_high_water_mark(size_t length)
{
    high_water_mark_ = length;
    for (auto& connection: connections_)
        connection->set_high_water_mark(length);
}

void manager::set_backlog(int32_t backlog)
{
    backlog_ = backlog;
}

bool manager::bind(std::string hostname, uint16_t port,
    const bind_options& options)
{
    LOG_VERBOSE(LOG_SERVER_HTTP)
        << (ssl_ ? "Secure" : "Public") << " bind called with host "
        << hostname << " and port " << port;
    port_ = port;

    std::memset(&listener_address_, 0, sizeof(listener_address_));
    listener_address_.sin_family = AF_INET;
    listener_address_.sin_port = htons(port_);
    listener_address_.sin_addr.s_addr = htonl(INADDR_ANY);

    listening_ = true;
    user_data_ = options.user_data;

    listener_ = std::make_shared<connection>();
    listener_->socket() = ::socket(listener_address_.sin_family, SOCK_STREAM,
        0);

    if (listener_->socket() == 0)
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Socket failed with error " << last_error() << ": "
            << error_string();
        return false;
    }

#ifdef WITH_MBEDTLS
    if (ssl_)
    {
        if (!options.ssl_certificate.empty() ||
            !options.ssl_ca_certificate.empty())
        {
            key_ = options.ssl_key;
            certificate_ = options.ssl_certificate;
            ca_certificate_ = options.ssl_ca_certificate;
        }

        if (!initialize_ssl(listener_, listening_))
            return false;

        LOG_DEBUG(LOG_SERVER_HTTP)
            << "SSL initialized for listener socket";
    }
#endif

    listener_->set_socket_non_blocking();
    listener_->reuse_address();

    if (::bind(listener_->socket(), reinterpret_cast<sockaddr*>(
        &listener_address_), sizeof(listener_address_)) != 0)
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Bind failed with error " << last_error() << ": "
            << error_string();
        listener_->close();
        return false;
    }

    ::listen(listener_->socket(), backlog_);

    listener_->set_state(connection_state::listening);
    add_connection(listener_);
    return true;
}

bool manager::accept_connection()
{
    struct sockaddr_in remote_address;
    std::memset(&remote_address, 0, sizeof(remote_address));

    sock_t socket{};
    auto address_size = static_cast<socklen_t>(sizeof(remote_address));

    do
    {
        socket = ::accept(listener_->socket(), reinterpret_cast<sockaddr*>(
            &remote_address), &address_size);

        const auto error = last_error();
        if ((static_cast<connection_state>(socket) ==
             connection_state::error) && !would_block(error))
        {
            LOG_ERROR(LOG_SERVER_HTTP)
                << "Accept call failed with " << error_string();
            return false;
        }
        else
        {
            break;
        }

    } while (true);

#ifdef SO_NOSIGPIPE
    int no_sig_pipe = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE,
        reinterpret_cast<void*>(&no_sig_pipe), sizeof(no_sig_pipe)) != 0)
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Failed to disable SIGPIPE";
        CLOSE_SOCKET(socket);
        return false;
    }
#endif

    auto connection = std::make_shared<http::connection>(socket,
        remote_address);
    if (connection == nullptr)
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Failed to create new connection object";
        CLOSE_SOCKET(socket);
        return false;
    }

#ifdef WITH_MBEDTLS
    if (ssl_)
    {
        if (!initialize_ssl(connection, false))
        {
            LOG_ERROR(LOG_SERVER_HTTP)
                << "Failed to initialize new SSL connection on port " << port_;
            connection->close();
            return false;
        }

        auto& context = connection->ssl_context().context;
        mbedtls_ssl_set_bio(&context, reinterpret_cast<void*>(connection.get()),
            ssl_send, ssl_receive, nullptr);

        int error = ~0;
        while (error != 0)
        {
            error = mbedtls_ssl_handshake_step(&context);
            if (mbedtls_would_block(error))
                continue;

            if (error < 0)
            {
                LOG_ERROR(LOG_SERVER_HTTP)
                    << "SSL handshake failed: " << error_string()
                    << "-- dropping accepted connection " << connection;

                connection->close();
                return false;
            }
        }

        BITCOIN_ASSERT(connection->ssl_context().enabled);
    }
#endif

    // Set all per-connection variables, based on user-specified
    // or otherwise default settings.
    connection->set_state(connection_state::connected);
    connection->set_maximum_incoming_frame_length(
        maximum_incoming_frame_length_);
    connection->set_high_water_mark(high_water_mark_);
    connection->set_user_data(user_data_);
    connection->set_socket_non_blocking();

    add_connection(connection);

    LOG_VERBOSE(LOG_SERVER_HTTP)
        << "Accepted " << (ssl_ ? "SSL" : "Plaintext") << " connection: "
        << connection << " on port " << port_;
    return true;
}

void manager::add_connection(const connection_ptr& connection)
{
    connections_.push_back(connection);

    LOG_VERBOSE(LOG_SERVER_HTTP)
        << "Added Connection [" << connection << ", "
        << connections_.size() << " total]";
}

void manager::remove_connection(connection_ptr& connection)
{
    const auto it = std::find(connections_.begin(), connections_.end(),
        connection);

    if (it != connections_.end())
    {
        LOG_VERBOSE(LOG_SERVER_HTTP)
            << "Removing Connection [" << connection << ", "
            << connections_.size() - 1 << " remaining]";

        connections_.erase(it);
    }
    else
    {
        LOG_VERBOSE(LOG_SERVER_HTTP)
            << "Cannot locate connection for removal";
    }
}

size_t manager::connection_count()
{
    return connections_.size();
}

bool manager::ssl()
{
    return ssl_;
}

bool manager::listening()
{
    return listening_;
}

void manager::start()
{
    static const auto timeout_milliseconds = 10u;

    running_ = true;
    while (running_)
        run_once(timeout_milliseconds);
}

void manager::run_once(size_t timeout_milliseconds)
{
    if (stopped())
        return;

    // Run any tasks the user queued that must be run inside this thread.
    run_tasks();

    // Monitor and process sockets.
    poll(timeout_milliseconds);
}

void manager::stop()
{
    running_ = false;
}

bool manager::stopped()
{
    return !running_;
}

void manager::execute(std::shared_ptr<task> task)
{
    task_lock_.lock();
    tasks_.push_back(task);
    task_lock_.unlock();
}

void manager::run_tasks()
{
    task_list tasks;

    task_lock_.lock();
    tasks_.swap(tasks);
    task_lock_.unlock();

    for (auto& task: tasks)
        if (!task->run())
            handle_connection(task->connection(), event::error);
}

// Portable select based implementation.
void manager::poll(size_t timeout_milliseconds)
{
    static const size_t maximum_items = FD_SETSIZE;
    // Break up number of connections into N lists of a specified
    // maximum size and call select for each of them, given a
    // timeout of (timeout_milliseconds / N).
    //
    // This is a hack to work around limitations of the select
    // system call.  Note that you may also need to adjust the
    // descriptor limit for this process in order for this to work
    // properly.
    //
    // With very large connection counts and small specified
    // timeout values, this poll method may very well exceed the
    // specified timeout.
    if (connections_.size() > maximum_items)
    {
        const size_t number_of_lists =
            (connections_.size() / maximum_items) + 1u;
        const auto adjusted_timeout = static_cast<size_t>(
            std::ceil(timeout_milliseconds / number_of_lists));
        std::vector<connection_list> connection_lists;
        connection_lists.reserve(number_of_lists);
        for (size_t i = 0; i < number_of_lists; i++)
        {
            connection_list connection_list;
            connection_list.reserve(maximum_items);
            connection_lists.push_back(connection_list);
        }

        for (size_t i = 0, list_index = 0; i < connections_.size(); i++)
        {
            connection_lists[list_index].push_back(connections_[i]);
            if ((i > 0) && ((i - 1) % maximum_items) == 0)
                list_index++;
        }

        for (auto& connection_list: connection_lists)
            select(adjusted_timeout, connection_list);
    }
    else
    {
        select(timeout_milliseconds, connections_);
    }
}

void manager::select(size_t timeout_milliseconds, connection_list& connections)
{
    static const int maximum_items = FD_SETSIZE;
    connection_ptr static_sockets[maximum_items]{};
    connection_ptr* socket_list = static_sockets;

    // This limit must be honored by the caller.
    BITCOIN_ASSERT(connections.size() <= maximum_items);

    timeval poll_interval{};
    poll_interval.tv_sec = static_cast<long>(timeout_milliseconds / 1000);
    poll_interval.tv_usec = static_cast<long>((timeout_milliseconds * 1000) -
        (poll_interval.tv_sec * 100000));

    fd_set read_set;
    fd_set write_set;
    fd_set error_set;

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    FD_ZERO(&error_set);

    size_t last_index = 0;
    int max_descriptor = 0;

    connection_list pending_removal;
    for (auto& connection: connections)
    {
        auto descriptor = static_cast<int>(connection->socket());
        if (connection == nullptr || connection->closed())
            continue;

        // Check if the descriptor is too high to monitor in our
        // fd_set.
        if (descriptor > maximum_items)
        {
#ifdef WIN32
            CLOSE_SOCKET(descriptor);
            LOG_ERROR(LOG_SERVER_HTTP)
                << "Error: cannot monitor socket " << descriptor
                << ", value is above" << maximum_items;
            pending_removal.push_back(connection);
            continue;
#else
            // Attempt to resolve this by looking for a lower
            // available descriptor.
            auto new_descriptor = dup(descriptor);
            if (new_descriptor < descriptor && new_descriptor < maximum_items)
            {
                connection->socket() = new_descriptor;
                CLOSE_SOCKET(descriptor);
            }
            else
            {
                // Select cannot monitor this descriptor.
                CLOSE_SOCKET(new_descriptor);
                LOG_ERROR(LOG_SERVER_HTTP)
                    << "Error: cannot monitor socket " << descriptor
                    << ", value is above" << maximum_items;
                pending_removal.push_back(connection);
                continue;
            }
#endif
        }

        if (connection->file_transfer().in_progress ||
            !connection->write_buffer().empty())
            FD_SET(descriptor, &write_set);

        FD_SET(descriptor, &read_set);
        FD_SET(descriptor, &error_set);

        socket_list[last_index++] = connection;
        if (descriptor > max_descriptor)
            max_descriptor = descriptor;
    }

    for (auto& connection: pending_removal)
        handle_connection(connection, event::error);

    pending_removal.clear();

    const auto num_events = ::select(max_descriptor + 1, &read_set,
        &write_set, &error_set, &poll_interval);

    if (num_events == 0)
        return;

    if (num_events < 0)
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Error: select failed: " << error_string()
            << "max fd: " << max_descriptor;
        return;
    }

    for (size_t i = 0; i < last_index; i++)
    {
        auto& connection = socket_list[i];
        if (connection == nullptr || connection->closed())
            continue;

        const auto descriptor = connection->socket();
        if (FD_ISSET(descriptor, &error_set))
        {
            pending_removal.push_back(connection);
            continue;
        }

        if (FD_ISSET(descriptor, &write_set))
        {
            if (connection->file_transfer().in_progress && !transfer_file_data(
                connection))
            {
                pending_removal.push_back(connection);
                continue;
            }

            auto& write_buffer = connection->write_buffer();
            if (!write_buffer.empty())
            {
                const auto segment_length = std::min(write_buffer.size(), transfer_buffer_length);
                const auto written = connection->do_write(write_buffer.data(), segment_length, false);

                if (written < 0)
                {
                    pending_removal.push_back(connection);
                    continue;
                }

                write_buffer.erase(write_buffer.begin(),
                    write_buffer.begin() + written);
            }
        }

        if (FD_ISSET(descriptor, &read_set))
        {
            if (connection->state() == connection_state::listening)
            {
                if (!handle_connection(connection, event::listen))
                {
                    LOG_ERROR(LOG_SERVER_HTTP)
                        << "Terminating due to error on listening socket";
                    stop();
                    return;
                }

                // After accepting a connection, break out of loop since we're
                // iterating over a list that's just been updated.
                break;
            }
            else
            {
                const auto read = connection->read();
                if ((read == 0) || ((read < 0) && !would_block(last_error())))
                    pending_removal.push_back(connection);
                else if (!handle_connection(connection, event::read))
                    pending_removal.push_back(connection);
            }
        }
    }

    for (auto& connection: pending_removal)
        handle_connection(connection, event::error);
}

bool manager::handle_connection(connection_ptr& connection, event current_event)
{
    switch (current_event)
    {
        case event::listen:
        {
            if (!accept_connection())
            {
                // Don't let this accept failure stop the service.
                LOG_ERROR(LOG_SERVER_HTTP)
                    << "Failed to accept new connection";
                break;
            }

            // We could allow the user to know that a connection was
            // accepted here, but we instead opt to notify them only
            // after the connection is upgraded to a websocket.
            return true;
        }

        case event::read:
        {
            const auto read_result = connection->read_length();
            if (read_result <= 0)
                break;

            const auto read_length = static_cast<size_t>(read_result);
            auto& buffer = connection->read_buffer();
            if (connection->websocket())
            {
                auto& transfer = connection->websocket_transfer();
                if (transfer.in_progress)
                {
                    transfer.data.insert(transfer.data.end(), buffer.data(),
                        buffer.data() + read_length);

                    transfer.offset += read_length;
                    BITCOIN_ASSERT(transfer.offset == transfer.data.size());

                    // Check for configuration violation (helps
                    // prevent DoS by filling RAM with unexpectedly
                    // large messages).
                    if (transfer.offset > maximum_incoming_frame_length_)
                    {
                        LOG_ERROR(LOG_SERVER_HTTP)
                            << "Terminating due to exceeding the "
                            "maximum_incoming_frame_length";
                        return false;
                    }

                    if (transfer.offset != transfer.length)
                        return true;
                }

                return handle_websocket(connection);
            }

            const std::string request{ buffer.begin(), buffer.begin() + read_length };

            http_request out{};
            if (parse_http(out, request))
            {
                // Check if we need to convert HTTP connection to websocket.
                if (out.upgrade_request)
                    return upgrade_connection(connection, out);

                // Check if we need to mark HTTP connection as
                // expecting a JSON-RPC reply.  If so, we need to call
                // the user handler to notify user that a new json_rpc
                // connection was accepted so that they can track it.
                connection->set_json_rpc(out.json_rpc);
                if (out.json_rpc)
                {
                    return handler_(connection, event::accepted, nullptr) &&
                        handler_(connection, event::json_rpc, reinterpret_cast<
                            void*>(&out));
                }

                // Call user's event handler with the parsed http
                // request.
                return (handler_(connection, event::read, reinterpret_cast<
                    void*>(&out)) ? send_response(connection, out) : false);
            }
            else
            {
                LOG_VERBOSE(LOG_SERVER_HTTP)
                    << "Failed to parse HTTP request from " << request;
                return handle_connection(connection, event::error);
            }

            break;
        }

        case event::write:
        {
            // Should never get here since writes are handled
            // elsewhere.
            BITCOIN_ASSERT(false);
            return false;
        }

        case event::error:
        case event::closing:
        {
            if ((connection == nullptr) || connection->closed())
                return false;

            LOG_VERBOSE(LOG_SERVER_HTTP)
                << "Connection closing: " << connection;
            handler_(connection, event::closing, nullptr);
            remove_connection(connection);
            connection->close();
            return false;
        }

        default:
            return false;
    }

    return true;
}

#ifdef WITH_MBEDTLS
// static
int manager::ssl_send(void* data, const uint8_t* buffer, size_t length)
{
    auto connection = reinterpret_cast<http::connection*>(data);
#ifdef MSG_NOSIGNAL
    int flags = MSG_NOSIGNAL;
#else
    int flags = 0;
#endif
    const auto sent = static_cast<int>(send(connection->socket(), buffer,
        length, flags));
    if (sent >= 0)
        return sent;

    return ((would_block(sent) || sent == EINPROGRESS) ?
        MBEDTLS_ERR_SSL_WANT_WRITE : -1);
}

// static
int manager::ssl_receive(void* data, uint8_t* buffer, size_t length)
{
    auto connection = reinterpret_cast<http::connection*>(data);
    auto read = static_cast<int>(recv(connection->socket(), buffer, length, 0));
    if (read >= 0)
        return read;

    return ((would_block(read) || read == EINPROGRESS) ?
        MBEDTLS_ERR_SSL_WANT_READ : -1);
}
#endif

bool manager::transfer_file_data(connection_ptr& connection)
{
    std::array<uint8_t, transfer_buffer_length> buffer;
    auto& file_transfer = connection->file_transfer();

    if (!file_transfer.in_progress)
        return false;

    auto amount_to_read = std::min(transfer_buffer_length,
        file_transfer.length - file_transfer.offset);

    const auto read = fread(buffer.data(), sizeof(uint8_t), amount_to_read,
        file_transfer.descriptor);

    auto success = (read == amount_to_read ||
        (read < amount_to_read && feof(file_transfer.descriptor)));

    if (!success)
    {
        if (file_transfer.in_progress)
        {
            fclose(file_transfer.descriptor);
            file_transfer.in_progress = false;
            file_transfer.offset = 0;
            file_transfer.length = 0;
        }

        return false;
    }

    const auto written = connection->write(buffer.data(), read);
    success = (written >= 0 && static_cast<size_t>(written) == read);

    if (!success)
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Write failed: requested " << read << " and wrote " << written;
        return false;
    }

    file_transfer.offset += read;

    if (file_transfer.offset == file_transfer.length)
    {
        if (file_transfer.in_progress)
        {
            fclose(file_transfer.descriptor);
            file_transfer.in_progress = false;
            file_transfer.offset = 0;
            file_transfer.length = 0;
        }
    }

    return success;
}

bool manager::send_http_file(connection_ptr& connection,
    const std::string& path, bool keep_alive)
{
    auto& file_transfer = connection->file_transfer();

    if (!file_transfer.in_progress)
    {
        file_transfer.descriptor = fopen(path.c_str(), "r");
        if (file_transfer.descriptor == nullptr)
            return false;

        file_transfer.in_progress = true;
        file_transfer.offset = 0;
        file_transfer.length = boost::filesystem::file_size(path);

        http_reply reply;
        const auto response = reply.generate(protocol_status::ok,
            mime_type(path), file_transfer.length, keep_alive);

        if (!connection->write(response))
            return false;
    }

    // On future iterations, this is called directly from poll while
    // the file transfer is in progress.
    return transfer_file_data(connection);
}


bool manager::handle_websocket(connection_ptr& connection)
{
    const auto read_result = connection->read_length();

    if (read_result <= 0)
        return false;

    size_t length = 0;
    size_t mask_length = 0;
    size_t data_length = 0;
    size_t header_length = 0;

    auto& buffer = connection->read_buffer();
    auto& transfer = connection->websocket_transfer();
    const auto data = transfer.in_progress ? transfer.data.data() :
        buffer.data();

    const auto flags = data[0];
    const auto final = (flags & 0x80) != 0;
    const auto op_code = static_cast<websocket_op>(flags & 0x0f);
    const auto fragment = !final|| op_code == websocket_op::continuation;

    if (fragment)
    {
        // RFC6455 Fragments are not currently supported.
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Websocket fragments are not supported";
        return false;
    }

    const auto read_length = static_cast<size_t>(read_result);

    if (read_length < 2)
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Invalid websocket frame";
        return false;
    }

    length = (data[1] & 0x7f);
    mask_length = (data[1] & 0x80 ? 4u : 0u);

    if (mask_length == 0)
    {
        // RFC6455: "The server MUST close the connection upon receiving a
        // frame that is not masked."
        LOG_ERROR(LOG_SERVER_HTTP)
            << "No mask included from client";
        return false;
    }

    if (length < 126 && read_length >= mask_length)
    {
        data_length = length;
        header_length = 2u + mask_length;
    }
    else if (length == 126 && read_length >= 2u + sizeof(uint16_t) + mask_length)
    {
        // BUGBUG: endianness, frame requires a deserializer.
        ////data_length = ntohs(*reinterpret_cast<const uint16_t*>(&data[2]));
        header_length = 2u + sizeof(uint16_t) + mask_length;
    }
    else if (read_length >= 2u + sizeof(uint64_t) + mask_length)
    {
        // BUGBUG: endianness, frame requires a deserializer.
        ////data_length = boost::endian::endian_reverse(
        ////    *reinterpret_cast<uint64_t*>(&data[2]));
        header_length = 2u + sizeof(uint64_t) + mask_length;
    }

    const auto reassemble = transfer.in_progress && transfer.offset > 0 &&
        fragment;

    LOG_VERBOSE(LOG_SERVER_HTTP)
        << "websocket data_frame flags: " << static_cast<uint32_t>(flags)
        << ", is_fragment: " << (fragment ? "true" : "false")
        << ", reassemble: " << (reassemble ? "true" : "false")
        << ", final_fragment: " << (final ? "true" : "false");
    LOG_VERBOSE(LOG_SERVER_HTTP)
        << "Incoming websocket data frame length: " << data_length
        << ", read length: " << read_length;

    // If the entire frame payload isn't buffered, initiate state to track the
    // transfer of this frame.
    if (data_length > read_length && !transfer.in_progress)
    {
        // Check if this transfer exceeds the maximum incoming frame length.
        if (data_length > maximum_incoming_frame_length_)
        {
            LOG_ERROR(LOG_SERVER_HTTP)
                << "Terminating connection due to exceeding the "
                << "maximum_incoming_frame_length";
            return false;
        }

        transfer.in_progress = true;
        transfer.header_length = header_length;
        transfer.length = data_length + header_length;

        data_chunk data;
        transfer.data.swap(data);
        transfer.data.reserve(transfer.length);
        transfer.data.insert(transfer.data.end(), buffer.data(),
            buffer.data() + header_length);

        const auto non_mask_length = transfer.header_length - mask_length;
        const auto mask_start = data.data() + non_mask_length;
        transfer.mask = { mask_start, mask_start + mask_length };
        transfer.offset = transfer.data.size();

        LOG_DEBUG(LOG_SERVER_HTTP)
            << "Initiated frame transfer for a length of " << data_length;
        return true;
    }
    else
    {
        LOG_VERBOSE(LOG_SERVER_HTTP)
            << "Data length: " << data_length << ", Header length: "
            << header_length << ", Mask length: " << mask_length;
    }

    const auto frame_length = header_length + data_length;
    if (frame_length < header_length || frame_length < data_length)
        return false;

    const auto event_type = flags & 0x8 ?
        event::websocket_control_frame : event::websocket_frame;

    if (event_type == event::websocket_control_frame)
    {
        // XOR mask the payload using the client provided mask.
        const auto mask_start = data + header_length - mask_length;

        for (size_t index = 0; index < data_length; ++index)
            data[index + header_length] ^= mask_start[index % 4];

        websocket_message message
        {
            connection->websocket_endpoint(),
            data + header_length,
            data_length,
            flags,
            static_cast<websocket_op>(flags & 0x0f)
        };

        // Possible TODO: If the opcode is a ping, send response here.
        if (message.code != http::websocket_op::close)
            LOG_DEBUG(LOG_SERVER_HTTP)
                << "Unhandled websocket op: " << to_string(message.code);

        // Call user handler for control frames.
        const auto status = handler_(connection, event_type, &message);

        // Returning false here causes the connection to be removed.
        if (message.code == http::websocket_op::close)
            return false;

        return status;
    }

    if (final && transfer.in_progress && transfer.offset == transfer.length)
    {
        const auto mask_start = transfer.mask.data();
        const auto payload_length = transfer.length - transfer.header_length;

        for (size_t index = 0; index < payload_length; ++index)
            data[index + transfer.header_length] ^= mask_start[index % 4];

        websocket_message message
        {
            connection->websocket_endpoint(),
            data + transfer.header_length,
            transfer.data.size() - transfer.header_length,
            flags,
            static_cast<websocket_op>(flags & 0x0f)
        };

        // Call user handler on last fragment with the entire message.
        const auto status = handler_(connection, event_type, &message);

        transfer.in_progress = false;
        transfer.offset = 0;
        transfer.length = 0;
        transfer.header_length = 0;
        transfer.mask.clear();
        transfer.data.clear();

        return status;
    }
    else if (!reassemble && (flags & 0x0f) ==
             static_cast<uint8_t>(websocket_op::close))
    {
        LOG_DEBUG(LOG_SERVER_HTTP)
            << "Closing websocket due to close op.";
        return false;
    }
    else
    {
         // Check if we need to read again.
        if (data_length > read_length)
            return true;

        // Apply websocket mask (if required) before user handling.
        if ((mask_length > 0) && (read_length > data_length))
        {
            const auto mask_start = data + header_length - mask_length;
            for(size_t index = 0; index < data_length; ++index)
                data[index + header_length] ^= mask_start[index % 4];
        }

        websocket_message message
        {
            connection->websocket_endpoint(),
            data + header_length,
            data_length,
            flags,
            op_code
        };

        // Call user handler for non-fragmented frames.
        return handler_(connection, event_type, &message);
    }

    return false;
}

bool manager::send_response(connection_ptr& connection,
    const http_request& request)
{
    auto path = document_root_;

    if (request.uri == "/")
    {
        static const std::vector<boost::filesystem::path> index_files
        {
            { "index.html" },
            { "index.htm" },
            { "index.shtml" }
        };

        for (const auto& index: index_files)
        {
            const auto test_path = path / index;
            if (boost::filesystem::exists(test_path))
            {
                path = test_path;
                break;
            }
        }

        if (path == document_root_)
            return false;
    }
    else
    {
        // BUGBUG: sanitize path to guard against information leak.
        path /= request.uri;
    }

    if (!boost::filesystem::exists(path))
    {
        LOG_VERBOSE(LOG_SERVER_HTTP)
            << "Requested Path: " << path << " does not exist";

        // TODO: move time formatter to independent utility.
        static const size_t max_date_time_length = 32;
        std::array<char, max_date_time_length> time_buffer;
        const auto current_time = std::time(nullptr);

        // BUGBUG: std::gmtime may not be thread safe.
        std::strftime(time_buffer.data(), time_buffer.size(),
            "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&current_time));

        const auto response = std::string(
            "HTTP/1.1 404 Not Found\r\n"
            "Date: ") + time_buffer.data() + std::string("\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 90\r\n\r\n"
            "<html><head><title>Page not found</title></head>"
            "<body>The page was not found.</body></html>\r\n\r\n");

        connection->write(response);
        return true;
    }

    const auto keep_alive =
        ((request.protocol.find("HTTP/1.0") == std::string::npos) ||
        (request.header(std::string("Connection")) == "keep-alive"));

    return send_http_file(connection, path.generic_string(), keep_alive);
}

bool manager::send_generated_reply(connection_ptr& connection,
    protocol_status status)
{
    http_reply reply;
    return connection->write(reply.generate(status, {}, 0, false));
}

bool manager::upgrade_connection(connection_ptr& connection,
    const http_request& request)
{
    // Request MUST be GET and Protocol must be at least 1.1
    if ((request.method != "get") || (request.protocol_version >= 1.1f))
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Rejecting upgrade request for method " << request.method
            << "/Protocol " << request.protocol;
        send_generated_reply(connection, protocol_status::bad_request);
        return false;
    }

    // Verify if origin is acceptable (contains either
    // localhost, hostname, or ip address of current server)
    if (!validate_origin(request.header("origin")))
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Rejecting upgrade request for origin: "
            << request.header("origin");
        send_generated_reply(connection, protocol_status::forbidden);
        return false;
    }

    const auto version = request.header("sec-websocket-version");
    if (!version.empty() && version != "13")
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Rejecting upgrade request for version: " << version;
        send_generated_reply(connection, protocol_status::bad_request);
        return false;
    }

    const auto key = request.header("sec-websocket-key");
    if (key.empty())
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Rejecting upgrade request due to missing sec-websocket-key";
        send_generated_reply(connection, protocol_status::bad_request);
        return false;
    }

    http_reply reply;
    const auto key_response = websocket_key_response(key);
    const auto protocol = request.header("sec-websocket-protocol");
    const auto response = reply.generate_upgrade(key_response, protocol);

    // This write is unbuffered since we're not a websocket yet and
    // want to be sure it completes before changing our state to be
    // upgraded to a websocket.
    if (connection->do_write(reinterpret_cast<const uint8_t*>(
        response.c_str()), response.size(), false) != static_cast<int32_t>(
            response.size()))
    {
        LOG_ERROR(LOG_SERVER_HTTP)
            << "Failed to upgrade connection due to a write failure";
        return false;
    }

    connection->set_websocket(true);
    connection->set_websocket_endpoint(request.uri);

    LOG_VERBOSE(LOG_SERVER_HTTP)
        << "Upgraded connection " << connection << " for uri " << request.uri;

    // On upgrade, call the user handler so they can track this websocket.
    return handler_(connection, event::accepted, nullptr);
}

bool manager::validate_origin(const std::string origin)
{
    if (origin.empty())
        return false;

    if ((origin.find("127.0.0.1") != std::string::npos) ||
       (origin.find("localhost") != std::string::npos))
        return true;

    static const auto max_hostname_length = 253;
    std::array<char, max_hostname_length> hostname;
    if (gethostname(hostname.data(), max_hostname_length) != 0)
        return false;

    return (origin.find(std::string{ hostname.data() }) != std::string::npos);
}

bool manager::initialize_ssl(connection_ptr& connection, bool listener)
{
#ifdef WITH_MBEDTLS
    auto& context = connection->ssl_context();
    auto& configuration = context.configuration;

    mbedtls_ssl_init(&context.context);
    if (listener)
    {
        mbedtls_ssl_config_init(&configuration);
        if (mbedtls_ssl_config_defaults(&configuration, MBEDTLS_SSL_IS_SERVER,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0)
            return false;

        // TLS 1.2 and up
        mbedtls_ssl_conf_min_version(&configuration,
            MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
        mbedtls_ssl_conf_rng(&configuration, https_random, nullptr);
    }

    if (!certificate_.empty())
    {
        if (key_.empty())
            key_ = certificate_;

        LOG_VERBOSE(LOG_SERVER_HTTP)
            << "Using cert " << certificate_ << " and key " << key_;

        mbedtls_pk_init(&context.key);
        mbedtls_x509_crt_init(&context.certificate);

        if (mbedtls_x509_crt_parse_file(&context.certificate,
            certificate_.c_str()) != 0)
        {
            LOG_ERROR(LOG_SERVER_HTTP)
                << "Failed to parse certificate: " << certificate_;
            return false;
        }

        if (mbedtls_pk_parse_keyfile(&context.key, key_.c_str(), nullptr) != 0)
        {
            LOG_ERROR(LOG_SERVER_HTTP)
                << "Failed to parse key: " << key_;
            return false;
        }

        if (mbedtls_ssl_conf_own_cert(&configuration, &context.certificate,
            &context.key) != 0)
        {
            LOG_ERROR(LOG_SERVER_HTTP)
                << "Failed to set own certificate chain and private key";
            return false;
        }
    }

    if (!ca_certificate_.empty() && ca_certificate_ != "*")
    {
        mbedtls_x509_crt_init(&context.ca_certificate);
        if (mbedtls_x509_crt_parse_file(&context.ca_certificate,
            ca_certificate_.c_str()) != 0)
        {
            LOG_ERROR(LOG_SERVER_HTTP)
                << "Failed to parse CA certificate: " << ca_certificate_;
            return false;
        }

        mbedtls_ssl_conf_ca_chain(&configuration, &context.ca_certificate,
            nullptr);
        mbedtls_ssl_conf_authmode(&configuration, MBEDTLS_SSL_VERIFY_REQUIRED);
    }

    if (!listener)
    {
        if (mbedtls_ssl_setup(&context.context,
            &listener_->ssl_context().configuration) != 0)
        {
            LOG_ERROR(LOG_SERVER_HTTP)
                << "SSL setup failed for " << connection;
            return false;
        }

        if (!context.hostname.empty() && mbedtls_ssl_set_hostname(
            &context.context, context.hostname.c_str()) != 0)
        {
            LOG_ERROR(LOG_SERVER_HTTP)
                << "SSL set hostname failed for " << connection;
            return false;
        }

        mbedtls_ssl_session_reset(&context.context);
    }

    mbedtls_ssl_conf_ciphersuites(&configuration, default_ciphers);

    context.enabled = true;
    return context.enabled;
#else
    return false;
#endif
}

} // namespace http
} // namespace server
} // namespace libbitcoin
