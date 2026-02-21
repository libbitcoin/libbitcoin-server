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
#ifndef LIBBITCOIN_SERVER_ESTIMATOR_HPP
#define LIBBITCOIN_SERVER_ESTIMATOR_HPP

#include <atomic>
#include <bitcoin/server.hpp>

namespace libbitcoin {
namespace server {

class BCS_API estimator
{
public:
    DELETE_COPY_MOVE_DESTRUCT(estimator);

    /// Estimation modes.
    enum class mode
    {
        basic,
        geometric,
        economical,
        conservative
    };

    estimator() NOEXCEPT {};

    /// Fee estimation in satoshis / transaction virtual size.
    /// Pass zero to target next block for confirmation, range:0..1007.
    uint64_t estimate(size_t target, mode mode) const NOEXCEPT;

    /// Populate accumulator with count blocks up to the top confirmed block.
    bool initialize(std::atomic_bool& cancel, const node::query& query,
        size_t count) NOEXCEPT;

    /// Update accumulator.
    bool push(const node::query& query) NOEXCEPT;
    bool pop(const node::query& query) NOEXCEPT;

    /// Top height of accumulator.
    size_t top_height() const NOEXCEPT;

protected:
    using rates = database::fee_rates;
    using rate_sets = database::fee_rate_sets;

    /// Bucket depth sizing parameters.
    enum horizon : size_t
    {
        small  = 12,
        medium = 48,
        large  = 1008
    };

    /// Bucket count sizing parameters.
    struct sizing
    {
        static constexpr double min  = 0.1;
        static constexpr double max  = 100'000.0;
        static constexpr double step = 1.05;

        /// Derived from min/max/step above.
        static constexpr size_t count = 283;
    };

    /// Estimation confidences.
    struct confidence
    {
        static constexpr double low = 0.60;
        static constexpr double mid = 0.85;
        static constexpr double high = 0.95;
    };

    /// Accumulator (persistent, decay-weighted counters).
    struct accumulator
    {
        template <size_t Horizon>
        struct bucket
        {
            /// Total scaled txs in bucket.
            std::atomic<size_t> total{};

            /// confirmed[n]: scaled txs confirmed in > n blocks.
            std::array<std::atomic<size_t>, Horizon> confirmed;
        };

        /// Current block height of accumulated state.
        size_t top_height{};

        /// Accumulated scaled fee in decayed buckets by horizon.
        /// Array count is the half life of the decay it implies.
        std::array<bucket<horizon::small>,  sizing::count> small{};
        std::array<bucket<horizon::medium>, sizing::count> medium{};
        std::array<bucket<horizon::large>,  sizing::count> large{};
    };

    // C++23: make consteval.
    static inline double decay_rate() NOEXCEPT
    {
        static const auto rate = std::pow(0.5, 1.0 / sizing::count);
        return rate;
    }

    // C++23: make constexpr.
    static inline double to_scale_term(size_t age) NOEXCEPT
    {
        return std::pow(decay_rate(), age);
    }

    // C++23: make constexpr.
    static inline double to_scale_factor(bool push) NOEXCEPT
    {
        return std::pow(decay_rate(), push ? +1.0 : -1.0);
    }

    const accumulator& history() const NOEXCEPT;
    bool initialize(const rate_sets& blocks) NOEXCEPT;
    bool push(const rates& block) NOEXCEPT;
    bool pop(const rates& block) NOEXCEPT;
    uint64_t compute(size_t target, double confidence,
        bool geometric=false) const NOEXCEPT;

private:
    bool update(const rates& block, size_t height, bool push) NOEXCEPT;
    void decay(auto& buckets, double factor) NOEXCEPT;
    void decay(bool push) NOEXCEPT;

    accumulator fees_{};
};

} // namespace server
} // namespace libbitcoin

#endif
