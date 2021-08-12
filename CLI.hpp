#pragma once

/*
 *   Name: CLI.hpp
 *
 *   Copyright (c) Mateusz Semegen and contributors. All rights reserved.
 *   Licensed under the MIT license. See LICENSE file in the project root for details.
 */

// std
#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

#ifdef CML
#include <cml/Non_constructible.hpp>
#include <cml/Non_copyable.hpp>
#endif

#ifdef _WIN32
#include <Windows.h>
#undef max
#undef min
#endif

namespace modules {

class CLI
#ifdef CML
    : private cml::Non_copyable
#endif
{
public:
    struct s
#ifdef CML
        : private cml::Non_constructible
#endif
    {
#ifdef CLI_COMMAND_PARAMETERS
        static constexpr size_t max_parameters_count = 10u;
#endif
        static constexpr size_t input_buffer_capacity    = 3u;
        static constexpr size_t line_buffer_capacity     = 128u;
        static constexpr size_t carousel_buffer_capacity = 5u;

#ifndef CML
        s()         = delete;
        s(const s&) = delete;
        s(s&&)      = delete;
        ~s()        = delete;

        s& operator=(const s&) = delete;
        s& operator=(s&&) = delete;
#endif
    };

    enum class Echo : uint32_t
    {
        disabled,
        enabled
    };

    enum class New_line_mode_flag : uint32_t
    {
        cr = 0x1u,
        lf = 0x2u
    };

    struct Write_character_handler
    {
        using Function = void (*)(char a_character, void* a_p_user_data);

        Function function = nullptr;
        void* p_user_data = nullptr;
    };

    struct Write_string_handler
    {
        using Function = void (*)(std::string_view a_string, void* a_p_user_data);

        Function function = nullptr;
        void* p_user_data = nullptr;
    };

    struct Read_character_handler
    {
        using Function = size_t (*)(char* a_p_buffer, size_t a_buffer_size, void* a_p_user_data);

        Function function = nullptr;
        void* p_user_data = nullptr;
    };

#ifdef CLI_COMMAND_PARAMETERS
    struct Callback
    {
        using Function = void (*)(std::string_view a_argv[], size_t a_argc, void* a_p_user_data);

        std::string_view name;

        Function function = nullptr;
        void* p_user_data = nullptr;
    };
#else
    struct Callback
    {
        using Function = void (*)(void* a_p_user_data);

        std::string_view name;

        Function function = nullptr;
        void* p_user_data = nullptr;
    };
#endif

public:
    CLI(const Write_character_handler& a_write_character,
        const Write_string_handler& a_write_string,
        const Read_character_handler& a_read_character,
        New_line_mode_flag a_new_line_mode_input,
        New_line_mode_flag a_new_line_mode_output)
        : write_character(a_write_character)
        , write_string(a_write_string)
        , read_character(a_read_character)
        , new_line_mode_input(a_new_line_mode_input)
        , new_line_mode_output(a_new_line_mode_output)
        , line_buffer_size(0)
#ifdef _WIN32
        , win32_mode(0)
#endif
    {
#ifdef CLI_ASSERT
        CLI_ASSERT(nullptr != a_write_character.function);
        CLI_ASSERT(nullptr != a_write_string.function);
        CLI_ASSERT(nullptr != a_read_character.function);
#endif
        memset(this->line_buffer, 0x0u, sizeof(line_buffer));
#ifdef _WIN32
        GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &(this->win32_mode));
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_INPUT | this->win32_mode);
#endif
    }

#ifdef _WIN32
    ~CLI()
    {
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), this->win32_mode);
    }
