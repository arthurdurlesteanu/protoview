#pragma once
typedef struct NotificationApp NotificationApp; typedef struct {int x;} NotificationMessage;
typedef const NotificationMessage* NotificationSequence[];
extern const NotificationMessage message_green_255, message_green_0, message_red_255, message_red_0, message_vibro_on, message_vibro_off, message_delay_50, message_display_backlight_on;
static inline void notification_message(NotificationApp* a, const void* s){(void)a;(void)s;}
extern const NotificationSequence sequence_single_vibro;
