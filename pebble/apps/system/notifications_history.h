/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/services/notifications/alerts_preferences_private.h>
#include <pbl/services/timeline/item.h>
#include <pbl/util/list.h>

//! Separates the app from the sender in an iOS group key
#define NOTIFICATIONS_HISTORY_GROUP_KEY_SEPARATOR '\x1f'
#define NOTIFICATIONS_HISTORY_GROUP_KEY_MAX_LEN   (2 * ATTRIBUTE_TITLE_MAX_LEN + 1)

typedef struct NotificationHistoryEntry {
  Uuid id;
  time_t timestamp;
  uint32_t sequence;
} NotificationHistoryEntry;

typedef struct NotificationHistoryMember {
  ListNode node;
  NotificationHistoryEntry entry;
} NotificationHistoryMember;

typedef struct NotificationHistoryRow {
  ListNode node;
  bool is_group;
  union {
    NotificationHistoryEntry notification;
    struct {
      char *sender;
      NotificationHistoryMember *members;
      uint16_t count;
    } group;
  };
} NotificationHistoryRow;

typedef struct NotificationHistory {
  NotificationHistoryRow *rows;
  bool group_by_sender;
  time_t grouping_cutoff;
  uint32_t next_sequence;
} NotificationHistory;

static inline bool notifications_history_is_hidden(const CommonTimelineItemHeader *header) {
  return alerts_preferences_get_notification_dismissal_sync() &&
         header->status & (TimelineItemStatusActioned | TimelineItemStatusDismissed);
}

void notifications_history_init(NotificationHistory *history, bool group_by_sender,
                                time_t grouping_cutoff);
void notifications_history_deinit(NotificationHistory *history);

void notifications_history_add_header(NotificationHistory *history,
                                      const CommonTimelineItemHeader *header);
void notifications_history_add_item(NotificationHistory *history, const TimelineItem *item);
bool notifications_history_remove(NotificationHistory *history, const Uuid *id);

uint16_t notifications_history_get_row_count(const NotificationHistory *history);
NotificationHistoryRow *notifications_history_get_row(const NotificationHistory *history,
                                                      uint16_t index);
bool notifications_history_has_collapsed_groups(const NotificationHistory *history);
bool notifications_history_row_is_collapsed_group(const NotificationHistoryRow *row);
const Uuid *notifications_history_row_get_latest_id(const NotificationHistoryRow *row);

//! Writes the sender key of @p item; returns it (inside @p buffer) or NULL if it is not grouped
const char *notifications_history_group_key(const TimelineItem *item, char *buffer,
                                            size_t buffer_size);

//! The sender to show for a group's sender key
const char *notifications_history_group_label(const char *sender);