#endif

    template<size_t callbacks_count> void update(std::string_view a_prompt,
                                                 std::string_view a_command_not_found_message,
                                                 const std::array<Callback, callbacks_count>& a_callbacks,
                                                 Echo a_echo)
    {
        char c[s::input_buffer_capacity] = { 0 };
        size_t r = this->read_character.function(c, sizeof(c) / sizeof(c[0]), this->read_character.p_user_data);

        if (0 != r)
        {
#ifdef CLI_CAROUSEL
            bool escape_handled = false;
#endif
            for (size_t char_index = 0; char_index < r
#ifdef CLI_CAROUSEL
                                        && false == escape_handled
#endif
                 ;
                 char_index++)
            {
                switch (c[char_index])
                {
                    case '\r': {
                        if (New_line_mode_flag::cr == this->new_line_mode_input)
                        {
                            this->execute(a_prompt, a_command_not_found_message, a_callbacks, a_echo);
                            this->line_buffer_size = 0;
                        }
                    }
                    break;
                    case '\n': {
                        if (New_line_mode_flag::lf == this->new_line_mode_input ||
                            (static_cast<uint32_t>(this->new_line_mode_input) ==
                             (static_cast<uint32_t>(CLI::New_line_mode_flag::cr) |
                              static_cast<uint32_t>(CLI::New_line_mode_flag::lf))))
                        {
                            this->execute(a_prompt, a_command_not_found_message, a_callbacks, a_echo);
                            this->line_buffer_size = 0;
                        }
                    }
                    break;
                    case '\b':
                    case 127u: {
                        if (this->line_buffer_size > 0)
                        {
                            this->line_buffer_size--;
                            this->write_string.function("\b \b", this->write_string.p_user_data);
                        }
                    }
                    break;
#ifdef CLI_AUTOCOMPLETION
                    case '\t': {
#ifdef CLI_ASSERT
                        CLI_ASSERT(s::line_buffer_capacity > this->line_buffer_size);
#endif
                        size_t indexes_found[callbacks_count];
                        size_t indexes_found_count = 0;

                        this->line_buffer[this->line_buffer_size] = 0;
                        for (size_t i = 0; i < a_callbacks.size(); i++)
                        {
                            if (std::string_view::npos != a_callbacks[i].name.find(this->line_buffer))
                            {
                                indexes_found[indexes_found_count++] = i;
                            }
                        }

                        this->clear_line(this->line_buffer_size + a_prompt.length());
                        this->write_string.function(a_prompt, this->write_string.p_user_data);
                        this->line_buffer_size = 0;

                        if (1 == indexes_found_count)
                        {
                            this->line_buffer_size = a_callbacks[indexes_found[0]].name.length();

                            this->write_string.function(a_callbacks[indexes_found[0]].name,
                                                        this->write_string.p_user_data);
                            memcpy(
                                this->line_buffer, a_callbacks[indexes_found[0]].name.data(), this->line_buffer_size);
                        }
                        else if (0 != indexes_found_count)
                        {
                            this->write_new_line();

                            for (size_t i = 0; i < indexes_found_count; i++)
                            {
                                this->write_character.function(' ', this->write_character.p_user_data);
                                this->write_string.function(a_callbacks[indexes_found[i]].name,
                                                            this->write_string.p_user_data);
                            }

                            this->write_new_line();
                            this->write_string.function(a_prompt, this->write_string.p_user_data);
                        }
                    }
                    break;
#endif
                    default: {
                        if (this->line_buffer_size + 1 < s::line_buffer_capacity && '\033' != c[0]
#ifdef _WIN32
                            && 0 != c[char_index]
#endif
                        )
                        {
                            this->line_buffer[this->line_buffer_size++] = c[char_index];

                            if (Echo::enabled == a_echo)
                            {
                                this->write_character.function(c[char_index], this->write_character.p_user_data);
                            }
                        }
#ifdef CLI_CAROUSEL
                        else if ('\033' == c[0] && '[' == c[1])
                        {
                            if (false == this->carousel.is_empty())
                            {
                                std::string_view line_data = 'A' == c[2] ?
                                                                 this->carousel.get_previus() :
                                                                 ('B' == c[2] ? this->carousel.get_next() : "");

                                if (false == line_data.empty())
                                {
                                    this->clear_line(this->line_buffer_size + a_prompt.length());
                                    this->line_buffer_size = 0;

                                    this->line_buffer_size                    = line_data.length();
                                    this->line_buffer[this->line_buffer_size] = 0;
                                    memcpy(this->line_buffer, line_data.data(), this->line_buffer_size);

                                    this->write_string.function(a_prompt, this->write_string.p_user_data);
                                    this->write_string.function(this->line_buffer, this->write_string.p_user_data);

                                    escape_handled = true;
                                }
                            }
                            break;
                        }
#endif
                    }
                    break;
                }
            }
        }
    }

private:
#ifndef CML
    CLI(const CLI&) = delete;
    CLI()           = default;
    CLI(CLI&&)      = default;

    CLI& operator=(CLI&&) = default;
    CLI& operator=(const CLI&) = delete;
