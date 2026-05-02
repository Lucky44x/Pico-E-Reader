#ifndef INPUTH
#define INPUTH

#include "pico/stdlib.h"
#include "stdio.h"
#include <stdlib.h>
#include "hardware/gpio.h"

#define INPUT_MAX_GPIO 30
#define INPUT_MAX_BUTTONS 16

struct InputEvent {
    uint8_t code;
    uint8_t type;
    uint32_t ts_ms;
};

class InputManager {
    public:
        static constexpr uint8_t EVENT_PRESS = 1;
        static constexpr uint8_t EVENT_RELEASE = 2;

        bool init(uint32_t debounce_us = 8000);
        bool register_button_pin(uint pin, uint8_t button_code);

        bool poll_event(InputEvent &out);
        uint8_t get_state(uint8_t code) const;
        uint8_t lost_events();
    private:
        struct Button {
            uint16_t pin = 0xFFFF;
            uint8_t code = 0;
            uint8_t level = 0;
            uint32_t last_us = 0;
        };

        static constexpr uint8_t QUEUE_CAP = 64;
        uint8_t m_button_count;
        Button m_buttons[INPUT_MAX_BUTTONS];
        uint8_t m_gpio_to_idx[INPUT_MAX_GPIO];
        volatile uint8_t m_state[INPUT_MAX_BUTTONS];

        InputEvent m_q[QUEUE_CAP];
        volatile uint8_t m_q_head = 0, m_q_tail = 0;
        volatile uint32_t m_lost_events = 0;

        uint32_t m_debounce_us = 8000;
        bool m_callback_set = false;

        static InputManager* m_self;

        static inline uint32_t now_us() {return to_us_since_boot(get_absolute_time());}
        static inline uint32_t now_ms() {return to_ms_since_boot(get_absolute_time());}

        inline void push_isr(const InputEvent &e) {
            uint8_t h = m_q_head;
            uint8_t n = static_cast<uint8_t>(h + 1);

            if (n == m_q_tail) { ++m_lost_events; return; }
            m_q[h] = e;
            m_q_head = n;
        };

        static void gpio_irq_trampoline(uint gpio, uint32_t events);
        void handle_irq(uint gpio, uint32_t events);
};

#endif