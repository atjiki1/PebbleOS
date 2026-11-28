/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "launcher.h"

#include "menu_layer.h"

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/time.h>

#include <applib/app.h>
#include <applib/app_focus_service.h>
#include <applib/app_malloc.h>
#include <applib/event_service_client.h>
#include <applib/ui/app_window_stack.h>
#include <kernel/events.h>
#include <process_state/app_state/app_state.h>
#include <shell/normal/app_idle_timeout.h>
#include <shell/prefs.h>

#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
#include <pbl/i18n/i18n.h>

#include <process_management/pebble_process_info.h>
#include <apps/system_app_ids.h>
#include <resource/resource.h>
#include <resource/resource_ids.auto.h>

//! The Apps folder: installed apps, by name
typedef struct LauncherFolderWindowData {
  Window window;
  LauncherMenuLayer launcher_menu_layer;
  bool is_open;
} LauncherFolderWindowData;
#endif

typedef struct LauncherAppWindowData {
  Window window;
  LauncherMenuLayer launcher_menu_layer;
  AppMenuDataSource app_menu_data_source;
  EventServiceInfo pref_change_event_info;
#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
  AppMenuDataSource folder_data_source;
  AppMenuNode folder_node;
  //! List positions of the folder's apps in name order
  uint16_t *folder_order;
  uint16_t folder_order_count;
  bool folder_order_valid;
  LauncherFolderWindowData *folder;
#endif
} LauncherAppWindowData;

typedef struct LauncherAppPersistedData {
  bool valid;
  uint64_t leave_time;
  LauncherMenuLayerSelectionState selection_state;
  LauncherDrawState draw_state;
} LauncherAppPersistedData;

static LauncherAppPersistedData s_launcher_app_persisted_data;

#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
typedef struct LauncherFolderPersistedData {
  //! The folder was open when an app was launched from it
  bool open;
  LauncherMenuLayerSelectionState selection_state;
} LauncherFolderPersistedData;

static LauncherFolderPersistedData s_launcher_folder_persisted_data;
#endif

/////////////////////////////
// AppFocusService handlers

static void prv_did_focus(bool in_focus) {
  LauncherAppWindowData *data = app_state_get_user_data();
  if (in_focus) {
    launcher_menu_layer_set_selection_animations_enabled(&data->launcher_menu_layer, true);
#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
    if (data->folder && data->folder->is_open) {
      launcher_menu_layer_set_selection_animations_enabled(&data->folder->launcher_menu_layer,
                                                           true);
    }
#endif
  }
}

static void prv_will_focus(bool in_focus) {
  LauncherAppWindowData *data = app_state_get_user_data();
  if (!in_focus) {
    launcher_menu_layer_set_selection_animations_enabled(&data->launcher_menu_layer, false);
#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
    if (data->folder && data->folder->is_open) {
      launcher_menu_layer_set_selection_animations_enabled(&data->folder->launcher_menu_layer,
                                                           false);
    }
#endif
  }
}

static void prv_pref_change_handler([[maybe_unused]] PebbleEvent *event, void *context) {
  LauncherAppWindowData *data = context;
  launcher_menu_layer_update_content_size(&data->launcher_menu_layer);
#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
  if (data->folder && data->folder->is_open) {
    launcher_menu_layer_update_content_size(&data->folder->launcher_menu_layer);
  }
#endif
}

////////////////////////////////
// AppMenuDataSource callbacks

static bool prv_app_filter_callback([[maybe_unused]] AppMenuDataSource *source,
                                    AppInstallEntry *entry) {
  // Skip watchfaces and hidden apps
  return (!app_install_entry_is_watchface(entry) && !app_install_entry_is_hidden((entry))
#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
          // Installed apps live in the Apps folder
          && !app_install_id_from_app_db(entry->install_id)
#endif
  );
}

#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
static bool prv_folder_filter_callback([[maybe_unused]] AppMenuDataSource *source,
                                       AppInstallEntry *entry) {
  return (!app_install_entry_is_watchface(entry) && !app_install_entry_is_hidden(entry) &&
          app_install_id_from_app_db(entry->install_id));
}

