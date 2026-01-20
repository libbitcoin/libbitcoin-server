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
using namespace node;
using boost::format;

// local
enum menu : uint8_t
{
    backup,
    close,
    errors,
    go,
    hold,
    info,
    menu_,
    test,
    work,
    zeroize
};

// for capture construct, defines [c]lose command key.
const std::string executor::close_{ "c" };

const std::unordered_map<std::string, uint8_t> executor::options_
{
    { "b", menu::backup },
    { "c", menu::close },
    { "e", menu::errors },
    { "g", menu::go },
    { "h", menu::hold },
    { "i", menu::info },
    { "m", menu::menu_ },
    { "t", menu::test },
    { "w", menu::work },
    { "z", menu::zeroize }
};

const std::unordered_map<uint8_t, std::string> executor::options_menu_
{
    { menu::backup,  "[b]ackup the store" },
    { menu::close,   "[c]lose the node" },
    { menu::errors,  "[e]rrors in store" },
    { menu::go,      "[g]o network communication" },
    { menu::hold,    "[h]old network communication" },
    { menu::info,    "[i]nfo about store" },
    { menu::menu_,   "[m]enu of options and toggles" },
    { menu::test,    "[t]est built-in case" },
    { menu::work,    "[w]ork distribution" },
    { menu::zeroize, "[z]eroize disk full error" }
};

const std::unordered_map<std::string, uint8_t> executor::toggles_
{
    { "a", levels::application },
    { "n", levels::news },
    { "s", levels::session },
    { "p", levels::protocol },
    { "x", levels::proxy },
    { "r", levels::remote },
    { "f", levels::fault },
    { "q", levels::quitting },
    { "o", levels::objects },
    { "v", levels::verbose }
};

const std::unordered_map<uint8_t, std::string> executor::toggles_menu_
{
    { levels::application, "[a]pplication" },
    { levels::news,        "[n]ews" },
    { levels::session,     "[s]ession" },
    { levels::protocol,    "[p]rotocol" },
    { levels::proxy,       "[x]proxy" },
    { levels::remote,      "[r]emote" },
    { levels::fault,       "[f]ault" },
    { levels::quitting,    "[q]uitting" },
    { levels::objects,     "[o]bjects" },
    { levels::verbose,     "[v]erbose" }
};

// Runtime options.
// ----------------------------------------------------------------------------

// [b]ackup
void executor::do_hot_backup()
{
    if (!node_)
    {
        logger(BS_NODE_UNAVAILABLE);
        return;
    }

    hot_backup_store(true);
}

// [c]lose
void executor::do_close()
{
    logger("CONSOLE: Close");
    stop(error::success);
}

// [e]rrors
void executor::do_report_condition() const
{
    store_.report([&](const auto& ec, auto table)
    {
        logger(format(BS_CONDITION) % server_node::store::tables.at(table) %
            ec.message());
    });

    if (query_.is_full())
        logger(format(BS_RELOAD_SPACE) % query_.get_space());
}

// [h]old
void executor::do_suspend()
{
    if (!node_)
    {
        logger(BS_NODE_UNAVAILABLE);
        return;
    }

    node_->suspend(node::error::suspended_service);
}

// [g]o
void executor::do_resume()
{
    if (query_.is_full())
    {
        logger(BS_NODE_DISK_FULL);
        return;
    }

    if (query_.is_fault())
    {
        logger(BS_NODE_UNRECOVERABLE);
        return;
    }

    if (!node_)
    {
        logger(BS_NODE_UNAVAILABLE);
        return;
    }

    node_->resume();
}

// [i]nfo
void executor::do_info() const
{
    dump_body_sizes();
    dump_records();
    dump_buckets();
    dump_collisions();
    ////dump_progress();
}

// [m]enu
void executor::do_menu() const
{
    for (const auto& toggle: toggles_menu_)
        logger(format("Toggle: %1%") % toggle.second);

    for (const auto& option: options_menu_)
        logger(format("Option: %1%") % option.second);
}

// [t]est
void executor::do_test() const
{
    read_test(system::null_hash);
}

// [w]ork
void executor::do_report_work()
{
    if (!node_)
    {
        logger(BS_NODE_UNAVAILABLE);
        return;
    }

    logger(format(BS_NODE_REPORT_WORK) % sequence_);
    node_->notify(error::success, chase::report, sequence_++);
}

// [z]eroize
void executor::do_reload_store()
{
    // Use do_resume command to restart connections after resetting here.
    if (query_.is_full())
    {
        if (!node_)
        {
            logger(BS_NODE_UNAVAILABLE);
            return;
        }

        reload_store(true);
        return;
    }

    // Any table with any error code.
    logger(query_.is_fault() ? BS_NODE_UNRECOVERABLE : BS_NODE_OK);
}

// Runtime options/toggles dispatch.
// ----------------------------------------------------------------------------

void executor::subscribe_capture()
{
    using namespace system;

    // This is not on a network thread, so the node may call close() while this
    // is running a backup (for example), resulting in a try_lock warning loop.
    capture_.subscribe(
        [&](const code& ec, const std::string& line)
        {
            // The only case in which false may be returned.
            if (ec == network::error::service_stopped)
            {
                set_console_echo();
                return false;
            }

            const auto token = trim_copy(line);

            // <control>-c emits empty token on Win32.
            if (token.empty())
                return true;

            // toggle log levels
            if (toggles_.contains(token))
            {
                const auto toggle = toggles_.at(token);
                if (defined_.at(toggle))
                {
                    toggle_.at(toggle) = !toggle_.at(toggle);
                    logger(format("CONSOLE: toggle %1% logging (%2%).") %
                        toggles_menu_.at(toggle) % 
                        (toggle_.at(toggle) ? "+" : "-"));
                }
                else
                {
                    logger(format("CONSOLE: %1% logging is not compiled.") %
                        toggles_menu_.at(toggle));
                }

                return true;
            }

            // dispatch options
            if (options_.contains(token))
            {
                switch (options_.at(token))
                {
                    case menu::backup:
                    {
                        do_hot_backup();
                        return true;
                    }
                    case menu::close:
                    {
                        do_close();
                        return true;
                    }
                    case menu::errors:
                    {
                        do_report_condition();
                        return true;
                    }
                    case menu::go:
                    {
                        do_resume();
                        return true;
                    }
                    case menu::hold:
                    {
                        do_suspend();
                        return true;
                    }
                    case menu::info:
                    {
                        do_info();
                        return true;
                    }
                    case menu::menu_:
                    {
                        do_menu();
                        return true;
                    }
                    case menu::test:
                    {
                        do_test();
                        return true;
                    }
                    case menu::work:
                    {
                        do_report_work();
                        return true;
                    }
                    case menu::zeroize:
                    {
                        do_reload_store();
                        return true;
                    }
                    default:
                    {
                        logger("CONSOLE: Unexpected option.");
                        return true;
                    }
                }
            }

            logger("CONSOLE: '" + line + "'");
            return true;
        },
        [&](const code& ec)
        {
            // subscription completion handler.
            if (!ec)
                unset_console_echo();
        }
    );
}

} // namespace server
} // namespace libbitcoin
