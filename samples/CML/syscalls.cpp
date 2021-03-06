/*
 *   Name: syscalls.cpp
 *
 *   Copyright (c) Mateusz Semegen and contributors. All rights reserved.
 *   Licensed under the MIT license. See LICENSE file in the project root for details.
 */

// std
#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

// cml
#include <cml/hal/peripherals/USART.hpp>

using namespace cml::hal::peripherals;

namespace {
USART* p_iostream;
} // namespace

void initialize_syscalls(USART* a_p_iostream)
{
    p_iostream = a_p_iostream;
}

extern "C" {

static uint8_t* __sbrk_heap_end = nullptr;

int _read(int, char* data, int len)
{
    using namespace cml::hal::peripherals;

    int read_len = 0;

    while (read_len < len)
    {
        char c          = 0;
        USART::Result r = p_iostream->receive_bytes_polling(&c, sizeof(c));

        if (USART::Bus_status_flag::ok == r.bus_status)
        {
            if ('\b' == c)
            {
                if (0 != read_len)
                {
                    p_iostream->transmit_bytes_polling("\b \b", 3);
                    data[read_len] = 0;
                    read_len--;
                }
            }
            else
            {
                p_iostream->transmit_bytes_polling(&c, sizeof(c));
                read_len += static_cast<int>(r.data_length_in_words);
                data[read_len - 1] = c;

                if ('\r' == c)
                {
                    return read_len;
                }
            }
        }
        else
        {
            return -1;
        }
    }

    return read_len;
}

int _write(int, char* data, int len)
{
    using namespace cml::hal::peripherals;

    USART::Result r = p_iostream->transmit_bytes_polling(data, len);
    if (USART::Bus_status_flag::ok != r.bus_status)
    {
        return -1;
    }
    else
    {
        return r.data_length_in_words;
    }
}

void* _sbrk(std::ptrdiff_t incr)
{
    extern uint8_t _end;
    extern uint8_t _estack;
    extern uint32_t _Min_Stack_Size;
    const uint32_t stack_limit = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;
    const uint8_t* max_heap    = (uint8_t*)stack_limit;
    uint8_t* prev_heap_end;

    if (nullptr == __sbrk_heap_end)
    {
        __sbrk_heap_end = &_end;
    }

    if (__sbrk_heap_end + incr > max_heap)
    {
        errno = ENOMEM;
        return (void*)-1;
    }

    prev_heap_end = __sbrk_heap_end;
    __sbrk_heap_end += incr;

    return (void*)prev_heap_end;
}

int _close(int file)
{
    return -1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}
int _fstat(int file, struct stat* st)
{
    st->st_mode = S_IFCHR;
    return 0;
}
int _isatty(int file)
{
    return 0;
}

void _exit(int status)
{
    while (1)
        ;
}

int _kill(int pid, int sig)
{
    errno = EINVAL;
    return -1;
}

int _getpid()
{
    return 1;
}

} // extern "C";
