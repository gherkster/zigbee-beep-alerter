# Zigbee Beep Alerter

A beep alerter using Zigbee and an ESP32-C6. When a sound pattern is detected at a certain threshold volume and sequence, a zigbee binary sensor value is toggled.

This can be used with Home Assistant to notify when a certain appliance beep is emitted. It is currently used to detect washing machine done beeps.

The microphone used in this project is the MAX9814, other modules will likely also work.
