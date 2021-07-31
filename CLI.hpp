#pragma once

/*
 *   Name: CLI.hpp
 *
 *   Copyright (c) Mateusz Semegen and contributors. All rights reserved.
 *   Licensed under the MIT license. See LICENSE file in the project root for details.
 */

// std
#include <algorithm>
#include <stdint.h>
#include <string.h>

#ifdef CML
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
    {
        static constexpr uint32_t input_buffer_capacity    = 3u;
        static constexpr uint32_t max_parameters_count     = 10u;
        static constexpr uint32_t line_buffer_capacity     = 128u;
        static constexpr uint32_t carousel_buffer_capacity = 5u;
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
        using Function = void (*)(const char* a_p_string, uint32_t a_length, void* a_p_user_data);

        Function function = nullptr;
        void* p_user_data = nullptr;
    };

    struct Read_character_handler
    {
        using Function = uint32_t (*)(char* a_p_buffer, uint32_t a_buffer_size, void* a_p_user_data);

        Function function = nullptr;
        void* p_user_data = nullptr;
    };

    struct Callback
    {
        using Function = void (*)(const char** a_p_argv, uint32_t a_argc, void* a_p_user_data);

        const char* p_name = nullptr;

        Function function = nullptr;
        void* p_user_data = nullptr;
    };