//! Case-insensitive for ASCII; other bytes compare as they are, so Hangul sorts after Latin
static int prv_compare_names(const char *a, const char *b) {
  while (*a && *b) {
    unsigned char ca = (unsigned char)*a++;
    unsigned char cb = (unsigned char)*b++;
    if ((ca >= 'A') && (ca <= 'Z')) {
      ca += 'a' - 'A';
    }
    if ((cb >= 'A') && (cb <= 'Z')) {
      cb += 'a' - 'A';
    }
    if (ca != cb) {
      return ca - cb;
    }
  }
  return (unsigned char)*a - (unsigned char)*b;
}

static void prv_folder_build_order(LauncherAppWindowData *data) {
  AppMenuDataSource *source = &data->folder_data_source;
  const uint16_t count = app_menu_data_source_get_count(source);
  app_free(data->folder_order);
  data->folder_order = count ? app_malloc(count * sizeof(uint16_t)) : nullptr;
  data->folder_order_count = data->folder_order ? count : 0;
  data->folder_order_valid = true;

  AppMenuNode **nodes =
      data->folder_order_count ? app_malloc(data->folder_order_count * sizeof(*nodes)) : nullptr;
  if (!nodes) {
    data->folder_order_count = 0;
    return;
  }
  AppMenuNode *node = source->list;
  for (uint16_t i = 0; (i < data->folder_order_count) && node; i++) {
    nodes[i] = node;
    data->folder_order[i] = i;
    node = (AppMenuNode *)list_get_next((ListNode *)node);
  }
  // Insertion sort by name; folders hold few apps
  for (uint16_t i = 1; i < data->folder_order_count; i++) {
    const uint16_t position = data->folder_order[i];
    int j = i - 1;
    while ((j >= 0) && (prv_compare_names(nodes[data->folder_order[j]]->name,
                                          nodes[position]->name) > 0)) {
      data->folder_order[j + 1] = data->folder_order[j];
      j--;
    }
    data->folder_order[j + 1] = position;
  }
  app_free(nodes);
}

static uint16_t prv_folder_transform_index([[maybe_unused]] AppMenuDataSource *source,
                                           uint16_t index, void *context) {
  LauncherAppWindowData *data = context;
  if (!data->folder_order_valid) {
    prv_folder_build_order(data);
  }
  return (index < data->folder_order_count) ? data->folder_order[index] : index;
}

static void prv_folder_open(void *context);

static void prv_folder_update_row(LauncherAppWindowData *data) {
  const bool has_apps = (app_menu_data_source_get_count(&data->folder_data_source) > 0);
  launcher_menu_layer_set_folder_row(&data->launcher_menu_layer,
                                     has_apps ? &data->folder_node : nullptr, APP_ID_SETTINGS,
                                     prv_folder_open, data);
}

static void prv_folder_data_changed(void *context) {
  LauncherAppWindowData *data = context;
  data->folder_order_valid = false;
  prv_folder_update_row(data);
  launcher_menu_layer_reload_data(&data->launcher_menu_layer);
  if (data->folder && data->folder->is_open) {
    launcher_menu_layer_reload_data(&data->folder->launcher_menu_layer);
  }
}

static void prv_folder_window_load(Window *window) {
  LauncherAppWindowData *data = window_get_user_data(window);
  LauncherFolderWindowData *folder = data->folder;

  LauncherMenuLayer *launcher_menu_layer = &folder->launcher_menu_layer;
  launcher_menu_layer_init(launcher_menu_layer, &data->folder_data_source);
  launcher_menu_layer_set_click_config_onto_window(launcher_menu_layer, window);
  layer_add_child(window_get_root_layer(window), launcher_menu_layer_get_layer(launcher_menu_layer));

  if (s_launcher_folder_persisted_data.open) {
    launcher_menu_layer_set_selection_state(launcher_menu_layer,
                                            &s_launcher_folder_persisted_data.selection_state);
  }
  s_launcher_folder_persisted_data.open = false;
  launcher_menu_layer_set_selection_animations_enabled(launcher_menu_layer, true);
}

