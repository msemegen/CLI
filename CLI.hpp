#pragma once

/*
 *   Name: CLI.hpp
 *
 *   Copyright (c) Mateusz Semegen and contributors. All rights reserved.
 *   Licensed under the MIT license. See LICENSE file in the project root for details.
 */

// std
#include <stdint.h>
#include <string.h>

#ifdef CML
#include <cml/Non_copyable.hpp>
#endif

#ifdef CLI_ASSERT
#ifdef CML
#include <cml/debug/assertion.hpp>
#else
#include <assert.h>
#endif
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
        static constexpr uint32_t max_parameters_count = 10;
        static constexpr uint32_t line_buffer_capacity = 128;
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
        , line_buffer_index(0)
    {
#ifdef CLI_ASSERT
#ifdef CML
        cml_assert(nullptr != a_write_character.function);
        cml_assert(nullptr != a_write_string.function);
        cml_assert(nullptr != a_read_character.function);
#else
        assert(nullptr != a_write_character.function);
        assert(nullptr != a_write_string.function);
        assert(nullptr != a_read_character.function);
#endif
#endif
        memset(this->line_buffer, 0x0u, sizeof(line_buffer));
    }

    void update(const char* a_p_prompt, const char* a_p_command_not_found_message, Echo a_echo)
    {
#ifdef CLI_ASSERT
#ifdef CML
        cml_assert(nullptr != a_p_prompt);
#else
        assert(nullptr != a_p_prompt);
#endif
#endif
        char c[3]  = { 0, 0, 0 };
        uint32_t r = this->read_character.function(c, 3u, this->read_character.p_user_data);

        if (0 != r)
        {
            for (uint32_t i = 0; i < r; i++)
            {
                switch (c[i])
                {
                    case '\r': {
                        if (New_line_mode_flag::cr == this->new_line_mode_input)
                        {
                            this->execute(a_p_prompt, a_p_command_not_found_message, a_echo);
                        }
                    }
                    break;
                    case '\n': {
                        if (New_line_mode_flag::lf == this->new_line_mode_input ||
                            (static_cast<uint32_t>(this->new_line_mode_input) ==
                             (static_cast<uint32_t>(CLI::New_line_mode_flag::cr) |
                              static_cast<uint32_t>(CLI::New_line_mode_flag::lf))))
                        {
                            this->execute(a_p_prompt, a_p_command_not_found_message, a_echo);
                        }
                    }
                    break;
                    case '\b': {
                        if (this->line_buffer_index > 0)
                        {
                            this->line_buffer_index--;
                            this->write_string.function("\b \b", 3 * sizeof(char), this->write_string.p_user_data);
                        }
                    }
                    break;
#ifdef CLI_AUTOCOMPLETION
                    case '\t': {
                        this->line_buffer[this->line_buffer_index] = 0;

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
                            this->clear_line();

                            uint32_t l              = static_cast<uint32_t>(strlen(p));
                            this->line_buffer_index = l < s::line_buffer_capacity ? l : s::line_buffer_capacity;
                            memcpy(this->line_buffer, p, this->line_buffer_index);

                            this->write_string.function(
                                a_p_prompt, static_cast<uint32_t>(strlen(a_p_prompt)), this->write_string.p_user_data);
                            this->write_string.function(
                                this->line_buffer, this->line_buffer_index, this->write_string.p_user_data);
                        }
                        else if (true == m && nullptr != p)
                        {
                            this->line_buffer_index = 0;
                            this->write_new_line();
                            this->write_string.function(
                                a_p_prompt, static_cast<uint32_t>(strlen(a_p_prompt)), this->write_string.p_user_data);
                        }
                    }
                    break;
#endif
                    default: {
                        if (this->line_buffer_index < s::line_buffer_capacity)
                        {
                            this->line_buffer[this->line_buffer_index++] = c[i];

                            if (Echo::enabled == a_echo)
                            {
                                this->write_character.function(c[i], this->write_character.p_user_data);
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

private:
#ifndef CML
    CLI(const CLI&) = delete;
    CLI& operator=(const CLI&) = delete;

    CLI()      = default;
    CLI(CLI&&) = default;

    CLI& operator=(CLI&&) = default;
#endif

    void execute(const char* a_p_prompt, const char* a_p_command_not_found_message, Echo a_echo)
    {
        const char* argv[s::max_parameters_count] = { nullptr };
        uint32_t argc                             = 0;
        bool callback_found                       = false;

        this->line_buffer[this->line_buffer_index] = 0;

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
             i < this->callbacks_count && false == callback_found && nullptr != argv[0] && this->line_buffer_index > 1;
             i++)
        {
            if (0 == strcmp(argv[0], this->p_callbacks[i].p_name))
            {
                this->p_callbacks[i].function(argv, argc, this->p_callbacks[i].p_user_data);
                callback_found = true;
            }
        }

        this->line_buffer_index = 0;

        if (false == callback_found && argc != 0)
        {
            if (nullptr != a_p_command_not_found_message)
            {
                this->write_string.function(a_p_command_not_found_message,
                                            static_cast<uint32_t>(strlen(a_p_command_not_found_message)),
                                            this->write_string.p_user_data);
            }

            this->write_new_line();
        }

        this->write_string.function(
            a_p_prompt, static_cast<uint32_t>(strlen(a_p_prompt)), this->write_string.p_user_data);
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

    void clear_line()
    {
        this->line_buffer_index = 0;
        memset(this->line_buffer, 0x0u, s::line_buffer_capacity * sizeof(char));

        this->write_character.function('\r', this->write_character.p_user_data);
        this->write_string.function(
            this->line_buffer, s::line_buffer_capacity * sizeof(char), this->write_string.p_user_data);
    }

private:
    Write_character_handler write_character;
    Write_string_handler write_string;
    Read_character_handler read_character;

    New_line_mode_flag new_line_mode_input;
    New_line_mode_flag new_line_mode_output;

    const Callback* p_callbacks;
    uint32_t callbacks_count;

    char line_buffer[s::line_buffer_capacity];
    uint32_t line_buffer_index;
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