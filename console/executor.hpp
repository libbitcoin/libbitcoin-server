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
#ifndef LIBBITCOIN_BS_EXECUTOR_HPP
#define LIBBITCOIN_BS_EXECUTOR_HPP

#include <atomic>
#include <future>
#include <optional>
#include <thread>
#include <unordered_map>

// Must pull in any base boost configuration before including boost.
#include <bitcoin/server.hpp>
#include <boost/format.hpp>

namespace libbitcoin {
namespace server {

// This class is just an ad-hoc user interface wrapper on the node.
class executor
{
private:
    // Singleton, private constructor.
    executor(parser& metadata, std::istream&, std::ostream& output,
        std::ostream& error);

public:
    DELETE_COPY_MOVE(executor);

    // Get the singleton instance.
    static executor& factory(parser& metadata, std::istream& input,
        std::ostream& output, std::ostream& error);

    // Called from main.
    bool dispatch();

    // Release shutdown monitor.
    ~executor();

private:
    // Executor.
    void handle_started(const system::code& ec);
    void handle_subscribed(const system::code& ec, size_t key);
    void handle_running(const system::code& ec);
    bool handle_stopped(const system::code& ec);

    // Store dumps.
    void dump_version() const;
    void dump_hardware() const;
    void dump_options() const;
    void dump_configuration() const;
    void dump_body_sizes() const;
    void dump_records() const;
    void dump_buckets() const;
    void dump_progress() const;
    void dump_collisions() const;

    // Store functions.
    bool check_store_path(bool create=false) const;
    bool create_store(bool details = false);
    bool open_store(bool details=false);
    code open_store_coded(bool details=false);
    bool close_store(bool details=false);
    bool reload_store(bool details=false);
    bool restore_store(bool details=false);
    bool hot_backup_store(bool details=false);
    bool cold_backup_store(bool details=false);

    // Long-running queries (scans).
    void scan_flags() const;
    void scan_slabs() const;
    void scan_buckets() const;
    void scan_collisions() const;

    // Command line (defaults to do_run).
    bool do_help();
    bool do_version();
    bool do_hardware();
    bool do_settings();
    bool do_new_store();
    bool do_backup();
    bool do_restore();
    bool do_flags();
    bool do_information();
    bool do_slabs();
    bool do_buckets();
    bool do_collisions();
    bool do_read(const system::hash_digest& hash);
    bool do_write(const system::hash_digest& hash);

    // Runtime options.
    void do_hot_backup();
    void do_close();
    void do_suspend();
    void do_resume();
    void do_report_work();
    void do_reload_store();
    void do_menu() const;
    void do_test() const;
    void do_info() const;
    void do_report_condition() const;
    void subscribe_capture();

    // Built in tests.
    void read_test(const system::hash_digest& hash) const;
    void write_test(const system::hash_digest& hash);

    // Logging.
    database::file::stream::out::rotator create_log_sink() const;
    system::ofstream create_event_sink() const;
    void subscribe_log(std::ostream& sink);
    void subscribe_events(std::ostream& sink);
    void logger(const boost::format& message) const;
    void logger(const std::string& message) const;
    void log_stopping();

    // Runner.
    void stopper(const std::string& message);
    void subscribe_connect();
    void subscribe_close();
    bool do_run();
    
    // Other user-facing values.
    static const std::string name_;
    static const std::string close_;

    // Runtime options.
    static const std::unordered_map<std::string, uint8_t> options_;
    static const std::unordered_map<uint8_t, std::string> options_menu_;

    // Runtime toggles.
    static const std::unordered_map<std::string, uint8_t> toggles_;
    static const std::unordered_map<uint8_t, std::string> toggles_menu_;
    static const std::unordered_map<uint8_t, bool> defined_;

    // Runtime events.
    static const std::unordered_map<uint8_t, std::string> fired_;

    parser& metadata_;
    server_node::ptr node_{};
    server_node::store store_;
    server_node::query query_;
    node::count_t sequence_{};

    std::istream& input_;
    std::ostream& output_;
    network::logger log_{};
    network::capture capture_{ input_, close_ };
    std_array<std::atomic_bool, add1(network::levels::verbose)> toggle_;

    // Shutdown.
    // ------------------------------------------------------------------------

    static constexpr int unsignalled{ -1 };
    static constexpr int signal_none{ -2 };

    static std::atomic<int> signal_;
    static std::promise<bool> stopped_;
    static std::promise<bool> stopping_;
    static std::optional<std::thread> poller_thread_;

    static void initialize_stop();
    static void uninitialize_stop();
    static void poll_for_stopping();
    static void wait_for_stopping();
    static void set_signal_handlers();
    static void create_hidden_window();
    static void destroy_hidden_window();
    static void stop(int signal=signal_none);
    static void handle_stop(int code);
    static bool canceled();

#if defined(HAVE_MSC)
    static HWND window_handle_;
    static std::promise<bool> window_ready_;
    static std::optional<std::thread> window_thread_;
    static BOOL WINAPI control_handler(DWORD signal);
    static LRESULT CALLBACK window_proc(HWND handle, UINT message,
        WPARAM wparam, LPARAM lparam);
#endif
};

} // namespace server
} // namespace libbitcoin

#endif
