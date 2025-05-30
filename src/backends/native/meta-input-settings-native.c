/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include <linux/input-event-codes.h>
#include <libinput.h>

#include "backends/native/meta-backend-native.h"
#include "backends/native/meta-input-thread.h"
#include "backends/native/meta-input-settings-native.h"

struct _MetaInputSettingsNative
{
  MetaInputSettings parent_instance;
  MetaSeatImpl *seat_impl;
};

G_DEFINE_FINAL_TYPE (MetaInputSettingsNative,
                     meta_input_settings_native,
                     META_TYPE_INPUT_SETTINGS)

enum
{
  PROP_0,
  PROP_SEAT_IMPL,
  N_PROPS,
};

static GParamSpec *props[N_PROPS] = { 0 };

static void
meta_input_settings_native_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  MetaInputSettingsNative *input_settings_native =
    META_INPUT_SETTINGS_NATIVE (object);

  switch (prop_id)
    {
    case PROP_SEAT_IMPL:
      input_settings_native->seat_impl = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_input_settings_native_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  MetaInputSettingsNative *input_settings_native =
    META_INPUT_SETTINGS_NATIVE (object);

  switch (prop_id)
    {
    case PROP_SEAT_IMPL:
      g_value_set_object (value, input_settings_native->seat_impl);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
set_send_events (GTask *task)
{
  GDesktopDeviceSendEvents mode;
  ClutterInputDevice *device;
  enum libinput_config_send_events_mode libinput_mode;
  struct libinput_device *libinput_device;

  device = g_task_get_source_object (task);
  mode = GPOINTER_TO_UINT (g_task_get_task_data (task));

  switch (mode)
    {
    case G_DESKTOP_DEVICE_SEND_EVENTS_DISABLED:
      libinput_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
      break;
    case G_DESKTOP_DEVICE_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE:
      libinput_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
      break;
    case G_DESKTOP_DEVICE_SEND_EVENTS_ENABLED:
      libinput_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
      break;
    default:
      g_assert_not_reached ();
    }

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (libinput_device)
    libinput_device_config_send_events_set_mode (libinput_device, libinput_mode);

  return G_SOURCE_REMOVE;
}

static void
meta_input_settings_native_set_send_events (MetaInputSettings        *settings,
                                            ClutterInputDevice       *device,
                                            GDesktopDeviceSendEvents  mode)
{
  MetaInputSettingsNative *input_settings_native;
  GTask *task;

  task = g_task_new (device, NULL, NULL, NULL);
  g_task_set_task_data (task, GUINT_TO_POINTER (mode), NULL);

  input_settings_native = META_INPUT_SETTINGS_NATIVE (settings);
  meta_seat_impl_run_input_task (input_settings_native->seat_impl,
                                 task, (GSourceFunc) set_send_events);
  g_object_unref (task);
}

static gboolean
set_matrix (GTask *task)
{
  ClutterInputDevice *device = g_task_get_source_object (task);
  float *matrix = g_task_get_task_data (task);
  graphene_matrix_t dev_matrix;

  graphene_matrix_init_from_2d (&dev_matrix,
                                matrix[0], matrix[3], matrix[1],
                                matrix[4], matrix[2], matrix[5]);
  g_object_set (device, "device-matrix", &dev_matrix, NULL);

  return G_SOURCE_REMOVE;
}

static void
meta_input_settings_native_set_matrix (MetaInputSettings  *settings,
                                       ClutterInputDevice *device,
                                       const float         matrix[6])
{
  MetaInputSettingsNative *input_settings_native;
  GTask *task;

  task = g_task_new (device, NULL, NULL, NULL);

  g_task_set_task_data (task, g_memdup2 (matrix, sizeof (float) * 6), g_free);

  input_settings_native = META_INPUT_SETTINGS_NATIVE (settings);
  meta_seat_impl_run_input_task (input_settings_native->seat_impl,
                                 task, (GSourceFunc) set_matrix);
  g_object_unref (task);
}

static void
meta_input_settings_native_set_speed (MetaInputSettings  *settings,
                                      ClutterInputDevice *device,
                                      gdouble             speed)
{
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;
  libinput_device_config_accel_set_speed (libinput_device,
                                          CLAMP (speed, -1, 1));
}

static void
meta_input_settings_native_set_left_handed (MetaInputSettings  *settings,
                                            ClutterInputDevice *device,
                                            gboolean            enabled)
{
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_left_handed_is_available (libinput_device))
    libinput_device_config_left_handed_set (libinput_device, enabled);
}

static void
meta_input_settings_native_set_tap_enabled (MetaInputSettings  *settings,
                                            ClutterInputDevice *device,
                                            gboolean            enabled)
{
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_tap_get_finger_count (libinput_device) > 0)
    libinput_device_config_tap_set_enabled (libinput_device,
                                            enabled ?
                                            LIBINPUT_CONFIG_TAP_ENABLED :
                                            LIBINPUT_CONFIG_TAP_DISABLED);
}

