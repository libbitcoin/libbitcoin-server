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
#include <malloc.h>
#include <exception>
#include <string>
#include <utility>
#include <boost/ptr_container/ptr_map.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>

extern "C"
{

// Required implementation provided for mbedtls random data usage.
int mg_ssl_if_mbed_random(void* connection, uint8_t* buffer, size_t length)
{
    bc::data_chunk data;
    data.reserve(length);
    bc::pseudo_random_fill(data);
    std::memcpy(buffer, data.data(), data.size());
    return 0;
}

// All of the below methods are either replacements or helpers of
// methods used/defined in mongoose sources, in order to avoid memory
// corruption and leaks that exist in the original sources.

struct allocation
{
    void* pointer;
    size_t size;
};

boost::ptr_map<void*, allocation> allocations;

// Provided to ensure that memory is not leaked through mongoose
// usage.
void* manager_malloc(size_t size)
{
    if (size == 0)
        return nullptr;

    auto allocated = malloc(size);
    allocations[allocated] = allocation{ allocated, size };
    return allocated;
}

void* manager_calloc(size_t element_size, size_t count)
{
    if (count == 0 || element_size == 0)
        return nullptr;

    auto allocated = manager_malloc(element_size * count);
    std::memset(allocated, 0, element_size * count);
    return allocated;
}

void* manager_realloc(void* data, size_t size)
{
    if (data == nullptr)
        return size == 0 ? nullptr : manager_malloc(size);

    auto it = allocations.find(data);
    BITCOIN_ASSERT(it != allocations.end());
    if (data != nullptr && size == 0)
    {
        // treat as free
        free(it->first);
        allocations.erase(it);
        return nullptr;
    }

    BITCOIN_ASSERT(size > it->second->size);
    auto allocated = malloc(size);
    memcpy(allocated, it->first, it->second->size);
    free(it->first);
    allocations.erase(it);
    allocations[allocated] = allocation{ allocated, size };
    return allocated;
}

void manager_free(void* data)
{
    if (data == nullptr)
        return;

    auto it = allocations.find(data);
    if (it == allocations.end())
        return;

    if (it->first != nullptr && it->second->size)
        free(it->first);

    allocations.erase(it);
}

void* manager_memmove(void* destination, const void* source, size_t count)
{
    static constexpr size_t temporary_buffer_length = 4096;
    static std::array<char, temporary_buffer_length> data;

    auto copy_destination = (count < temporary_buffer_length) ?
        static_cast<void*>(data.data()) : manager_malloc(count);

    if (copy_destination == nullptr)
        return nullptr;

    std::memcpy(copy_destination, source, count);
    std::memcpy(destination, copy_destination, count);

    if (copy_destination != static_cast<void*>(data.data()))
        manager_free(copy_destination);

    return destination;
}

typedef std::vector<char> data_buffer;

struct memory_buffer
{
    data_buffer data;
    struct mbuf* buffer;
};

std::unordered_map<struct mbuf*, std::shared_ptr<memory_buffer>> buffer_map;

// Initializes an mbuf (a structure defined within mongoose and used
// throughout internally).  'initial_capacity' specifies the initial
// capacity of the mbuf.
void mbuf_init(struct mbuf* mbuf, size_t initial_capacity)
{
    static constexpr size_t default_capacity = 2048;

    auto buffer = std::make_shared<memory_buffer>();
    buffer->data.reserve(initial_capacity == 0 ? default_capacity :
        initial_capacity);
    buffer->buffer = mbuf;
    mbuf->buf = buffer->data.data();
    mbuf->len = buffer->data.size();
    mbuf->size = buffer->data.capacity();
    buffer_map[mbuf] = buffer;
}

// Frees the space allocated for the mbuffer and resets the mbuf
// structure.  Does not remove the entry in the buffer_map since those
// are re-used.
void mbuf_free(struct mbuf* mbuf)
{
    auto it = buffer_map.find(mbuf);
    if (it == buffer_map.end())
        return;

    auto& buffer = it->second;
    BITCOIN_ASSERT(buffer->buffer == mbuf);

    buffer->data.clear();
    mbuf->buf = buffer->data.data();
    mbuf->len = buffer->data.size();
    mbuf->size = buffer->data.capacity();
}

// Appends data to the mbuf.  Returns the number of bytes appended or
// 0 if out of memory.
size_t mbuf_append(struct mbuf* mbuf, const void* data, size_t data_size)
{
    auto it = buffer_map.find(mbuf);
    if (it == buffer_map.end())
    {
        mbuf_init(mbuf, data_size);
        it = buffer_map.find(mbuf);
        BITCOIN_ASSERT(it != buffer_map.end());
    }

    auto& buffer = it->second;
    BITCOIN_ASSERT(buffer->buffer == mbuf);
    BITCOIN_ASSERT(mbuf->buf == buffer->data.data());

    if ((mbuf->buf == nullptr && mbuf->size == 0) || (mbuf->len == 0))
    {
        mbuf_init(mbuf, 0);
        it = buffer_map.find(mbuf);
        BITCOIN_ASSERT(it != buffer_map.end());
        buffer = it->second;
        BITCOIN_ASSERT(buffer->data.empty());
    }

    const auto char_data = static_cast<const char*>(data);
    data_buffer cur_data(char_data, char_data + data_size);

    if ((mbuf->size == 0) || (mbuf->len + cur_data.size() >
        buffer->data.capacity()))
        buffer->data.reserve(mbuf->len + cur_data.size());

    if (mbuf->len == 0 || buffer->data.empty())
        buffer->data = cur_data;
    else
        buffer->data.insert(buffer->data.end(), cur_data.begin(),
            cur_data.end());

    mbuf->buf = buffer->data.data();
    mbuf->len += data_size;
    mbuf->size = buffer->data.capacity();
    return data_size;
}

// Inserts data at a specified offset in the mbuf.  Existing data will
// be shifted forward and the buffer will be grown if necessary.
// Returns the number of bytes inserted.
size_t mbuf_insert(struct mbuf* mbuf, size_t offset, const void* data,
    size_t data_size)
{
    auto it = buffer_map.find(mbuf);
    if (it == buffer_map.end())
        return 0;

    auto& buffer = it->second;
    BITCOIN_ASSERT(buffer->buffer == mbuf);

    BITCOIN_ASSERT(offset <= buffer->data.size());
    const auto it_offset = buffer->data.begin() + offset;
    const auto char_data = static_cast<const char*>(data);
    data_buffer new_data(char_data, char_data + data_size);

    buffer->data.insert(it_offset, new_data.begin(), new_data.end());
    mbuf->len += data_size;
    mbuf->size = buffer->data.capacity();
    return data_size;
}

/* Removes `data_size` bytes from the beginning of the buffer. */
void mbuf_remove(struct mbuf* mbuf, size_t data_size)
{
    if (data_size > mbuf->len)
        return;

    auto it = buffer_map.find(mbuf);
    if (it == buffer_map.end())
        return;

    auto& buffer = it->second;
    BITCOIN_ASSERT(buffer->buffer == mbuf);

    // Check if mongoose replaced the backing buffer.
    if (mbuf->buf != buffer->data.data())
    {
        BITCOIN_ASSERT((mbuf->len - data_size) >= 0);
        const auto char_data = static_cast<const char*>(mbuf->buf + data_size);
        data_buffer subset(char_data, char_data + (mbuf->len - data_size));

        buffer->data.insert(buffer->data.end(), subset.begin(), subset.end());

        mbuf->buf = buffer->data.data();
        mbuf->len = buffer->data.size();
        mbuf->size = buffer->data.capacity();
        return;
    }

    BITCOIN_ASSERT(mbuf->len >= data_size);
    BITCOIN_ASSERT(mbuf->size == buffer->data.capacity());
    BITCOIN_ASSERT(mbuf->buf == buffer->data.data());

    if (data_size == buffer->data.size())
    {
        buffer->data.clear();
        mbuf->len = 0;
        mbuf->size = buffer->data.capacity();
        return;
    }

    buffer->data.erase(buffer->data.begin(), buffer->data.begin() +
        data_size);
    mbuf->len -= data_size;
    mbuf->size = buffer->data.capacity();
}

// Resizes an mbuf.  If 'new_size' is smaller than buffer's 'len', the
// resize is not performed.
void mbuf_resize(struct mbuf* mbuf, size_t new_size)
{
    auto it = buffer_map.find(mbuf);
    if (it == buffer_map.end())
        return;

    auto& buffer = it->second;
    BITCOIN_ASSERT(buffer->buffer == mbuf);

    // Check if mongoose replaced the backing buffer.
    if (mbuf->buf != buffer->data.data())
    {
        BITCOIN_ASSERT(mbuf->len >= new_size);
        buffer->data.clear();
        std::memcpy(buffer->data.data(), mbuf->buf, new_size);
        if (buffer->buffer != mbuf)
            manager_free(mbuf->buf);
        mbuf->buf = buffer->data.data();
        mbuf->len = buffer->data.size();
        BITCOIN_ASSERT(buffer->data.size() == new_size);
        mbuf->size = buffer->data.capacity();
        return;
    }

    BITCOIN_ASSERT(mbuf->buf == buffer->data.data());
    BITCOIN_ASSERT(mbuf->len == buffer->data.size());
    BITCOIN_ASSERT(mbuf->size == buffer->data.capacity());

    if (new_size < buffer->data.size())
        return;

    buffer->data.resize(new_size);
    BITCOIN_ASSERT(buffer->data.size() == new_size);
    mbuf->len = new_size;
    mbuf->size = buffer->data.capacity();
}

// Shrinks an mbuf by resizing it's 'size' to 'len'.
void mbuf_trim(struct mbuf* mbuf)
{
    auto it = buffer_map.find(mbuf);
    if (it == buffer_map.end())
        return;

    auto& buffer = it->second;
    BITCOIN_ASSERT(buffer->buffer == mbuf);

    // Check if mongoose replaced the backing buffer.
    if (mbuf->buf != buffer->data.data())
    {
        BITCOIN_ASSERT(mbuf->size >= mbuf->len);
        buffer->data.clear();
        std::memcpy(buffer->data.data(), mbuf->buf, mbuf->len);

        if (buffer->buffer != mbuf)
            manager_free(mbuf->buf);
        mbuf->buf = buffer->data.data();
        buffer->data.resize(mbuf->len);
        buffer->data.shrink_to_fit();
        mbuf->len = buffer->data.size();
        mbuf->size = buffer->data.capacity();
        return;
    }

    BITCOIN_ASSERT(mbuf->buf == buffer->data.data());
    BITCOIN_ASSERT(mbuf->size == buffer->data.capacity());

    if (mbuf->len == 0)
        buffer->data.clear();
    else
        buffer->data.resize(mbuf->len);

    mbuf->len = buffer->data.size();
    mbuf->size = buffer->data.capacity();
}

size_t mbuf_memmove(struct mbuf* mbuf, size_t offset, const void* data,
    size_t data_size)
{
    auto it = buffer_map.find(mbuf);
    if (it == buffer_map.end())
    {
        mbuf_init(mbuf, data_size);
        it = buffer_map.find(mbuf);
        BITCOIN_ASSERT(it != buffer_map.end());
    }

    auto& buffer = it->second;
    BITCOIN_ASSERT(buffer->buffer == mbuf);

    auto dest = buffer->data.data() + offset;
    manager_memmove(dest, data, data_size);
    mbuf->len -= static_cast<const char*>(data) - mbuf->buf;
}

} // extern "C"
