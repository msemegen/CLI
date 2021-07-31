/*
 *   Name: main.cpp
 *
 *   Copyright (c) Mateusz Semegen and contributors. All rights reserved.
 *   Licensed under the MIT license. See LICENSE file in the project root for details.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <assert.h>

#define CLI_ASSERT assert
#define CLI_AUTOCOMPLETION
#define CLI_CAROUSEL
#define CLI_COMMAND_PARAMETERS
#include <CLI\CLI.hpp>

void cli_write_character(char a_character, void* a_p_user_data)
{
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), &a_character, 1u, nullptr, nullptr);
}

void cli_write_string(const char* a_p_string, uint32_t a_length, void* a_p_user_data)
{
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), a_p_string, static_cast<DWORD>(strlen(a_p_string)), nullptr, nullptr);
}

uint32_t cli_read_character(char* a_p_out, uint32_t a_buffer_size, void* a_p_user_data)
{
    using namespace modules;

    INPUT_RECORD input_buffer[CLI::s::input_buffer_capacity] = { 0 };
    DWORD items_read                                         = 0;

    ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), input_buffer, CLI::s::input_buffer_capacity, &items_read);

    uint32_t r = 0;
    for (DWORD i = 0; i < items_read; i++)
    {
        if (KEY_EVENT == input_buffer[i].EventType && TRUE == input_buffer[i].Event.KeyEvent.bKeyDown)
        {
            a_p_out[r++] = input_buffer[i].Event.KeyEvent.uChar.AsciiChar;
        }
    }

    return r;
}

#ifdef CLI_COMMAND_PARAMETERS
void cli_callback_test(const char** a_p_argv, uint32_t a_argc, void*)
{
    for (uint32_t i = 0; i < a_argc; i++)
    {
        WriteConsole(
            GetStdHandle(STD_OUTPUT_HANDLE), a_p_argv[i], static_cast<DWORD>(strlen(a_p_argv[i])), nullptr, nullptr);
        WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\n", 1u, nullptr, nullptr);
    }
}

void cli_callback_test_reverse(const char** a_p_argv, uint32_t a_argc, void*)
{
    for (uint32_t i = 0; i < a_argc; i++)
    {
        WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),
                     a_p_argv[a_argc - i - 1],
                     static_cast<DWORD>(strlen(a_p_argv[a_argc - i - 1])),
                     nullptr,
                     nullptr);
        WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\n", 1u, nullptr, nullptr);
    }
}

void cli_callback_exit(const char**, uint32_t, void*)
{
    exit(0);
}
#else
void cli_callback_test(void*)
{
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "test\n", 5u, nullptr, nullptr);
}

void cli_callback_test_reverse(void*)
{
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "test_reverse\n", 13u, nullptr, nullptr);
}

void cli_callback_exit(void*)
{
    exit(0);
}
#endif

int main()
{
    using namespace modules;

    const CLI::Callback callbacks[] = { { "exit", cli_callback_exit, nullptr },
                                        { "test", cli_callback_test, nullptr },
                                        { "test_reverse", cli_callback_test_reverse, nullptr } };

    CLI cli({ cli_write_character, nullptr },
            { cli_write_string, nullptr },
            { cli_read_character, nullptr },
            CLI::New_line_mode_flag::cr,
            CLI::New_line_mode_flag::cr | CLI::New_line_mode_flag::lf,
            callbacks,
            sizeof(callbacks) / sizeof(callbacks[0]));

    const char* p_prompt                      = "$ ";
    uint32_t prompt_length                    = static_cast<uint32_t>(strlen(p_prompt));
    const char* p_command_not_found_message   = "> Command not found";
    uint32_t command_not_found_message_lenght = static_cast<uint32_t>(strlen(p_command_not_found_message));

    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), p_prompt, static_cast<DWORD>(strlen(p_prompt)), nullptr, nullptr);

    while (true)
    {
        cli.update(
            p_prompt, prompt_length, p_command_not_found_message, command_not_found_message_lenght, CLI::Echo::enabled);
    }

    return 0;
}