static void
meta_input_settings_native_set_tap_and_drag_enabled (MetaInputSettings  *settings,
                                                     ClutterInputDevice *device,
                                                     gboolean            enabled)
{
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_tap_get_finger_count (libinput_device) > 0)
    libinput_device_config_tap_set_drag_enabled (libinput_device,
                                                 enabled ?
                                                 LIBINPUT_CONFIG_DRAG_ENABLED :
                                                 LIBINPUT_CONFIG_DRAG_DISABLED);
}

static void
meta_input_settings_native_set_tap_and_drag_lock_enabled (MetaInputSettings  *settings,
                                                          ClutterInputDevice *device,
                                                          gboolean            enabled)
{
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_tap_get_finger_count (libinput_device) > 0)
    libinput_device_config_tap_set_drag_lock_enabled (libinput_device,
                                                      enabled ?
                                                      LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY :
                                                      LIBINPUT_CONFIG_DRAG_LOCK_DISABLED);
}

static void
meta_input_settings_native_set_disable_while_typing (MetaInputSettings  *settings,
                                                     ClutterInputDevice *device,
                                                     gboolean            enabled)
{
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);

  if (!libinput_device)
    return;

  if (libinput_device_config_dwt_is_available (libinput_device))
    libinput_device_config_dwt_set_enabled (libinput_device,
                                            enabled ?
                                            LIBINPUT_CONFIG_DWT_ENABLED :
                                            LIBINPUT_CONFIG_DWT_DISABLED);
}

static void
meta_input_settings_native_set_invert_scroll (MetaInputSettings  *settings,
                                              ClutterInputDevice *device,
                                              gboolean            inverted)
{
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_scroll_has_natural_scroll (libinput_device))
    libinput_device_config_scroll_set_natural_scroll_enabled (libinput_device,
                                                              inverted);
}

static gboolean
device_set_scroll_method (struct libinput_device             *libinput_device,
                          enum libinput_config_scroll_method  method)
{
  enum libinput_config_status status =
    libinput_device_config_scroll_set_method (libinput_device, method);
  return status == LIBINPUT_CONFIG_STATUS_SUCCESS;
}

static gboolean
device_set_click_method (struct libinput_device            *libinput_device,
                         enum libinput_config_click_method  method)
{
  enum libinput_config_status status =
    libinput_device_config_click_set_method (libinput_device, method);
  return status == LIBINPUT_CONFIG_STATUS_SUCCESS;
}

static gboolean
device_set_tap_button_map (struct libinput_device              *libinput_device,
                           enum libinput_config_tap_button_map  map)
{
  enum libinput_config_status status =
    libinput_device_config_tap_set_button_map (libinput_device, map);
  return status == LIBINPUT_CONFIG_STATUS_SUCCESS;
}