static void prv_folder_window_unload(Window *window) {
  LauncherAppWindowData *data = window_get_user_data(window);
  LauncherFolderWindowData *folder = data->folder;
  LauncherMenuLayer *launcher_menu_layer = &folder->launcher_menu_layer;

  // Come back to the folder after an app launched from it
  s_launcher_folder_persisted_data.open = launcher_menu_layer->launched_app;
  launcher_menu_layer_get_selection_state(launcher_menu_layer,
                                          &s_launcher_folder_persisted_data.selection_state);
  if (launcher_menu_layer->launched_app) {
    launcher_menu_layer_get_selection_vertical_range(
        launcher_menu_layer, &s_launcher_app_persisted_data.draw_state.selection_vertical_range);
  }

  folder->is_open = false;
  launcher_menu_layer_deinit(launcher_menu_layer);
}

static void prv_folder_push(LauncherAppWindowData *data, bool animated) {
  if (!data->folder) {
    data->folder = app_zalloc_check(sizeof(*data->folder));
  }
  if (data->folder->is_open) {
    return;
  }
  data->folder->launcher_menu_layer = (LauncherMenuLayer){};
  data->folder->is_open = true;
  Window *window = &data->folder->window;
  window_init(window, WINDOW_NAME("Launcher Apps"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .load = prv_folder_window_load,
                                       .unload = prv_folder_window_unload,
                                     });
  app_window_stack_push(window, animated);
}

static void prv_folder_open(void *context) {
  prv_folder_push(context, true /* animated */);
}

static void prv_folder_init(LauncherAppWindowData *data) {
  app_menu_data_source_init(&data->folder_data_source,
                            &(AppMenuDataSourceCallbacks){
                              .changed = prv_folder_data_changed,
                              .filter = prv_folder_filter_callback,
                              .transform_index = prv_folder_transform_index,
                            },
                            data);
  data->folder_node = (AppMenuNode){
    .install_id = INSTALL_ID_INVALID,
    .app_num = SYSTEM_APP,
    .icon_resource_id = RESOURCE_ID_MENU_LAYER_GENERIC_WATCHAPP_ICON,
    /// Launcher item that holds the installed apps
    .name = (char *)pbl_i18n_get("Apps", data),
    .visibility = ProcessVisibilityShown,
    .sdk_version =
        {
          .major = PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR,
          .minor = PROCESS_INFO_CURRENT_SDK_VERSION_MINOR,
        },
  };
  prv_folder_update_row(data);
}

static void prv_folder_deinit(LauncherAppWindowData *data) {
  app_menu_data_source_deinit(&data->folder_data_source);
  app_free(data->folder_order);
  data->folder_order = nullptr;
  // Windows unload from the top, so the folder is closed by now
  if (data->folder && !data->folder->is_open) {
    app_free(data->folder);
  }
  data->folder = nullptr;
  pbl_i18n_free_all(data);
}
#endif

static void prv_data_changed(void *context) {
  LauncherAppWindowData *data = context;
  launcher_menu_layer_reload_data(&data->launcher_menu_layer);
}

//! We're not 100% sure of the order of the launcher list yet, so use this function to transform
//! the row index to achieve the desired list ordering
static uint16_t prv_transform_index(AppMenuDataSource *data_source, uint16_t original_index,
                                    void *context) {
#ifdef CONFIG_SHELL_SDK
  // We want the newest installed developer app to appear at the top
  // This works at the moment because there is only one system app, Watchfaces
  return app_menu_data_source_get_count(data_source) - 1 - original_index;
#else
  return original_index;
#endif
}

/////////////////////
// Window callbacks

static void prv_window_load(Window *window) {
  LauncherAppWindowData *data = window_get_user_data(window);

  Layer *window_root_layer = window_get_root_layer(window);

  AppMenuDataSource *data_source = &data->app_menu_data_source;
  app_menu_data_source_init(data_source,
                            &(AppMenuDataSourceCallbacks){
                              .changed = prv_data_changed,
                              .filter = prv_app_filter_callback,
                              .transform_index = prv_transform_index,
                            },
                            data);

  LauncherMenuLayer *launcher_menu_layer = &data->launcher_menu_layer;
  launcher_menu_layer_init(launcher_menu_layer, data_source);
#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
  prv_folder_init(data);
#endif
  launcher_menu_layer_set_click_config_onto_window(launcher_menu_layer, window);
  layer_add_child(window_root_layer, launcher_menu_layer_get_layer(launcher_menu_layer));

  // If we have a saved launcher selection state, restore it
  if (s_launcher_app_persisted_data.valid) {
    launcher_menu_layer_set_selection_state(launcher_menu_layer,
                                            &s_launcher_app_persisted_data.selection_state);
  }

  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .did_focus = prv_did_focus,
    .will_focus = prv_will_focus,
  });

  data->pref_change_event_info = (EventServiceInfo){
    .type = PEBBLE_PREF_CHANGE_EVENT,
    .handler = prv_pref_change_handler,
    .context = data,
  };
  event_service_client_subscribe(&data->pref_change_event_info);

