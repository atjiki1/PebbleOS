# SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_notifications_history
  SOURCES
    pebble/apps/system/notifications_history.c
    lib/util/string.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_notifications_history_ios
  SOURCES
    pebble/apps/system/notifications_history.c
    lib/util/string.c
  DEFINES CONFIG_BT_MULTI_LINK_EXPERIMENT=1
  OVERRIDES dummy_board
)

pbl_clar_test(test_notification_sender_popup
  SOURCES
    pebble/popups/notifications/notification_sender_popup.c
  OVERRIDES dummy_board
)