static void
meta_input_settings_native_set_edge_scroll (MetaInputSettings            *settings,
                                            ClutterInputDevice           *device,
                                            gboolean                      edge_scrolling_enabled)
{
  struct libinput_device *libinput_device;
  enum libinput_config_scroll_method current, method;

  libinput_device = meta_input_device_native_get_libinput_device (device);

  method = edge_scrolling_enabled ? LIBINPUT_CONFIG_SCROLL_EDGE : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
  current = libinput_device_config_scroll_get_method (libinput_device);
  current &= ~LIBINPUT_CONFIG_SCROLL_EDGE;

  device_set_scroll_method (libinput_device, current | method);
}

static void
meta_input_settings_native_set_two_finger_scroll (MetaInputSettings            *settings,
                                                  ClutterInputDevice           *device,
                                                  gboolean                      two_finger_scroll_enabled)
{
  struct libinput_device *libinput_device;
  enum libinput_config_scroll_method current, method;

  libinput_device = meta_input_device_native_get_libinput_device (device);

  method = two_finger_scroll_enabled ? LIBINPUT_CONFIG_SCROLL_2FG : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
  current = libinput_device_config_scroll_get_method (libinput_device);
  current &= ~LIBINPUT_CONFIG_SCROLL_2FG;

  device_set_scroll_method (libinput_device, current | method);
}

static gboolean
meta_input_settings_native_has_two_finger_scroll (MetaInputSettings  *settings,
                                                  ClutterInputDevice *device)
{
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return FALSE;

  return libinput_device_config_scroll_get_methods (libinput_device) & LIBINPUT_CONFIG_SCROLL_2FG;
}

static void
meta_input_settings_native_set_scroll_button (MetaInputSettings  *settings,
                                              ClutterInputDevice *device,
                                              guint               button,
                                              gboolean            button_lock)
{
  struct libinput_device *libinput_device;
  enum libinput_config_scroll_method method;
  enum libinput_config_scroll_button_lock_state lock_state;
  guint evcode;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (button == 0)
    {
      method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
      evcode = 0;
    }
  else
    {
      evcode = meta_clutter_button_to_evdev (button);
      method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    }

  if (!device_set_scroll_method (libinput_device, method))
    return;

  libinput_device_config_scroll_set_button (libinput_device, evcode);

  if (button_lock)
    lock_state = LIBINPUT_CONFIG_SCROLL_BUTTON_LOCK_ENABLED;
  else
    lock_state = LIBINPUT_CONFIG_SCROLL_BUTTON_LOCK_DISABLED;

  libinput_device_config_scroll_set_button_lock (libinput_device, lock_state);
}

static void
meta_input_settings_native_set_click_method (MetaInputSettings           *settings,
                                             ClutterInputDevice          *device,
                                             GDesktopTouchpadClickMethod  mode)
{
  enum libinput_config_click_method click_method = 0;
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  switch (mode)
    {
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_DEFAULT:
      click_method = libinput_device_config_click_get_default_method (libinput_device);
      break;
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_NONE:
      click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
      break;
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_AREAS:
      click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
      break;
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_FINGERS:
      click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
      break;
    default:
      g_assert_not_reached ();
      return;
  }

  device_set_click_method (libinput_device, click_method);
}

static void
meta_input_settings_native_set_tap_button_map (MetaInputSettings            *settings,
                                               ClutterInputDevice           *device,
                                               GDesktopTouchpadTapButtonMap  mode)
{
  enum libinput_config_tap_button_map button_map = 0;
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_tap_get_finger_count (libinput_device) == 0)
    return;

  switch (mode)
    {
    case G_DESKTOP_TOUCHPAD_BUTTON_TAP_MAP_DEFAULT:
      button_map = libinput_device_config_tap_get_default_button_map (libinput_device);
      break;
    case G_DESKTOP_TOUCHPAD_BUTTON_TAP_MAP_LRM:
      button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
      break;
    case G_DESKTOP_TOUCHPAD_BUTTON_TAP_MAP_LMR:
      button_map = LIBINPUT_CONFIG_TAP_MAP_LMR;
      break;
    default:
      g_assert_not_reached ();
      return;
  }

  device_set_tap_button_map (libinput_device, button_map);
}