public:
    CLI(const Write_character_handler& a_write_character,
        const Write_string_handler& a_write_string,
        const Read_character_handler& a_read_character,
        New_line_mode_flag a_new_line_mode_input,
        New_line_mode_flag a_new_line_mode_output,
        const Callback* a_p_callbacks,
        uint32_t a_callbacks_count)
        : write_character(a_write_character)
        , write_string(a_write_string)
        , read_character(a_read_character)
        , new_line_mode_input(a_new_line_mode_input)
        , new_line_mode_output(a_new_line_mode_output)
        , p_callbacks(a_p_callbacks)
        , callbacks_count(a_callbacks_count)
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

    void update(const char* a_p_prompt,
                uint32_t a_prompt_length,
                const char* a_p_command_not_found_message,
                uint32_t a_command_not_found_message_lenght,
                Echo a_echo)
    {
#ifdef CLI_ASSERT
        CLI_ASSERT(nullptr != a_p_prompt);
#endif
        char c[s::input_buffer_capacity] = { 0 };
        uint32_t r = this->read_character.function(c, sizeof(c) / sizeof(c[0]), this->read_character.p_user_data);

        if (0 != r)
        {
#ifdef CLI_CAROUSEL
            bool ap = false;
#endif
            for (uint32_t i = 0; i < r
#ifdef CLI_CAROUSEL
                                 && false == ap
#endif
                 ;
                 i++)
            {
                switch (c[i])
                {
                    case '\r': {
                        if (New_line_mode_flag::cr == this->new_line_mode_input)
                        {
                            this->execute(a_p_prompt,
                                          a_prompt_length,
                                          a_p_command_not_found_message,
                                          a_command_not_found_message_lenght,
                                          a_echo);
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
                            this->execute(a_p_prompt,
                                          a_prompt_length,
                                          a_p_command_not_found_message,
                                          a_command_not_found_message_lenght,
                                          a_echo);
                            this->line_buffer_size = 0;
                        }
                    }
                    break;
                    case '\b':
                    case 127u: {
                        if (this->line_buffer_size > 0)
                        {
                            this->line_buffer_size--;
                            this->write_string.function("\b \b", 3 * sizeof(char), this->write_string.p_user_data);
                        }
                    }
                    break;
#ifdef CLI_AUTOCOMPLETION
                    case '\t': {
                        this->line_buffer_size = std::min(this->line_buffer_size, s::line_buffer_capacity - 1u);
                        this->line_buffer[this->line_buffer_size] = 0;

                        const char* p = nullptr;
                        bool m        = false;
                        for (uint32_t i = 0; i < this->callbacks_count; i++)
                        {
                            if (nullptr == p)
                            {
                                p = strstr(this->p_callbacks[i].p_name, this->line_buffer);
                                p = this->p_callbacks[i].p_name == p ? p : nullptr;
                            }
                            else
                            {
                                const char* p2 = strstr(this->p_callbacks[i].p_name, this->line_buffer);
                                if (nullptr != p2 && this->p_callbacks[i].p_name == p2)
                                {
                                    if (false == m)
                                    {
                                        this->write_new_line();
                                        this->write_string.function(
                                            p, static_cast<uint32_t>(strlen(p)), this->write_string.p_user_data);
                                    }
                                    m = true;
                                    this->write_character.function(' ', this->write_character.p_user_data);

                                    this->write_string.function(
                                        this->p_callbacks[i].p_name,
                                        static_cast<uint32_t>(strlen(this->p_callbacks[i].p_name)),
                                        this->write_string.p_user_data);
                                }
                            }
                        }

                        if (false == m && nullptr != p)
                        {
                            this->clear_line(this->line_buffer_size + a_prompt_length);
                            this->line_buffer_size = 0;

                            this->line_buffer_size =
                                std::min(s::line_buffer_capacity, static_cast<uint32_t>(strlen(p)));
                            memcpy(this->line_buffer, p, this->line_buffer_size);

                            this->write_string.function(a_p_prompt, a_prompt_length, this->write_string.p_user_data);
                            this->write_string.function(
                                this->line_buffer, this->line_buffer_size, this->write_string.p_user_data);
                        }
                        else if (true == m && nullptr != p)
                        {
                            this->line_buffer_size = 0;
                            this->write_new_line();
                            this->write_string.function(a_p_prompt, a_prompt_length, this->write_string.p_user_data);
                        }
                    }
                    break;
#endif
                    default: {
                        if (this->line_buffer_size < s::line_buffer_capacity && '\033' != c[0]
#ifdef _WIN32
                            && 0 != c[i]
#endif
                        )
                        {
                            this->line_buffer[this->line_buffer_size++] = c[i];

                            if (Echo::enabled == a_echo)
                            {
                                this->write_character.function(c[i], this->write_character.p_user_data);
                            }
                        }
#ifdef CLI_CAROUSEL
                        else if ('\033' == c[0] && '[' == c[1])
                        {
                            if (false == this->carousel.is_empty())
                            {
                                const char* p = 'A' == c[2] ? this->carousel.get_previus() :
                                                              ('B' == c[2] ? this->carousel.get_next() : nullptr);

                                if (nullptr != p)
                                {
                                    this->clear_line(this->line_buffer_size + a_prompt_length);
                                    this->line_buffer_size = 0;

                                    this->line_buffer_size                    = static_cast<uint32_t>(strlen(p));
                                    this->line_buffer[this->line_buffer_size] = 0;
                                    memcpy(this->line_buffer, p, this->line_buffer_size);

                                    this->write_string.function(
                                        a_p_prompt, a_prompt_length, this->write_string.p_user_data);
                                    this->write_string.function(
                                        this->line_buffer, this->line_buffer_size, this->write_string.p_user_data);

                                    ap = true;
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

    void execute(const char* a_p_prompt,
                 uint32_t a_prompt_lenght,
                 const char* a_p_command_not_found_message,
                 uint32_t a_command_not_found_message_lenght,
                 Echo a_echo)
    {
        const char* argv[s::max_parameters_count] = { nullptr };
        uint32_t argc                             = 0;
        bool callback_found                       = false;

        this->line_buffer[this->line_buffer_size] = 0;

#ifdef CLI_CAROUSEL
        if (0 != this->line_buffer_size)
        {
            this->carousel.push(this->line_buffer, this->line_buffer_size);
        }
#endif

        if (Echo::enabled == a_echo)
        {
            this->write_new_line();
        }

        argv[argc] = strtok(this->line_buffer, " ");
        while (nullptr != argv[argc] && argc < s::max_parameters_count)
        {
            argv[++argc] = strtok(nullptr, " ");
        }

        for (uint32_t i = 0;
             i < this->callbacks_count && false == callback_found && nullptr != argv[0] && this->line_buffer_size > 1;
             i++)
        {
            if (0 == strcmp(argv[0], this->p_callbacks[i].p_name))
            {
                this->p_callbacks[i].function(argv, argc, this->p_callbacks[i].p_user_data);
                callback_found = true;
            }
        }

        if (false == callback_found && argc != 0)
        {
            if (nullptr != a_p_command_not_found_message)
            {
                this->write_string.function(
                    a_p_command_not_found_message, a_command_not_found_message_lenght, this->write_string.p_user_data);
            }

            this->write_new_line();
        }

        this->write_string.function(a_p_prompt, a_prompt_lenght, this->write_string.p_user_data);
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
                this->write_string.function("\r\n", 2 * sizeof(char), this->write_string.p_user_data);
            }
        }
    }

    void clear_line(uint32_t a_length)
    {
        memset(this->line_buffer, ' ', a_length);
        this->line_buffer[a_length] = 0;

        this->write_character.function('\r', this->write_character.p_user_data);
        this->write_string.function(this->line_buffer, a_length, this->write_string.p_user_data);
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
            for (uint32_t i = 0; i < s::carousel_buffer_capacity; i++)
            {
                memset(this->buffer[i], 0x0u, sizeof(this->buffer[i]));
            }
        }

        void push(const char* a_p_data, uint32_t a_length)
        {
#ifdef CLI_ASSERT
            CLI_ASSERT(a_length > 0);
#endif
            this->buffer[this->write_index][a_length] = 0;
            memcpy(this->buffer[this->write_index++], a_p_data, a_length);

            if (this->write_index == s::carousel_buffer_capacity)
            {
                this->write_index = 0;
            }

            if (this->buffer_size < CLI::s::carousel_buffer_capacity)
            {
                this->buffer_size++;
            }
        }

        const char* get_next() const
        {
            const char* p = this->buffer[this->read_index++];

            if (this->buffer_size == this->read_index)
            {
                this->read_index = 0;
            }

            return p;
        }

        const char* get_previus() const
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

        mutable uint32_t read_index;
        uint32_t write_index;
        uint32_t buffer_size;
    };
#endif

    Write_character_handler write_character;
    Write_string_handler write_string;
    Read_character_handler read_character;

    New_line_mode_flag new_line_mode_input;
    New_line_mode_flag new_line_mode_output;

    const Callback* p_callbacks;
    uint32_t callbacks_count;

    char line_buffer[s::line_buffer_capacity];
    uint32_t line_buffer_size;

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