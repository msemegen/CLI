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

void cli_write_string(std::string_view a_string, void* a_p_user_data)
{
    WriteConsole(
        GetStdHandle(STD_OUTPUT_HANDLE), a_string.data(), static_cast<DWORD>(a_string.length()), nullptr, nullptr);
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
void cli_callback_test(std::string_view a_argv[], uint32_t a_argc, void*)
{
    for (uint32_t i = 0; i < a_argc; i++)
    {
        WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),
                     a_argv[i].data(),
                     static_cast<DWORD>(a_argv[i].length()),
                     nullptr,
                     nullptr);
        WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\n", 1u, nullptr, nullptr);
    }
}

void cli_callback_test_reverse(std::string_view a_argv[], uint32_t a_argc, void*)
{
    for (uint32_t i = 0; i < a_argc; i++)
    {
        WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),
                     a_argv[a_argc - i - 1].data(),
                     static_cast<DWORD>(a_argv[a_argc - i - 1].length()),
                     nullptr,
                     nullptr);
        WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\n", 1u, nullptr, nullptr);
    }
}

void cli_callback_exit(std::string_view[], uint32_t, void*)
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

    std::array<CLI::Callback, 3> callbacks = { CLI::Callback { "exit", cli_callback_exit, nullptr },
                                               CLI::Callback { "test", cli_callback_test, nullptr },
                                               CLI::Callback { "test_reverse", cli_callback_test_reverse, nullptr } };

    CLI cli({ cli_write_character, nullptr },
            { cli_write_string, nullptr },
            { cli_read_character, nullptr },
            CLI::New_line_mode_flag::cr,
            CLI::New_line_mode_flag::cr | CLI::New_line_mode_flag::lf);

    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "$ ", 2u, nullptr, nullptr);

    while (true)
    {
        cli.update("$ ", "> Command not found", callbacks, CLI::Echo::enabled);
    }

    return 0;
}