/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "notification_sender_popup.h"

#include <stdio.h>
#include <string.h>

#include <pbl/services/clock.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

time_t notification_sender_popup_range_cutoff(NotificationGroupingRange range, time_t now) {
  time_t window = 0;
  if (range == NotificationGroupingRange_OneDay) {
    window = PBL_SEC_PER_DAY;
  } else if (range == NotificationGroupingRange_OneWeek) {
    window = 7 * PBL_SEC_PER_DAY;
  }
  if ((window == 0) || (now <= window)) {
    return 0;
  }
  return now - window;
}

static size_t prv_append_line(char *buffer, size_t size, size_t offset, time_t timestamp,
                              const char *body, bool newline) {
  char time_string[16];
  clock_copy_time_string_timestamp(time_string, sizeof(time_string), timestamp);
  char *out = (offset < size) ? &buffer[offset] : nullptr;
  const size_t out_size = (offset < size) ? (size - offset) : 0;
  const int length = snprintf(out, out_size, "%s%s %s", newline ? "\n" : "", time_string, body);
  return (length > 0) ? (size_t)length : 0;
}

size_t notification_sender_popup_format_body(char *buffer, size_t size,
                                             const NotificationSenderPopupMessage *older,
                                             int num_older, time_t timestamp, const char *body) {
  size_t offset = 0;
  for (int i = 0; i < num_older; i++) {
    offset += prv_append_line(buffer, size, offset, older[i].timestamp, older[i].body, i > 0);
  }
  offset += prv_append_line(buffer, size, offset, timestamp, body, num_older > 0);
  return offset;
}

#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)

#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/services/notifications/notification_storage.h>
#include <pbl/services/timeline/attribute.h>

#include <applib/applib_malloc.auto.h>
#include <apps/system/notifications_history.h>

//! Room for one stored string; ANCS bodies are at most 200 bytes plus an ellipsis
#define STRING_BUFFER_SIZE 256

typedef struct {
  Uuid id;
  time_t timestamp;
  char body[STRING_BUFFER_SIZE];
} OlderMessage;

typedef struct {
  char key[NOTIFICATIONS_HISTORY_GROUP_KEY_MAX_LEN + 1];
  char item_key[NOTIFICATIONS_HISTORY_GROUP_KEY_MAX_LEN + 1];
  Uuid id;
  time_t timestamp;
  int count;
  OlderMessage messages[NOTIFICATION_SENDER_POPUP_MAX_MESSAGES - 1];
  char strings[4][STRING_BUFFER_SIZE];
} Collect;

bool notification_sender_popup_enabled(void) {
  return alerts_preferences_get_notification_popup_grouping_range() !=
         NotificationGroupingRange_Never;
}

static time_t prv_cutoff(void) {
  return notification_sender_popup_range_cutoff(
      alerts_preferences_get_notification_popup_grouping_range(), pbl_realtime());
}

static bool prv_collect_cb(void *data, const CommonTimelineItemHeader *header,
                           const TimelineItem *item) {
  Collect *collect = data;
  if (!item || uuid_equal(&header->id, &collect->id) ||
      (header->status & (TimelineItemStatusActioned | TimelineItemStatusDismissed)) ||
      (header->timestamp > collect->timestamp)) {
    return true;
  }
  const char *key =
      notifications_history_group_key(item, collect->item_key, sizeof(collect->item_key));
  if (!key || (strcmp(key, collect->key) != 0)) {
    return true;
  }

  // Keep the newest ones
  int slot = collect->count;
  if (slot == ARRAY_LENGTH(collect->messages)) {
    slot = 0;
    for (int i = 1; i < collect->count; i++) {
      if (collect->messages[i].timestamp < collect->messages[slot].timestamp) {
        slot = i;
      }
    }
    if (collect->messages[slot].timestamp > header->timestamp) {
      return true;
    }
  } else {
    collect->count++;
  }
  OlderMessage *message = &collect->messages[slot];
  message->id = header->id;
  message->timestamp = header->timestamp;
  strncpy(message->body, attribute_get_string(&item->attr_list, AttributeIdBody, ""),
          sizeof(message->body) - 1);
  message->body[sizeof(message->body) - 1] = '\0';
  return true;
}