#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
  // Back from an app launched from the folder: open the folder again
  if (s_launcher_app_persisted_data.valid && s_launcher_folder_persisted_data.open &&
      (app_menu_data_source_get_count(&data->folder_data_source) > 0)) {
    prv_folder_push(data, false /* animated */);
  }
  s_launcher_folder_persisted_data.open = false;
#endif
}

static void prv_window_unload(Window *window) {
  LauncherAppWindowData *data = window_get_user_data(window);

  // Capture the vertical range of the selection rectangle for compositor transition animations
  GRangeVertical launcher_selection_vertical_range;
  launcher_menu_layer_get_selection_vertical_range(&data->launcher_menu_layer,
                                                   &launcher_selection_vertical_range);
#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
  // An app launched from the folder animates from the folder's selection
  if (s_launcher_folder_persisted_data.open) {
    launcher_selection_vertical_range =
        s_launcher_app_persisted_data.draw_state.selection_vertical_range;
  }
#endif

  // Save the current state of the launcher so we can know its draw state and restore it later
  s_launcher_app_persisted_data = (LauncherAppPersistedData){
    .valid = true,
    .leave_time = pbl_uptime_ticks(),
    .draw_state.selection_vertical_range = launcher_selection_vertical_range,
    .draw_state.selection_background_color = shell_prefs_get_theme_highlight_color(),
  };
  launcher_menu_layer_get_selection_state(&data->launcher_menu_layer,
                                          &s_launcher_app_persisted_data.selection_state);

  event_service_client_unsubscribe(&data->pref_change_event_info);
  app_focus_service_unsubscribe();
  launcher_menu_layer_deinit(&data->launcher_menu_layer);
  app_menu_data_source_deinit(&data->app_menu_data_source);
#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
  prv_folder_deinit(data);
#endif
}

////////////////////
// App boilerplate

static void prv_launcher_menu_window_push(void) {
  LauncherAppWindowData *data = app_zalloc_check(sizeof(*data));
  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Launcher Menu"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .load = prv_window_load,
                                       .unload = prv_window_unload,
                                     });

  const bool animated = false;
  app_window_stack_push(window, animated);
}

static void prv_main(void) {
  const LauncherMenuArgs *args = (const LauncherMenuArgs *)app_manager_get_task_context()->args;
  // Reset the selection state of the launcher if we're visiting it for the first time or if
  // it has been more than RETURN_TIMEOUT_TICKS since we were last in the launcher
  if (args && args->reset_scroll) {
    if ((s_launcher_app_persisted_data.leave_time + RETURN_TIMEOUT_TICKS) <= pbl_uptime_ticks()) {
      s_launcher_app_persisted_data.valid = false;
    }
  }

  prv_launcher_menu_window_push();

  app_idle_timeout_start(APP_IDLE_TIMEOUT_LAUNCHER_MS);

  app_event_loop();
}

const PebbleProcessMd *launcher_menu_app_get_app_info(void) {
  static const PebbleProcessMdSystem s_launcher_menu_app_info = {
    .common =
        {.main_func = prv_main,
         // UUID: dec0424c-0625-4878-b1f2-147e57e83688
         .uuid =
             {0xde, 0xc0, 0x42, 0x4c, 0x06, 0x25, 0x48, 0x78, 0xb1, 0xf2, 0x14, 0x7e, 0x57, 0xe8,
              0x36, 0x88},
         .visibility = ProcessVisibilityHidden},
    .name = "Launcher",
  };
  return (const PebbleProcessMd *)&s_launcher_menu_app_info;
}

const LauncherDrawState *launcher_app_get_draw_state(void) {
  return &s_launcher_app_persisted_data.draw_state;
}
