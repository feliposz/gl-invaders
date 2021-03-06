#pragma once

enum EventType
{
    EVENT_TYPE_NONE,
    EVENT_TYPE_QUIT,
    EVENT_TYPE_KEYBOARD,
};

enum KeyCode
{
    KEY_NONE,
    KEY_ARROW_LEFT,
    KEY_ARROW_RIGHT,
    KEY_ARROW_DOWN,
    KEY_ARROW_UP,
    KEY_SHIFT,
    KEY_ESCAPE,
};

struct Event
{
    EventType type;
    KeyCode key_code;
    bool key_pressed;
};

// Platform functions
bool create_window(int width, int height);
void window_clear(float r, float g, float b, float a);
void swap_buffers();
bool update_window_events();
void do_sleep(int ms);
double get_time();
bool get_next_event(Event *event);

// game entry point
int invaders();
