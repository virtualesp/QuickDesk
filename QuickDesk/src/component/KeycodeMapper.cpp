// Copyright 2026 QuickDesk Authors

#include "KeycodeMapper.h"
#include <Qt>

namespace quickdesk {

KeycodeMapper* KeycodeMapper::s_instance = nullptr;

KeycodeMapper::KeycodeMapper(QObject* parent)
    : QObject(parent)
{
    initKeyMap();
    s_instance = this;
}

KeycodeMapper* KeycodeMapper::instance()
{
    if (!s_instance) {
        s_instance = new KeycodeMapper();
    }
    return s_instance;
}

int KeycodeMapper::qtKeyToUsb(int qtKey, int modifiers) const
{
    Q_UNUSED(modifiers);
    
    // USB HID Usage Page for keyboard is 0x07
    constexpr int USB_PAGE_KEYBOARD = 0x00070000;
    
    if (m_keyMap.contains(qtKey)) {
        return USB_PAGE_KEYBOARD | m_keyMap.value(qtKey);
    }
    
    return 0;
}

void KeycodeMapper::initKeyMap()
{
    // USB HID Usage IDs for keyboard (from USB HID Usage Tables)
    // https://usb.org/sites/default/files/hut1_4.pdf
    
    // Letters A-Z (Usage IDs 0x04-0x1D)
    m_keyMap[Qt::Key_A] = 0x04;
    m_keyMap[Qt::Key_B] = 0x05;
    m_keyMap[Qt::Key_C] = 0x06;
    m_keyMap[Qt::Key_D] = 0x07;
    m_keyMap[Qt::Key_E] = 0x08;
    m_keyMap[Qt::Key_F] = 0x09;
    m_keyMap[Qt::Key_G] = 0x0A;
    m_keyMap[Qt::Key_H] = 0x0B;
    m_keyMap[Qt::Key_I] = 0x0C;
    m_keyMap[Qt::Key_J] = 0x0D;
    m_keyMap[Qt::Key_K] = 0x0E;
    m_keyMap[Qt::Key_L] = 0x0F;
    m_keyMap[Qt::Key_M] = 0x10;
    m_keyMap[Qt::Key_N] = 0x11;
    m_keyMap[Qt::Key_O] = 0x12;
    m_keyMap[Qt::Key_P] = 0x13;
    m_keyMap[Qt::Key_Q] = 0x14;
    m_keyMap[Qt::Key_R] = 0x15;
    m_keyMap[Qt::Key_S] = 0x16;
    m_keyMap[Qt::Key_T] = 0x17;
    m_keyMap[Qt::Key_U] = 0x18;
    m_keyMap[Qt::Key_V] = 0x19;
    m_keyMap[Qt::Key_W] = 0x1A;
    m_keyMap[Qt::Key_X] = 0x1B;
    m_keyMap[Qt::Key_Y] = 0x1C;
    m_keyMap[Qt::Key_Z] = 0x1D;
    
    // Numbers 1-0 (Usage IDs 0x1E-0x27)
    m_keyMap[Qt::Key_1] = 0x1E;
    m_keyMap[Qt::Key_2] = 0x1F;
    m_keyMap[Qt::Key_3] = 0x20;
    m_keyMap[Qt::Key_4] = 0x21;
    m_keyMap[Qt::Key_5] = 0x22;
    m_keyMap[Qt::Key_6] = 0x23;
    m_keyMap[Qt::Key_7] = 0x24;
    m_keyMap[Qt::Key_8] = 0x25;
    m_keyMap[Qt::Key_9] = 0x26;
    m_keyMap[Qt::Key_0] = 0x27;
    
    // Special keys
    m_keyMap[Qt::Key_Return] = 0x28;     // Enter
    m_keyMap[Qt::Key_Enter] = 0x28;      // Keypad Enter (same as Return)
    m_keyMap[Qt::Key_Escape] = 0x29;
    m_keyMap[Qt::Key_Backspace] = 0x2A;
    m_keyMap[Qt::Key_Tab] = 0x2B;
    m_keyMap[Qt::Key_Space] = 0x2C;
    m_keyMap[Qt::Key_Minus] = 0x2D;      // -
    m_keyMap[Qt::Key_Equal] = 0x2E;      // =
    m_keyMap[Qt::Key_BracketLeft] = 0x2F;  // [
    m_keyMap[Qt::Key_BracketRight] = 0x30; // ]
    m_keyMap[Qt::Key_Backslash] = 0x31;  // \ (backslash)
    m_keyMap[Qt::Key_Semicolon] = 0x33;  // ;
    m_keyMap[Qt::Key_Apostrophe] = 0x34; // '
    m_keyMap[Qt::Key_QuoteLeft] = 0x35;  // ` (grave accent)
    m_keyMap[Qt::Key_Comma] = 0x36;      // ,
    m_keyMap[Qt::Key_Period] = 0x37;     // .
    m_keyMap[Qt::Key_Slash] = 0x38;      // /
    m_keyMap[Qt::Key_CapsLock] = 0x39;
    
    // Function keys F1-F12 (Usage IDs 0x3A-0x45)
    m_keyMap[Qt::Key_F1] = 0x3A;
    m_keyMap[Qt::Key_F2] = 0x3B;
    m_keyMap[Qt::Key_F3] = 0x3C;
    m_keyMap[Qt::Key_F4] = 0x3D;
    m_keyMap[Qt::Key_F5] = 0x3E;
    m_keyMap[Qt::Key_F6] = 0x3F;
    m_keyMap[Qt::Key_F7] = 0x40;
    m_keyMap[Qt::Key_F8] = 0x41;
    m_keyMap[Qt::Key_F9] = 0x42;
    m_keyMap[Qt::Key_F10] = 0x43;
    m_keyMap[Qt::Key_F11] = 0x44;
    m_keyMap[Qt::Key_F12] = 0x45;
    
    // Print Screen, Scroll Lock, Pause
    m_keyMap[Qt::Key_Print] = 0x46;
    m_keyMap[Qt::Key_ScrollLock] = 0x47;
    m_keyMap[Qt::Key_Pause] = 0x48;
    
    // Insert, Home, Page Up, Delete, End, Page Down
    m_keyMap[Qt::Key_Insert] = 0x49;
    m_keyMap[Qt::Key_Home] = 0x4A;
    m_keyMap[Qt::Key_PageUp] = 0x4B;
    m_keyMap[Qt::Key_Delete] = 0x4C;
    m_keyMap[Qt::Key_End] = 0x4D;
    m_keyMap[Qt::Key_PageDown] = 0x4E;
    
    // Arrow keys
    m_keyMap[Qt::Key_Right] = 0x4F;
    m_keyMap[Qt::Key_Left] = 0x50;
    m_keyMap[Qt::Key_Down] = 0x51;
    m_keyMap[Qt::Key_Up] = 0x52;
    
    // Num Lock and keypad
    m_keyMap[Qt::Key_NumLock] = 0x53;
    // Note: Keypad keys have separate Qt::Key values with Key_* prefix
    // but they map to the same USB HID codes when NumLock is on
    
    // Modifier keys (left side)
    m_keyMap[Qt::Key_Control] = 0xE0;    // Left Control
    m_keyMap[Qt::Key_Shift] = 0xE1;      // Left Shift
    m_keyMap[Qt::Key_Alt] = 0xE2;        // Left Alt
    m_keyMap[Qt::Key_Meta] = 0xE3;       // Left GUI (Windows key)
    
    // Additional common keys
    m_keyMap[Qt::Key_Menu] = 0x65;       // Application/Context Menu
    
    // Multimedia keys (Consumer Page 0x0C, but we use keyboard page for simplicity)
    // These might need special handling in practice
    m_keyMap[Qt::Key_VolumeUp] = 0x80;
    m_keyMap[Qt::Key_VolumeDown] = 0x81;
    m_keyMap[Qt::Key_VolumeMute] = 0x7F;
}

} // namespace quickdesk