static void
meta_input_settings_native_set_keyboard_repeat (MetaInputSettings *settings,
                                                gboolean           enabled,
                                                guint              delay,
                                                guint              interval)
{
  MetaInputSettingsNative *input_settings_native;

  input_settings_native = META_INPUT_SETTINGS_NATIVE (settings);
  meta_seat_impl_set_keyboard_repeat_in_impl (input_settings_native->seat_impl,
                                              enabled, delay, interval);
}

static void
set_device_accel_profile (ClutterInputDevice         *device,
                          GDesktopPointerAccelProfile profile)
{
  struct libinput_device *libinput_device;
  enum libinput_config_accel_profile libinput_profile;
  uint32_t profiles;

  libinput_device = meta_input_device_native_get_libinput_device (device);

  switch (profile)
    {
    case G_DESKTOP_POINTER_ACCEL_PROFILE_FLAT:
      libinput_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
      break;
    case G_DESKTOP_POINTER_ACCEL_PROFILE_ADAPTIVE:
      libinput_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
      break;
    default:
      g_warn_if_reached ();
      G_GNUC_FALLTHROUGH;
    case G_DESKTOP_POINTER_ACCEL_PROFILE_DEFAULT:
      libinput_profile =
        libinput_device_config_accel_get_default_profile (libinput_device);
    }

  profiles = libinput_device_config_accel_get_profiles (libinput_device);
  if ((profiles & libinput_profile) == 0)
    {
      libinput_profile =
        libinput_device_config_accel_get_default_profile (libinput_device);
    }

  libinput_device_config_accel_set_profile (libinput_device,
                                            libinput_profile);
}

static void
meta_input_settings_native_set_mouse_accel_profile (MetaInputSettings          *settings,
                                                    ClutterInputDevice         *device,
                                                    GDesktopPointerAccelProfile profile)
{
  ClutterInputCapabilities caps = clutter_input_device_get_capabilities (device);

  if ((caps & CLUTTER_INPUT_CAPABILITY_POINTER) == 0)
    return;
  if ((caps &
       (CLUTTER_INPUT_CAPABILITY_TRACKBALL |
        CLUTTER_INPUT_CAPABILITY_TOUCHPAD |
        CLUTTER_INPUT_CAPABILITY_TRACKPOINT)) != 0)
    return;

  set_device_accel_profile (device, profile);
}

static void
meta_input_settings_native_set_touchpad_accel_profile (MetaInputSettings           *settings,
                                                       ClutterInputDevice          *device,
                                                       GDesktopPointerAccelProfile  profile)
{
  ClutterInputCapabilities caps = clutter_input_device_get_capabilities (device);

  if ((caps & CLUTTER_INPUT_CAPABILITY_TOUCHPAD) == 0)
    return;

  set_device_accel_profile (device, profile);
}

static void
meta_input_settings_native_set_trackball_accel_profile (MetaInputSettings          *settings,
                                                        ClutterInputDevice         *device,
                                                        GDesktopPointerAccelProfile profile)
{
  ClutterInputCapabilities caps = clutter_input_device_get_capabilities (device);

  if ((caps & CLUTTER_INPUT_CAPABILITY_TRACKBALL) == 0)
    return;

  set_device_accel_profile (device, profile);
}

static void
meta_input_settings_native_set_pointing_stick_accel_profile (MetaInputSettings           *settings,
                                                             ClutterInputDevice          *device,
                                                             GDesktopPointerAccelProfile  profile)
{
  ClutterInputCapabilities caps = clutter_input_device_get_capabilities (device);

  if ((caps & CLUTTER_INPUT_CAPABILITY_TRACKPOINT) == 0)
    return;

  set_device_accel_profile (device, profile);
}

