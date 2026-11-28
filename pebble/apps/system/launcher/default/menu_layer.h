/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "app_glance_service.h"

#include <applib/preferred_content_size.h>
#include <process_management/app_menu_data_source.h>
#include <shell/system_theme_style.h>

typedef SystemThemeLauncherStyle LauncherMenuLayerStyle;

typedef struct LauncherMenuLayer {
  Layer container_layer;
  MenuLayer menu_layer;
#if PBL_ROUND
  Layer up_arrow_layer;
  Layer down_arrow_layer;
#endif
  PreferredContentSize content_size;
  AppMenuDataSource *data_source;
  LauncherAppGlanceService glance_service;
  bool selection_animations_enabled;
  AppInstallId app_to_launch_after_next_render;
#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
  //! Row that opens a folder instead of launching an app; nullptr for none
  AppMenuNode *folder_node;
  //! The folder row follows the app with this install ID, or comes first without it
  AppInstallId folder_after_install_id;
  void (*folder_selected)(void *context);
  void *folder_context;
  //! Set once the user picked an app to launch
  bool launched_app;
#endif
} LauncherMenuLayer;

typedef struct LauncherMenuLayerSelectionState {
  int16_t scroll_offset_y;
  uint16_t row_index;
  //! Content size the scroll offset was captured with
  PreferredContentSize content_size;
} LauncherMenuLayerSelectionState;

//! @return The style for the user's preferred content size
const LauncherMenuLayerStyle *launcher_menu_layer_get_style(void);

void launcher_menu_layer_init(LauncherMenuLayer *launcher_menu_layer,
                              AppMenuDataSource *data_source);

Layer *launcher_menu_layer_get_layer(LauncherMenuLayer *launcher_menu_layer);

void launcher_menu_layer_set_click_config_onto_window(LauncherMenuLayer *launcher_menu_layer,
                                                      Window *window);

void launcher_menu_layer_reload_data(LauncherMenuLayer *launcher_menu_layer);

//! Re-initialize the launcher menu layer if the user's preferred content size changed since it was
//! initialized, keeping the current selection.
void launcher_menu_layer_update_content_size(LauncherMenuLayer *launcher_menu_layer);

void launcher_menu_layer_set_selection_state(LauncherMenuLayer *launcher_menu_layer,
                                             const LauncherMenuLayerSelectionState *new_state);

void launcher_menu_layer_get_selection_state(const LauncherMenuLayer *launcher_menu_layer,
                                             LauncherMenuLayerSelectionState *state_out);

void launcher_menu_layer_get_selection_vertical_range(const LauncherMenuLayer *launcher_menu_layer,
                                                      GRangeVertical *vertical_range_out);

void launcher_menu_layer_set_selection_animations_enabled(LauncherMenuLayer *launcher_menu_layer,
                                                          bool enabled);

void launcher_menu_layer_deinit(LauncherMenuLayer *launcher_menu_layer);

#if defined(CONFIG_BT_MULTI_LINK_EXPERIMENT)
//! Shows @p node as a folder row after the app with @p after_install_id; nullptr hides it
void launcher_menu_layer_set_folder_row(LauncherMenuLayer *launcher_menu_layer, AppMenuNode *node,
                                        AppInstallId after_install_id,
                                        void (*selected)(void *context), void *context);
#endif
