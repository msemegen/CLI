/*
 *   Name: main.cpp
 *
 *   Copyright (c) Mateusz Semegen and contributors. All rights reserved.
 *   Licensed under the MIT license. See LICENSE file in the project root for details.
 */

// this
#include "syscalls.hpp"

// std
#include <stdio.h>

// cml
#include <cml/debug/assertion.hpp>
#include <cml/hal/Systick.hpp>
#include <cml/hal/mcu.hpp>
#include <cml/hal/peripherals/GPIO.hpp>
#include <cml/hal/peripherals/USART.hpp>
#include <cml/hal/rcc.hpp>
#include <cml/hal/system_timer.hpp>

// modules
#define CML
#define CLI_ASSERT cml_assert
#define CLI_AUTOCOMPLETION
#define CLI_CAROUSEL
#define CLI_COMMAND_PARAMETERS
#include <CLI\CLI.hpp>

namespace {

using namespace cml::hal;
using namespace cml::hal::peripherals;

void system_timer_update(void*)
{
    system_timer::update();
}

void assert_halt(void*)
{
    mcu::halt();
    while (true)
        ;
}

void assert_print(const char* a_p_file, uint32_t a_line, const char* a_p_expression, void*)
{
    printf("ASSERT failed: %s, LOC: %lu, expression: \"%s\"\n", a_p_file, a_line, a_p_expression);
}

void cli_write_character(char a_character, void* a_p_user_data)
{
    USART* p_usart = static_cast<USART*>(a_p_user_data);
    p_usart->transmit_bytes_polling(&a_character, sizeof(a_character));
}

void cli_write_string(const char* a_p_string, uint32_t a_length, void* a_p_user_data)
{
    USART* p_usart = static_cast<USART*>(a_p_user_data);
    p_usart->transmit_bytes_polling(a_p_string, a_length * sizeof(char));
}

uint32_t cli_read_character(char* a_p_out, uint32_t a_buffer_size, void* a_p_user_data)
{
    USART* p_usart = static_cast<USART*>(a_p_user_data);
    return p_usart->receive_bytes_polling(a_p_out, a_buffer_size).data_length_in_words;
}

#ifdef CLI_COMMAND_PARAMETERS
void cli_callback_test(const char** a_p_argv, uint32_t a_argc, void*)
{
    for (uint32_t i = 0; i < a_argc; i++)
    {
        printf("%s\n", a_p_argv[i]);
    }
}

void cli_callback_test_reverse(const char** a_p_argv, uint32_t a_argc, void*)
{
    for (uint32_t i = 0; i < a_argc; i++)
    {
        printf("%s\n", a_p_argv[a_argc - i - 1]);
    }
}
#else
void cli_callback_test(void*)
{
    puts("test\n");
}

void cli_callback_test_reverse(void*)
{
    puts("test_reverse");
}

#endif

} // namespace

int main()
{
    using namespace cml;
    using namespace cml::debug;
    using namespace cml::hal;
    using namespace cml::hal::peripherals;
    using namespace modules;

    Systick systick;
    GPIO gpio_port_a(GPIO::Id::a);

    assertion::register_halt({ assert_halt, nullptr });
    assertion::register_print({ assert_print, nullptr });

    systick.enable((rcc<mcu>::get_SYSCLK_frequency_Hz() / 1000u) - 1, Systick::Prescaler::_1, 0x9u);
    systick.register_tick_callback({ system_timer_update, nullptr });

    rcc<GPIO>::enable(GPIO::Id::a, false);
    rcc<USART>::enable(USART::Id::_2, rcc<USART>::Clock_source::sysclk, false);

    gpio_port_a.enable();

    GPIO::Alternate_function::Config usart_pin_config = {
        GPIO::Mode::push_pull, GPIO::Pull::up, GPIO::Speed::high, 0x7u
    };

    gpio_port_a.p_alternate_function->enable(2u, usart_pin_config);
    gpio_port_a.p_alternate_function->enable(3u, usart_pin_config);

    USART iostream(USART::Id::_2);
    bool iostream_ready = iostream.enable({ 115200,
                                            rcc<mcu>::get_SYSCLK_frequency_Hz(),
                                            USART::Oversampling::_16,
                                            USART::Stop_bits::_1,
                                            USART::Flow_control_flag::none,
                                            USART::Sampling_method::three_sample_bit,
                                            USART::Mode_flag::tx | USART::Mode_flag::rx },
                                          { USART::Word_length::_8_bit, USART::Parity::none },
                                          0x1u,
                                          10u);

    if (true == iostream_ready)
    {
        initialize_syscalls(&iostream);
        setvbuf(stdout, nullptr, _IONBF, 0);

        const CLI::Callback callbacks[] = { { "test", cli_callback_test, nullptr },
                                            { "test_reverse", cli_callback_test_reverse, nullptr } };

        CLI cli({ cli_write_character, &iostream },
                { cli_write_string, &iostream },
                { cli_read_character, &iostream },
                CLI::New_line_mode_flag::lf,
                CLI::New_line_mode_flag::lf,
                callbacks,
                sizeof(callbacks) / sizeof(callbacks[0]));

        printf("$ ");

        const char* p_prompt                    = "$ ";
        const char* p_command_not_found_message = "> Command not found";
        const uint32_t prompt_length            = strlen(p_prompt);
        const uint32_t command_not_found_length = strlen(p_command_not_found_message);

        while (true)
        {
            cli.update(
                p_prompt, prompt_length, p_command_not_found_message, command_not_found_length, CLI::Echo::enabled);
        }

        return 0;
    }
    else
    {
        while (true)
            ;
    }
}