static void
meta_input_settings_native_set_pointing_stick_scroll_method (MetaInputSettings                 *settings,
                                                             ClutterInputDevice                *device,
                                                             GDesktopPointingStickScrollMethod  method)
{
  ClutterInputCapabilities caps = clutter_input_device_get_capabilities (device);
  struct libinput_device *libinput_device;
  enum libinput_config_scroll_method libinput_method;

  if ((caps & CLUTTER_INPUT_CAPABILITY_TRACKPOINT) == 0)
    return;

  libinput_device = meta_input_device_native_get_libinput_device (device);

  switch (method)
    {
    case G_DESKTOP_POINTING_STICK_SCROLL_METHOD_DEFAULT:
      libinput_method = libinput_device_config_scroll_get_default_method (libinput_device);
      break;
    case G_DESKTOP_POINTING_STICK_SCROLL_METHOD_NONE:
      libinput_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
      break;
    case G_DESKTOP_POINTING_STICK_SCROLL_METHOD_ON_BUTTON_DOWN:
      libinput_method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
      break;
    default:
      g_assert_not_reached ();
      return;
    }

  device_set_scroll_method (libinput_device, libinput_method);
}

static void
meta_input_settings_native_set_tablet_mapping (MetaInputSettings     *settings,
                                               ClutterInputDevice    *device,
                                               GDesktopTabletMapping  mapping)
{
  MetaInputDeviceMapping dev_mapping;

  if (mapping == G_DESKTOP_TABLET_MAPPING_ABSOLUTE)
    dev_mapping = META_INPUT_DEVICE_MAPPING_ABSOLUTE;
  else if (mapping == G_DESKTOP_TABLET_MAPPING_RELATIVE)
    dev_mapping = META_INPUT_DEVICE_MAPPING_RELATIVE;
  else
    return;

  meta_input_device_native_set_mapping_mode_in_impl (device, dev_mapping);
}

static gboolean
set_tablet_aspect_ratio (GTask *task)
{
  ClutterInputDevice *device;
  double *aspect_ratio;

  device = g_task_get_source_object (task);
  aspect_ratio = g_task_get_task_data (task);
  g_object_set (device, "output-aspect-ratio", *aspect_ratio, NULL);

  return G_SOURCE_REMOVE;
}

static void
meta_input_settings_native_set_tablet_aspect_ratio (MetaInputSettings  *settings,
                                                    ClutterInputDevice *device,
                                                    gdouble             aspect_ratio)
{
  MetaInputSettingsNative *input_settings_native;
  GTask *task;

  if (meta_input_device_native_get_mapping_mode_in_impl (device) ==
      META_INPUT_DEVICE_MAPPING_RELATIVE)
    aspect_ratio = 0;

  task = g_task_new (device, NULL, NULL, NULL);
  g_task_set_task_data (task,
                        g_memdup2 (&aspect_ratio, sizeof (double)),
                        g_free);

  input_settings_native = META_INPUT_SETTINGS_NATIVE (settings);
  meta_seat_impl_run_input_task (input_settings_native->seat_impl,
                                 task, (GSourceFunc) set_tablet_aspect_ratio);
  g_object_unref (task);
}

static void
meta_input_settings_native_set_tablet_area (MetaInputSettings  *settings,
                                            ClutterInputDevice *device,
                                            gdouble             padding_left,
                                            gdouble             padding_right,
                                            gdouble             padding_top,
                                            gdouble             padding_bottom)
{
  struct libinput_device *libinput_device;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device ||
      !libinput_device_config_calibration_has_matrix (libinput_device))
    return;

  if (padding_left == 0.0 && padding_right == 0.0 &&
      padding_top == 0.0 && padding_bottom == 0.0)
    {
      float matrix[6];
      libinput_device_config_calibration_get_default_matrix (libinput_device, matrix);
      libinput_device_config_calibration_set_matrix (libinput_device, matrix);
    }
  else
    {
      float scale_x = (float) (1.0 / (1.0 - (padding_left + padding_right)));
      float scale_y = (float) (1.0 / (1.0 - (padding_top + padding_bottom)));
      float offset_x = (float) (-padding_left * scale_x);
      float offset_y = (float) (-padding_top * scale_y);

      float matrix[6] = { scale_x, 0., offset_x,
                          0., scale_y, offset_y };

      libinput_device_config_calibration_set_matrix (libinput_device, matrix);
    }
}

