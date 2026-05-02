#include "input.h"
#include <string.h>
#include "hardware/irq.h"

InputManager *InputManager::m_self = nullptr;

bool InputManager::init(uint32_t debounce_us) {
    m_debounce_us = debounce_us ? debounce_us : 8000;
    m_button_count = 0;

    memset(m_gpio_to_idx, 0xFF, sizeof(m_gpio_to_idx));
    memset((void*)m_state, 0, sizeof(m_state));

    m_q_head = m_q_tail = 0;
    m_lost_events = 0;
    m_self = this;
    return true;
}

bool InputManager::register_button_pin(uint pin, uint8_t button_code) {
    if (pin >= INPUT_MAX_GPIO || button_code == 0) return false;
    if (m_button_count >= INPUT_MAX_BUTTONS) return false;
    if (m_gpio_to_idx[pin] != 0xFF) return false;

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);

    Button b;
    b.pin = pin;
    b.code = button_code;
    b.level = gpio_get(pin) ? 0 : 1;
    b.last_us = now_us();

    uint8_t idx = m_button_count++;
    m_buttons[idx] = b;
    m_gpio_to_idx[pin] = idx;
    m_state[button_code] = b.level;

    if (!m_callback_set) {
        gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &InputManager::gpio_irq_trampoline);
        m_callback_set = true;
    } else {
        gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    }
    return true;
}

void InputManager::gpio_irq_trampoline(uint gpio, uint32_t events) {
    if(m_self) m_self->handle_irq(gpio, events);
}

void InputManager::handle_irq(uint gpio, uint32_t events) {
    if (gpio >= INPUT_MAX_GPIO) return;
    uint8_t idx = m_gpio_to_idx[gpio];
    if (idx == 0xFF) return;

    Button &btn = m_buttons[idx];
    uint32_t t_us = now_us();
    if (t_us - btn.last_us < m_debounce_us) return;

    uint8_t new_level, type;
    if(events & GPIO_IRQ_EDGE_FALL) {
        new_level = 1;
        type = EVENT_PRESS;
    } else if (events & GPIO_IRQ_EDGE_RISE) {
        new_level = 2;
        type = EVENT_RELEASE;
    } else return;

    if (new_level == btn.level) {
        btn.last_us = t_us;
        return;
    }

    btn.level = new_level;
    btn.last_us = t_us;
    m_state[btn.code] = new_level;


    push_isr({btn.code, type, now_ms()});
}

bool InputManager::poll_event(InputEvent &out) {
    uint8_t t = m_q_tail;
    if (t == m_q_head) return false;
    out = m_q[t];
    m_q_tail = static_cast<uint8_t>(t + 1);
    return true;
}

uint8_t InputManager::get_state(uint8_t code) const {
    return m_state[code];
}