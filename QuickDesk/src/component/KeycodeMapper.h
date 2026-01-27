// Copyright 2026 QuickDesk Authors
// Qt Key to USB HID Keycode Mapper

#ifndef QUICKDESK_COMPONENT_KEYCODEMAPPER_H
#define QUICKDESK_COMPONENT_KEYCODEMAPPER_H

#include <QObject>
#include <QMap>

namespace quickdesk {

/**
 * @brief Converts Qt key codes to USB HID keycodes
 * 
 * USB HID keycode format:
 * - Upper 16 bits: Usage Page (0x0007 for keyboard)
 * - Lower 16 bits: Usage ID
 * 
 * Usage as QML singleton:
 *   KeycodeMapper.qtKeyToUsb(event.key, event.modifiers)
 */
class KeycodeMapper : public QObject {
    Q_OBJECT

public:
    explicit KeycodeMapper(QObject* parent = nullptr);
    ~KeycodeMapper() override = default;

    /**
     * @brief Convert Qt key code to USB HID keycode
     * @param qtKey Qt::Key value
     * @param modifiers Qt::KeyboardModifiers
     * @return USB HID keycode (0x0007XXXX), or 0 if not mapped
     */
    Q_INVOKABLE int qtKeyToUsb(int qtKey, int modifiers) const;

    /**
     * @brief Get singleton instance
     */
    static KeycodeMapper* instance();

private:
    void initKeyMap();
    
    // Qt Key -> USB Usage ID mapping
    QMap<int, int> m_keyMap;
    
    static KeycodeMapper* s_instance;
};

} // namespace quickdesk

#endif // QUICKDESK_COMPONENT_KEYCODEMAPPER_H
