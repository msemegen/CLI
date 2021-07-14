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
        static constexpr char new_line_character       = '\n';
    };

    enum class Status
    {
        ok,
        input_error,
        output_error
    };

    enum class Echo
    {
        disabled,
        enabled
    };

    struct Write_character_handler
    {
        using Function = bool (*)(char a_character, void* a_p_user_data);

        Function function = nullptr;
        void* p_user_data = nullptr;
    };

    struct Write_string_handler
    {
        using Function = uint32_t (*)(const char* a_p_string, uint32_t a_length, void* a_p_user_data);

        Function function = nullptr;
        void* p_user_data = nullptr;
    };

    struct Read_character_handler
    {
        using Function = bool (*)(char* a_p_out, void* a_p_user_data);

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
        const Callback* a_p_callbacks,
        uint32_t a_callbacks_count)
        : write_character(a_write_character)
        , write_string(a_write_string)
        , read_character(a_read_character)
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

    Status update(const char* a_p_prompt, const char* a_p_command_not_found_message, Echo a_echo)
    {
#ifdef CLI_ASSERT
#ifdef CML
        cml_assert(nullptr != a_p_prompt);
#else
        assert(nullptr != a_p_prompt);
#endif
#endif
        char c                                    = 0;
        const char* argv[s::max_parameters_count] = { nullptr };
        uint32_t argc                             = 0;
        bool callback_found                       = false;

        if (true == this->read_character.function(&c, this->read_character.p_user_data))
        {
            switch (c)
            {
                case s::new_line_character: {
                    this->line_buffer[this->line_buffer_index] = 0;

                    if (Echo::enabled == a_echo)
                    {
                        if (false == this->write_character.function('\n', this->write_character.p_user_data))
                        {
                            this->line_buffer_index = 0;
                            return Status::output_error;
                        }
                    }

                    argv[argc] = strtok(this->line_buffer, " ");
                    while (nullptr != argv[argc] && argc < s::max_parameters_count)
                    {
                        argv[++argc] = strtok(nullptr, " ");
                    }

                    for (uint32_t i = 0; i < this->callbacks_count && false == callback_found &&
                                         nullptr != argv[0] && this->line_buffer_index > 1;
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
                            if (false == this->write_string.function(
                                             a_p_command_not_found_message,
                                             static_cast<uint32_t>(strlen(a_p_command_not_found_message)),
                                             this->write_string.p_user_data))
                            {
                                return Status::output_error;
                            }
                        }

                        if (false == this->write_character.function('\n', this->write_character.p_user_data))
                        {
                            return Status::output_error;
                        }
                    }

                    if (false == this->write_string.function(a_p_prompt,
                                                             static_cast<uint32_t>(strlen(a_p_prompt)),
                                                             this->write_string.p_user_data))
                    {
                        return Status::output_error;
                    }
                }
                break;

                case '\b': {
                    if (this->line_buffer_index > 0)
                    {
                        this->line_buffer_index--;
                        if (false ==
                            this->write_string.function("\b \b", 3 * sizeof(char), this->write_string.p_user_data))
                        {
                            return Status::output_error;
                        }
                    }
                }
                break;

                default: {
                    if (this->line_buffer_index < s::line_buffer_capacity)
                    {
                        this->line_buffer[this->line_buffer_index++] = c;

                        if (Echo::enabled == a_echo)
                        {
                            if (false == this->write_character.function(c, this->write_character.p_user_data))
                            {
                                return Status::output_error;
                            }
                        }
                    }
                }
                break;
            }

            return Status::ok;
        }

        return Status::input_error;
    }

private:
#ifndef CML
    CLI(const CLI&) = delete;
    CLI& operator=(const CLI&) = delete;

    CLI()      = default;
    CLI(CLI&&) = default;

    CLI& operator=(CLI&&) = default;
#endif

private:
    Write_character_handler write_character;
    Write_string_handler write_string;
    Read_character_handler read_character;

    const Callback* p_callbacks;
    uint32_t callbacks_count;

    char line_buffer[s::line_buffer_capacity];
    uint32_t line_buffer_index;
};

} // namespace modules