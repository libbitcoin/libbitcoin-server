/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include "executor.hpp"
#include "localize.hpp"

#include <unordered_map>

namespace libbitcoin {
namespace server {

using namespace network;

const std::unordered_map<uint8_t, bool> executor::defined_
{
    { levels::application, levels::application_defined },
    { levels::news,        levels::news_defined },
    { levels::session,     levels::session_defined },
    { levels::protocol,    levels::protocol_defined },
    { levels::proxy,       levels::proxy_defined },
    { levels::remote,      levels::remote_defined },
    { levels::fault,       levels::fault_defined },
    { levels::quitting,    levels::quitting_defined },
    { levels::objects,     levels::objects_defined },
    { levels::verbose,     levels::verbose_defined },
};

// Logging.
// ----------------------------------------------------------------------------

// TODO: verify construction failure handled.
database::file::stream::out::rotator executor::create_log_sink() const
{
    return 
    {
        // Standard file names, within the [node].path directory.
        metadata_.configured.log.log_file1(),
        metadata_.configured.log.log_file2(),
        to_half(metadata_.configured.log.maximum_size)
    };
}

void executor::subscribe_log(std::ostream& sink)
{
    using namespace system;

    log_.subscribe_messages
    (
        [&](const code& ec, uint8_t level, time_t time, const std::string& message)
        {
            if (level >= toggle_.size())
            {
                sink    << "Invalid log [" << serialize(level) << "] : " << message;
                output_ << "Invalid log [" << serialize(level) << "] : " << message;
                output_.flush();
                return true;
            }

            // Write only selected logs.
            if (!ec && !toggle_.at(level))
                return true;

            const auto prefix = format_zulu_time(time) + "." +
                serialize(level) + " ";

            if (ec)
            {
                sink << prefix << message << std::endl;
                output_ << prefix << message << std::endl;
                sink << prefix << BS_NODE_FOOTER << std::endl;
                output_ << prefix << BS_NODE_FOOTER << std::endl;
                output_ << prefix << BS_NODE_TERMINATE << std::endl;

                // Release subscription and signal termination of log.
                // Log destruct blocks on its own threadpool join, which occurs
                // only after all logging work is completed. After the above
                // message is queued, no more are accepted, and the executor
                // may initiate its own (and thereby this log's) destruction.
                logging_complete_.set_value(ec);
                return false;
            }
            else
            {
                sink << prefix << message;
                output_ << prefix << message;
                output_.flush();
                return true;
            }
        }
    );
}

void executor::logger(const std::string& message) const
{
    if (log_.stopped())
        output_ << message << std::endl;
    else
        log_.write(network::levels::application) << message << std::endl;
}

void executor::logger(const boost::format& message) const
{
    if (log_.stopped())
        output_ << message << std::endl;
    else
        log_.write(network::levels::application) << message << std::endl;
}

} // namespace server
} // namespace libbitcoin
