/**
 * Copyright (c) 2011-2026 libbitcoin developers
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

namespace libbitcoin {
namespace server {

using format = boost_format;

constexpr auto giga = system::power2<uint64_t>(30u);

#if defined(HAVE_APPLE)
constexpr auto minimum_memory = 10_u64 * giga;
constexpr auto validate_memory = 18_u64 * giga;
#else
constexpr auto minimum_memory = 8_u64 * giga;
constexpr auto validate_memory = 16_u64 * giga;
#endif

constexpr auto recommend_memory = 32_u64 * giga;
constexpr auto require_space = 1024_u64 * giga;
constexpr auto limited_space = 512_u64 * giga;
constexpr auto bypass_height = 950'000_size;

// Warnings (emitted only when there is something to report).
// ----------------------------------------------------------------------------

// Limited blocks reduce storage only for blocks the milestone bypasses.
bool executor::milestoned() const
{
    return metadata_.configured.bitcoin.milestone.height() >= bypass_height;
}

void executor::warn_hardware(system::string_list& out) const
{
    using namespace system;

    const auto device = database::gpu_device();

#if defined(HAVE_ARM)
    const auto suboptimal =
        (try_neon() && !have_128) ||
        (try_crypto() && !have_sha);
#else
    const auto suboptimal =
        (try_avx512() && !have_512) ||
        (try_avx2() && !have_256) ||
        (try_sse41() && !have_128) ||
        (try_shani() && !have_sha);
#endif

    if (suboptimal || (device && !batched::compiled()))
        out.emplace_back(BS_HARDWARE_SUBOPTIMAL);

    if (device && batched::compiled() && !batched::accelerated())
        out.emplace_back(BS_HARDWARE_UNSUPPORTED);

    if (batched::accelerated() &&
        !metadata_.configured.node.batch_signatures_enabled())
        out.emplace_back(BS_HARDWARE_UNCONFIGURED);
}

void executor::warn_memory(system::string_list& out) const
{
    const auto memory = database::system_memory();
    if (is_zero(memory))
        return;

    // A milestone bypasses validation, which is what raises the minimum.
    const auto milestone = !is_zero(
        metadata_.configured.bitcoin.milestone.height());

    if (memory < minimum_memory)
        out.emplace_back(BS_MEMORY_BELOW_MINIMUM);
    else if (!milestone && memory < validate_memory)
        out.emplace_back(BS_MEMORY_BELOW_VALIDATION);
    else if (memory < recommend_memory)
        out.emplace_back(BS_MEMORY_BELOW_RECOMMENDED);
    else
        return;

    out.emplace_back((format(BS_MEMORY_PHYSICAL) % (memory / giga)).str());
}

void executor::warn_space(system::string_list& out) const
{
    size_t available{};
    if (!database::file::space(available, metadata_.configured.database.path))
        return;

    // Limited blocks drop witness and input scripts for bypassed blocks.
    const auto limited = metadata_.configured.node.limited_blocks &&
        milestoned();

    const auto space = limited ? limited_space : require_space;
    const auto store = query_.store_size();
    const auto require = space > store ? space - store : zero;
    if (available >= require)
        return;

    out.emplace_back(BS_SPACE_BELOW_REQUIRED);
    out.emplace_back((format(BS_SPACE_AVAILABLE) % (available / giga)).str());
}

void executor::warn_storage(system::string_list& out) const
{
    const auto& path = metadata_.configured.database.path;
    if (!database::solid_state(path) || !database::internal_storage(path))
        out.emplace_back(BS_STORAGE_NOT_INTERNAL);
}

bool executor::prompt_warnings() const
{
    system::string_list warnings{};
    warn_hardware(warnings);
    warn_memory(warnings);
    warn_space(warnings);
    warn_storage(warnings);
    if (warnings.empty())
        return true;

    logger(BS_PROMPT_SETOFF);
    for (const auto& warning: warnings)
        logger(warning);

    if (!interactive() || metadata_.configured.accept)
    {
        logger(BS_PROMPT_SETOFF);
        return true;
    }

    logger(BS_WARNINGS_CHOICE1);
    logger(BS_WARNINGS_CHOICE2);
    logger(BS_PROMPT_SETOFF);

    std::string line{};
    while (std::getline(input_, line) && !canceled())
    {
        system::trim(line);
        if (line.empty())
            return true;

        if (line == "c")
        {
            logger(BS_WARNINGS_HALTED);
            return false;
        }

        logger(BS_WARNINGS_CHOICE1);
        logger(BS_WARNINGS_CHOICE2);
    }

    return false;
}

} // namespace server
} // namespace libbitcoin
