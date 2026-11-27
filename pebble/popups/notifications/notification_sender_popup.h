/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include <pbl/services/notifications/alerts_preferences_private.h>
#include <pbl/services/timeline/item.h>

//! Most messages shown on one sender card, the new one included
#define NOTIFICATION_SENDER_POPUP_MAX_MESSAGES 10

//! Oldest timestamp a range covers at @p now; 0 for All
time_t notification_sender_popup_range_cutoff(NotificationGroupingRange range, time_t now);

//! One earlier message of the sender
typedef struct {
  time_t timestamp;
  const char *body;
} NotificationSenderPopupMessage;

//! Writes "<time> <body>" lines, oldest first, ending with the newest message.
//! @p older is sorted oldest first. Returns the length needed, like snprintf().
size_t notification_sender_popup_format_body(char *buffer, size_t size,
                                             const NotificationSenderPopupMessage *older,
                                             int num_older, time_t timestamp, const char *body);

#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
//! Whether the popup shows earlier messages of the sender
bool notification_sender_popup_enabled(void);

//! Replaces @p item with a copy whose body also holds the sender's earlier messages, or returns
//! @p item when there are none. The returned item is freed with timeline_item_destroy().
TimelineItem *notification_sender_popup_merge(TimelineItem *item);

//! Writes the IDs of the earlier messages that a card for @p item shows; returns their number.
int notification_sender_popup_older_ids(const TimelineItem *item, Uuid *ids, int max_ids);

//! Whether notifications @p a and @p b are from the same sender within the popup range
bool notification_sender_popup_same_sender(const Uuid *a, const Uuid *b);
#endif