static void
meta_input_settings_native_set_stylus_pressure (MetaInputSettings      *settings,
                                                ClutterInputDevice     *device,
                                                ClutterInputDeviceTool *tool,
                                                const gint              curve[4],
                                                const gdouble           range[2])
{
  gdouble pressure_curve[4];
  gdouble pressure_range[2];

  pressure_curve[0] = (gdouble) curve[0] / 100;
  pressure_curve[1] = (gdouble) curve[1] / 100;
  pressure_curve[2] = (gdouble) curve[2] / 100;
  pressure_curve[3] = (gdouble) curve[3] / 100;

  pressure_range[0] = (gdouble) range[0];
  pressure_range[1] = (gdouble) range[1];

  meta_input_device_tool_native_set_pressure_curve_in_impl (tool, pressure_curve, pressure_range);
}

static void
meta_input_settings_native_set_stylus_button_map (MetaInputSettings          *settings,
                                                  ClutterInputDevice         *device,
                                                  ClutterInputDeviceTool     *tool,
                                                  GDesktopStylusButtonAction  primary,
                                                  GDesktopStylusButtonAction  secondary,
                                                  GDesktopStylusButtonAction  tertiary)
{
  meta_input_device_tool_native_set_button_code_in_impl (tool, CLUTTER_BUTTON_MIDDLE, primary);
  meta_input_device_tool_native_set_button_code_in_impl (tool, CLUTTER_BUTTON_SECONDARY, secondary);
  meta_input_device_tool_native_set_button_code_in_impl (tool, 8, tertiary);
}

static void
meta_input_settings_native_set_mouse_middle_click_emulation (MetaInputSettings  *settings,
                                                             ClutterInputDevice *device,
                                                             gboolean            enabled)
{
  struct libinput_device *libinput_device;
  ClutterInputCapabilities caps = clutter_input_device_get_capabilities (device);

  if ((caps & CLUTTER_INPUT_CAPABILITY_POINTER) == 0)
    return;
  if ((caps &
       (CLUTTER_INPUT_CAPABILITY_TRACKBALL |
        CLUTTER_INPUT_CAPABILITY_TOUCHPAD |
        CLUTTER_INPUT_CAPABILITY_TRACKPOINT)) != 0)
    return;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_middle_emulation_is_available (libinput_device))
    libinput_device_config_middle_emulation_set_enabled (libinput_device, enabled);
}

static void
meta_input_settings_native_set_touchpad_middle_click_emulation (MetaInputSettings  *settings,
                                                                ClutterInputDevice *device,
                                                                gboolean            enabled)
{
  struct libinput_device *libinput_device;
  ClutterInputCapabilities caps = clutter_input_device_get_capabilities (device);

  if ((caps & CLUTTER_INPUT_CAPABILITY_TOUCHPAD) == 0)
    return;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_middle_emulation_is_available (libinput_device))
    libinput_device_config_middle_emulation_set_enabled (libinput_device, enabled);
}

static void
meta_input_settings_native_set_trackball_middle_click_emulation (MetaInputSettings  *settings,
                                                                 ClutterInputDevice *device,
                                                                 gboolean            enabled)
{
  struct libinput_device *libinput_device;
  ClutterInputCapabilities caps = clutter_input_device_get_capabilities (device);

  if ((caps & CLUTTER_INPUT_CAPABILITY_TRACKBALL) == 0)
    return;

  libinput_device = meta_input_device_native_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_middle_emulation_is_available (libinput_device))
    libinput_device_config_middle_emulation_set_enabled (libinput_device, enabled);
}

