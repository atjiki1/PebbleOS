/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/services/timeline/attribute.h>
#include <pbl/services/timeline/timeline.h>

#include <apps/system/notifications_history.h>
#include <clar.h>
#include <fake_pbl_malloc.h>

static NotificationHistory s_history;

bool alerts_preferences_get_notification_dismissal_sync(void) {
  return false;
}

const char *attribute_get_string(const AttributeList *attr_list, AttributeId id,
                                 char *default_value) {
  for (uint8_t i = 0; i < attr_list->num_attributes; i++) {
    if (attr_list->attributes[i].id == id) {
      return attr_list->attributes[i].cstring;
    }
  }
  return default_value;
}

bool timeline_item_is_ancs_notif(const TimelineItem *item) {
  return item->header.ancs_notif;
}

static Uuid prv_id(uint8_t value) {
  return UuidMake(value, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

static void prv_add_ios(uint8_t id, time_t timestamp, const char *app_id, const char *title) {
  Attribute attributes[] = {
    {.id = AttributeIdTitle, .cstring = (char *)title},
    {.id = AttributeIdiOSAppIdentifier, .cstring = (char *)app_id},
  };
  TimelineItem item = {
    .header =
        {
          .id = prv_id(id),
          .timestamp = timestamp,
          .type = TimelineItemTypeNotification,
          .ancs_notif = true,
        },
    .attr_list = {
      .num_attributes = app_id ? 2 : 1,
      .attributes = attributes,
    },
  };
  notifications_history_add_item(&s_history, &item);
}

static NotificationHistoryRow *prv_row(uint16_t index) {
  return notifications_history_get_row(&s_history, index);
}

void test_notifications_history_ios__initialize(void) {
  fake_pbl_malloc_clear_tracking();
  notifications_history_init(&s_history, true, 0);
}

void test_notifications_history_ios__cleanup(void) {
  notifications_history_deinit(&s_history);
  fake_pbl_malloc_check_net_allocs();
  fake_pbl_malloc_clear_tracking();
}

void test_notifications_history_ios__same_app_and_sender_form_one_group(void) {
  prv_add_ios(1, 100, "com.iwilab.KakaoTalk", "Anna");
  prv_add_ios(2, 200, "com.iwilab.KakaoTalk", "Anna");

  cl_assert_equal_i(notifications_history_get_row_count(&s_history), 1);
  cl_assert(prv_row(0)->is_group);
  cl_assert_equal_i(prv_row(0)->group.count, 2);
  cl_assert_equal_s(notifications_history_group_label(prv_row(0)->group.sender), "Anna");
}

void test_notifications_history_ios__same_sender_in_other_apps_stays_apart(void) {
  prv_add_ios(1, 100, "com.iwilab.KakaoTalk", "Anna");
  prv_add_ios(2, 200, "com.apple.MobileSMS", "Anna");
  prv_add_ios(3, 300, "com.iwilab.KakaoTalk", "Anna");

  cl_assert_equal_i(notifications_history_get_row_count(&s_history), 2);
  cl_assert(prv_row(0)->is_group);
  cl_assert_equal_i(prv_row(0)->group.count, 2);
  cl_assert(prv_row(1)->is_group);
  cl_assert_equal_i(prv_row(1)->group.count, 1);
  cl_assert_equal_s(notifications_history_group_label(prv_row(1)->group.sender), "Anna");
}

void test_notifications_history_ios__missing_app_id_groups_by_sender(void) {
  prv_add_ios(1, 100, nullptr, "Anna");
  prv_add_ios(2, 200, nullptr, "Anna");

  cl_assert_equal_i(notifications_history_get_row_count(&s_history), 1);
  cl_assert_equal_s(notifications_history_group_label(prv_row(0)->group.sender), "Anna");
}

void test_notifications_history_ios__removing_last_member_removes_group(void) {
  prv_add_ios(1, 100, "com.iwilab.KakaoTalk", "Anna");
  Uuid id = prv_id(1);

  cl_assert(notifications_history_remove(&s_history, &id));
  cl_assert_equal_i(notifications_history_get_row_count(&s_history), 0);
}

void test_notifications_history_ios__long_titles_match_their_stored_prefix(void) {
  char long_title[ATTRIBUTE_TITLE_MAX_LEN + 20];
  memset(long_title, 'a', sizeof(long_title) - 1);
  long_title[sizeof(long_title) - 1] = '\0';
  char stored_title[ATTRIBUTE_TITLE_MAX_LEN + 1];
  memcpy(stored_title, long_title, ATTRIBUTE_TITLE_MAX_LEN);
  stored_title[ATTRIBUTE_TITLE_MAX_LEN] = '\0';

  prv_add_ios(1, 100, "com.iwilab.KakaoTalk", stored_title);
  prv_add_ios(2, 200, "com.iwilab.KakaoTalk", long_title);

  cl_assert_equal_i(notifications_history_get_row_count(&s_history), 1);
  cl_assert_equal_i(prv_row(0)->group.count, 2);
}