//! Finds the earlier messages of the sender of @p item, oldest first
static Collect *prv_collect(const TimelineItem *item) {
  if (!notification_sender_popup_enabled() || (item->header.type != TimelineItemTypeNotification)) {
    return nullptr;
  }
  const time_t cutoff = prv_cutoff();
  if (item->header.timestamp < cutoff) {
    return nullptr;
  }

  Collect *collect = applib_zalloc(sizeof(*collect));
  if (!collect) {
    return nullptr;
  }
  const char *key =
      notifications_history_group_key(item, collect->item_key, sizeof(collect->item_key));
  if (!key) {
    applib_free(collect);
    return nullptr;
  }
  strncpy(collect->key, key, sizeof(collect->key) - 1);
  collect->id = item->header.id;
  collect->timestamp = item->header.timestamp;

  Attribute attributes[] = {
    {.id = AttributeIdSender, .cstring = collect->strings[0]},
    {.id = AttributeIdTitle, .cstring = collect->strings[1]},
    {.id = AttributeIdiOSAppIdentifier, .cstring = collect->strings[2]},
    {.id = AttributeIdBody, .cstring = collect->strings[3]},
  };
  AttributeList attr_list = {
    .num_attributes = ARRAY_LENGTH(attributes),
    .attributes = attributes,
  };
  notification_storage_iterate_strings_after(cutoff, &attr_list, STRING_BUFFER_SIZE, prv_collect_cb,
                                             collect);

  // Oldest first
  for (int i = 1; i < collect->count; i++) {
    OlderMessage tmp = collect->messages[i];
    int j = i - 1;
    while ((j >= 0) && (collect->messages[j].timestamp > tmp.timestamp)) {
      collect->messages[j + 1] = collect->messages[j];
      j--;
    }
    collect->messages[j + 1] = tmp;
  }
  return collect;
}

TimelineItem *notification_sender_popup_merge(TimelineItem *item) {
  Collect *collect = prv_collect(item);
  if (!collect) {
    return item;
  }
  if (collect->count == 0) {
    applib_free(collect);
    return item;
  }

  NotificationSenderPopupMessage older[ARRAY_LENGTH(collect->messages)];
  for (int i = 0; i < collect->count; i++) {
    older[i] = (NotificationSenderPopupMessage){
      .timestamp = collect->messages[i].timestamp,
      .body = collect->messages[i].body,
    };
  }
  const char *body = attribute_get_string(&item->attr_list, AttributeIdBody, "");
  const size_t length = notification_sender_popup_format_body(nullptr, 0, older, collect->count,
                                                              item->header.timestamp, body);
  char *merged = applib_malloc(length + 1);

  const uint8_t num_attributes = item->attr_list.num_attributes;
  Attribute *attributes = applib_malloc(sizeof(Attribute) * (num_attributes + 1));
  TimelineItem *merged_item = nullptr;
  if (merged && attributes) {
    notification_sender_popup_format_body(merged, length + 1, older, collect->count,
                                          item->header.timestamp, body);
    memcpy(attributes, item->attr_list.attributes, sizeof(Attribute) * num_attributes);
    AttributeList attr_list = {
      .num_attributes = num_attributes,
      .attributes = attributes,
    };
    Attribute *body_attribute = attribute_find(&attr_list, AttributeIdBody);
    if (!body_attribute) {
      body_attribute = &attributes[attr_list.num_attributes++];
      body_attribute->id = AttributeIdBody;
    }
    body_attribute->cstring = merged;

    merged_item = timeline_item_create_with_attributes(
        item->header.timestamp, item->header.duration, item->header.type, item->header.layout,
        &attr_list, &item->action_group);
  }
  applib_free(attributes);
  applib_free(merged);
  applib_free(collect);

  if (!merged_item) {
    PBL_LOG_WRN("Could not merge the sender's notifications");
    return item;
  }
  merged_item->header = item->header;
  timeline_item_destroy(item);
  return merged_item;
}

int notification_sender_popup_older_ids(const TimelineItem *item, Uuid *ids, int max_ids) {
  Collect *collect = prv_collect(item);
  if (!collect) {
    return 0;
  }
  const int count = MIN(collect->count, max_ids);
  for (int i = 0; i < count; i++) {
    ids[i] = collect->messages[i].id;
  }
  applib_free(collect);
  return count;
}

bool notification_sender_popup_same_sender(const Uuid *a, const Uuid *b) {
  if (!notification_sender_popup_enabled()) {
    return false;
  }
  const time_t cutoff = prv_cutoff();
  char *keys = applib_zalloc(2 * (NOTIFICATIONS_HISTORY_GROUP_KEY_MAX_LEN + 1));
  if (!keys) {
    return false;
  }
  const char *found[2] = {};
  const Uuid *ids[2] = {a, b};
  for (int i = 0; i < 2; i++) {
    TimelineItem item;
    if (!notification_storage_get(ids[i], &item)) {
      break;
    }
    if ((item.header.type == TimelineItemTypeNotification) && (item.header.timestamp >= cutoff)) {
      char *buffer = &keys[i * (NOTIFICATIONS_HISTORY_GROUP_KEY_MAX_LEN + 1)];
      found[i] = notifications_history_group_key(&item, buffer,
                                                 NOTIFICATIONS_HISTORY_GROUP_KEY_MAX_LEN + 1);
    }
    timeline_item_free_allocated_buffer(&item);
  }
  const bool same = found[0] && found[1] && (strcmp(found[0], found[1]) == 0);
  applib_free(keys);
  return same;
}

#endif