static void
meta_input_settings_native_class_init (MetaInputSettingsNativeClass *klass)
{
  MetaInputSettingsClass *input_settings_class = META_INPUT_SETTINGS_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = meta_input_settings_native_set_property;
  object_class->get_property = meta_input_settings_native_get_property;

  input_settings_class->set_send_events = meta_input_settings_native_set_send_events;
  input_settings_class->set_matrix = meta_input_settings_native_set_matrix;
  input_settings_class->set_speed = meta_input_settings_native_set_speed;
  input_settings_class->set_left_handed = meta_input_settings_native_set_left_handed;
  input_settings_class->set_tap_enabled = meta_input_settings_native_set_tap_enabled;
  input_settings_class->set_tap_button_map = meta_input_settings_native_set_tap_button_map;
  input_settings_class->set_tap_and_drag_enabled = meta_input_settings_native_set_tap_and_drag_enabled;
  input_settings_class->set_tap_and_drag_lock_enabled =
    meta_input_settings_native_set_tap_and_drag_lock_enabled;
  input_settings_class->set_invert_scroll = meta_input_settings_native_set_invert_scroll;
  input_settings_class->set_edge_scroll = meta_input_settings_native_set_edge_scroll;
  input_settings_class->set_two_finger_scroll = meta_input_settings_native_set_two_finger_scroll;
  input_settings_class->set_scroll_button = meta_input_settings_native_set_scroll_button;
  input_settings_class->set_click_method = meta_input_settings_native_set_click_method;
  input_settings_class->set_keyboard_repeat = meta_input_settings_native_set_keyboard_repeat;
  input_settings_class->set_disable_while_typing = meta_input_settings_native_set_disable_while_typing;

  input_settings_class->set_tablet_mapping = meta_input_settings_native_set_tablet_mapping;
  input_settings_class->set_tablet_aspect_ratio = meta_input_settings_native_set_tablet_aspect_ratio;
  input_settings_class->set_tablet_area = meta_input_settings_native_set_tablet_area;

  input_settings_class->set_mouse_accel_profile = meta_input_settings_native_set_mouse_accel_profile;
  input_settings_class->set_touchpad_accel_profile = meta_input_settings_native_set_touchpad_accel_profile;
  input_settings_class->set_trackball_accel_profile = meta_input_settings_native_set_trackball_accel_profile;
  input_settings_class->set_pointing_stick_accel_profile = meta_input_settings_native_set_pointing_stick_accel_profile;
  input_settings_class->set_pointing_stick_scroll_method = meta_input_settings_native_set_pointing_stick_scroll_method;

  input_settings_class->set_stylus_pressure = meta_input_settings_native_set_stylus_pressure;
  input_settings_class->set_stylus_button_map = meta_input_settings_native_set_stylus_button_map;

  input_settings_class->set_mouse_middle_click_emulation = meta_input_settings_native_set_mouse_middle_click_emulation;
  input_settings_class->set_touchpad_middle_click_emulation = meta_input_settings_native_set_touchpad_middle_click_emulation;
  input_settings_class->set_trackball_middle_click_emulation = meta_input_settings_native_set_trackball_middle_click_emulation;

  input_settings_class->has_two_finger_scroll = meta_input_settings_native_has_two_finger_scroll;

  props[PROP_SEAT_IMPL] =
    g_param_spec_object ("seat-impl", NULL, NULL,
                         META_TYPE_SEAT_IMPL,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
meta_input_settings_native_init (MetaInputSettingsNative *settings)
{
}

MetaInputSettings *
meta_input_settings_native_new_in_impl (MetaSeatImpl *seat_impl)
{
  return g_object_new (META_TYPE_INPUT_SETTINGS_NATIVE,
                       "backend", meta_seat_impl_get_backend (seat_impl),
                       "seat-impl", seat_impl,
                       NULL);
}