#endif

    template<size_t callbacks_count> void execute(std::string_view a_prompt,
                                                  std::string_view a_command_not_found_message,
                                                  const std::array<Callback, callbacks_count>& a_callbacks,
                                                  Echo a_echo)
    {
#ifdef CLI_COMMAND_PARAMETERS
        std::string_view argv[s::max_parameters_count];
        size_t argc = 0;
#endif
        bool callback_found                       = false;
        this->line_buffer[this->line_buffer_size] = 0;

#ifdef CLI_CAROUSEL
        if (0 != this->line_buffer_size)
        {
            this->carousel.push(this->line_buffer);
        }
#endif
        if (Echo::enabled == a_echo)
        {
            this->write_new_line();
        }

#ifdef CLI_COMMAND_PARAMETERS
        size_t first = 0;
        std::string_view strv(this->line_buffer);

        while (first < strv.size())
        {
            const size_t second = strv.find_first_of(' ', first);

            if (first != second)
            {
                argv[argc++] = strv.substr(first, second - first);
            }

            if (second == std::string_view::npos)
            {
                break;
            }

            first = second + 1;
        }

        for (size_t i = 0; i < a_callbacks.size() && false == callback_found && this->line_buffer_size > 1; i++)
        {
            if (0 == a_callbacks[i].name.compare(argv[0]))
            {
                a_callbacks[i].function(argv, argc, a_callbacks[i].p_user_data);
                callback_found = true;
            }
        }
#else
        for (size_t i = 0; i < a_callbacks.size() && false == callback_found && this->line_buffer_size > 1; i++)
        {
            if (0 == a_callbacks[i].name.compare(this->line_buffer))
            {
                a_callbacks[i].function(a_callbacks[i].p_user_data);
                callback_found = true;
            }
        }
#endif
        if (false == callback_found && 0 != this->line_buffer_size)
        {
            this->write_string.function(a_command_not_found_message, this->write_string.p_user_data);

            this->write_new_line();
        }

        this->write_string.function(a_prompt, this->write_string.p_user_data);
    }

    void write_new_line()
    {
        switch (this->new_line_mode_output)
        {
            case New_line_mode_flag::cr: {
                this->write_character.function('\r', this->write_character.p_user_data);
            }
            break;

            case New_line_mode_flag::lf: {
                this->write_character.function('\n', this->write_character.p_user_data);
            }
            break;
            default: {
                this->write_string.function("\r\n", this->write_string.p_user_data);
            }
        }
    }

    void clear_line(size_t a_length)
    {
        memset(this->line_buffer, ' ', a_length);
        this->line_buffer[a_length] = 0;

        this->write_character.function('\r', this->write_character.p_user_data);
        this->write_string.function(this->line_buffer, this->write_string.p_user_data);
        this->write_character.function('\r', this->write_character.p_user_data);
    }

private:
#ifdef CLI_CAROUSEL
    class Carousel
#ifdef CML
        : private cml::Non_copyable
#endif
    {
    public:
        Carousel()
            : read_index(0)
            , write_index(0)
            , buffer_size(0)
        {
            for (size_t i = 0; i < s::carousel_buffer_capacity; i++)
            {
                memset(this->buffer[i], 0x0u, sizeof(this->buffer[i]));
            }
        }

        void push(std::string_view a_data)
        {
            this->buffer[this->write_index][a_data.length()] = 0;
            memcpy(this->buffer[this->write_index++], a_data.data(), a_data.length());

            if (this->write_index == s::carousel_buffer_capacity)
            {
                this->write_index = 0;
            }

            if (this->buffer_size < CLI::s::carousel_buffer_capacity)
            {
                this->buffer_size++;
            }
        }

        std::string_view get_next() const
        {
            std::string_view ret = this->buffer[this->read_index++];

            if (this->buffer_size == this->read_index)
            {
                this->read_index = 0;
            }

            return ret;
        }

        std::string_view get_previus() const
        {
#ifdef CLI_ASSERT
            CLI_ASSERT(this->buffer_size > 0);
#endif
            if (0 == this->read_index)
            {
                this->read_index = this->buffer_size - 1;
            }
            else
            {
                this->read_index--;
            }

            return this->buffer[this->read_index];
        }

        bool is_empty() const
        {
            return 0 == this->buffer_size;
        }

#ifndef CML
    private:
        Carousel(const Carousel&) = delete;
        Carousel(Carousel&&)      = default;

        Carousel& operator=(Carousel&&) = default;
        Carousel& operator=(const Carousel&) = delete;
#endif

    private:
        char buffer[CLI::s::carousel_buffer_capacity][CLI::s::line_buffer_capacity];

        mutable size_t read_index;
        size_t write_index;
        size_t buffer_size;
    };
#endif

    Write_character_handler write_character;
    Write_string_handler write_string;
    Read_character_handler read_character;

    New_line_mode_flag new_line_mode_input;
    New_line_mode_flag new_line_mode_output;

    char line_buffer[s::line_buffer_capacity];
    size_t line_buffer_size;

#ifdef CLI_CAROUSEL
    Carousel carousel;
#endif

#ifdef _WIN32
    DWORD win32_mode;
#endif
};

inline constexpr CLI::New_line_mode_flag operator|(CLI::New_line_mode_flag a_f1, CLI::New_line_mode_flag a_f2)
{
    return static_cast<CLI::New_line_mode_flag>(static_cast<uint32_t>(a_f1) | static_cast<uint32_t>(a_f2));
}

inline constexpr CLI::New_line_mode_flag operator&(CLI::New_line_mode_flag a_f1, CLI::New_line_mode_flag a_f2)

{
    return static_cast<CLI::New_line_mode_flag>(static_cast<uint32_t>(a_f1) & static_cast<uint32_t>(a_f2));
}

inline constexpr CLI::New_line_mode_flag operator|=(CLI::New_line_mode_flag& a_f1, CLI::New_line_mode_flag a_f2)
{
    a_f1 = a_f1 | a_f2;
    return a_f1;
}

} // namespace modules