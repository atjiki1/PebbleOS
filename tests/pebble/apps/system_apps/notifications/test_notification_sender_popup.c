/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include <clar.h>
#include <popups/notifications/notification_sender_popup.h>

size_t clock_copy_time_string_timestamp(char *buffer, uint8_t size, time_t timestamp) {
  const int minutes = (int)((timestamp / 60) % (24 * 60));
  return (size_t)snprintf(buffer, size, "%d:%02d", minutes / 60, minutes % 60);
}

void test_notification_sender_popup__range_cutoff(void) {
  const time_t now = 10 * 24 * 3600;
  cl_assert_equal_i(notification_sender_popup_range_cutoff(NotificationGroupingRange_All, now), 0);
  cl_assert_equal_i(notification_sender_popup_range_cutoff(NotificationGroupingRange_Never, now),
                    0);
  cl_assert_equal_i(notification_sender_popup_range_cutoff(NotificationGroupingRange_OneDay, now),
                    now - 24 * 3600);
  cl_assert_equal_i(notification_sender_popup_range_cutoff(NotificationGroupingRange_OneWeek, now),
                    now - 7 * 24 * 3600);
  cl_assert_equal_i(notification_sender_popup_range_cutoff(NotificationGroupingRange_OneWeek, 100),
                    0);
}

void test_notification_sender_popup__single_message_has_time(void) {
  char buffer[64];
  const size_t length =
      notification_sender_popup_format_body(buffer, sizeof(buffer), nullptr, 0, 9 * 3600, "Hi");
  cl_assert_equal_s(buffer, "9:00 Hi");
  cl_assert_equal_i(length, strlen("9:00 Hi"));
}

void test_notification_sender_popup__older_messages_come_first(void) {
  const NotificationSenderPopupMessage older[] = {
    {.timestamp = 9 * 3600, .body = "Where are you?"},
    {.timestamp = 9 * 3600 + 60, .body = "Lunch?"},
  };
  char buffer[128];
  notification_sender_popup_format_body(buffer, sizeof(buffer), older, 2, 10 * 3600, "12:00 ok?");
  cl_assert_equal_s(buffer, "9:00 Where are you?\n9:01 Lunch?\n10:00 12:00 ok?");
}

void test_notification_sender_popup__length_without_buffer(void) {
  const NotificationSenderPopupMessage older[] = {
    {.timestamp = 0, .body = "a"},
  };
  const size_t length = notification_sender_popup_format_body(nullptr, 0, older, 1, 60, "b");
  cl_assert_equal_i(length, strlen("0:00 a\n0:01 b"));

  char small[8];
  notification_sender_popup_format_body(small, sizeof(small), older, 1, 60, "b");
  cl_assert_equal_s(small, "0:00 a\n");
}
