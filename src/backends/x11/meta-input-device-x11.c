/*
 * Copyright © 2011  Intel Corp.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#include "config.h"

#include <X11/extensions/XInput2.h>

#include "clutter/clutter-mutter.h"
#include "backends/x11/meta-backend-x11.h"
#include "backends/x11/meta-clutter-backend-x11.h"
#include "backends/x11/meta-input-device-x11.h"
#include "backends/x11/meta-seat-x11.h"
#include "mtk/mtk-x11.h"

struct _MetaInputDeviceX11
{
  MetaInputDevice parent_instance;

  ClutterInputDevice device;

  int32_t device_id;
  ClutterInputDeviceTool *current_tool;

  int inhibit_pointer_query_timer;
  gboolean query_status;
  float current_x;
  float current_y;

  GArray *axes;
  GArray *scroll_info;

#ifdef HAVE_LIBWACOM
  GArray *group_modes;
#endif
};

typedef struct _MetaX11AxisInfo
{
  ClutterInputAxis axis;

  double min_axis;
  double max_axis;

  double min_value;
  double max_value;

  double resolution;
} MetaX11AxisInfo;

typedef struct _MetaX11ScrollInfo
{
  guint axis_id;
  ClutterScrollDirection direction;
  double increment;

  double last_value;
  guint last_value_valid : 1;
} MetaX11ScrollInfo;


#define N_BUTTONS       5

enum
{
  PROP_0,
  PROP_ID,
  N_PROPS
};

static GParamSpec *props[N_PROPS] = { 0 };

G_DEFINE_FINAL_TYPE (MetaInputDeviceX11,
                     meta_input_device_x11,
                     META_TYPE_INPUT_DEVICE)

static void
meta_input_device_x11_constructed (GObject *object)
{
  MetaInputDeviceX11 *device_xi2 = META_INPUT_DEVICE_X11 (object);

  if (G_OBJECT_CLASS (meta_input_device_x11_parent_class)->constructed)
    G_OBJECT_CLASS (meta_input_device_x11_parent_class)->constructed (object);

#ifdef HAVE_LIBWACOM
  if (clutter_input_device_get_device_type (CLUTTER_INPUT_DEVICE (object)) == CLUTTER_PAD_DEVICE)
    {
      device_xi2->group_modes = g_array_new (FALSE, TRUE, sizeof (uint32_t));
      g_array_set_size (device_xi2->group_modes,
                        clutter_input_device_get_n_mode_groups (CLUTTER_INPUT_DEVICE (object)));
    }
#endif
}

static gboolean
meta_input_device_x11_is_grouped (ClutterInputDevice *device,
                                  ClutterInputDevice *other_device)
{
#ifdef HAVE_LIBWACOM
  WacomDevice *wacom_device, *other_wacom_device;

  wacom_device =
    meta_input_device_get_wacom_device (META_INPUT_DEVICE (device));
  other_wacom_device =
    meta_input_device_get_wacom_device (META_INPUT_DEVICE (other_device));

  if (wacom_device && other_wacom_device &&
      libwacom_compare (wacom_device,
                        other_wacom_device,
                        WCOMPARE_NORMAL) == 0)
    return TRUE;
#endif

  return clutter_input_device_get_vendor_id (device) ==
         clutter_input_device_get_vendor_id (other_device) &&
         clutter_input_device_get_product_id (device) ==
         clutter_input_device_get_product_id (other_device);
}

static void
meta_input_device_x11_finalize (GObject *object)
{
  MetaInputDeviceX11 *device_xi2 = META_INPUT_DEVICE_X11 (object);

  g_clear_pointer (&device_xi2->axes, g_array_unref);
  g_clear_pointer (&device_xi2->scroll_info, g_array_unref);

#ifdef HAVE_LIBWACOM
  if (device_xi2->group_modes)
    g_array_unref (device_xi2->group_modes);
#endif

  g_clear_handle_id (&device_xi2->inhibit_pointer_query_timer, g_source_remove);

  G_OBJECT_CLASS (meta_input_device_x11_parent_class)->finalize (object);
}

static void
meta_input_device_x11_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (object);

  switch (prop_id)
    {
    case PROP_ID:
      device_x11->device_id = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_input_device_x11_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_int (value, device_x11->device_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

#ifdef HAVE_LIBWACOM
#ifndef HAVE_LIBWACOM_GET_NUM_RINGS
static int
libwacom_get_num_rings (WacomDevice *device)
{
  if (libwacom_has_ring2 (device))
    return 2;

  if (libwacom_has_ring (device))
    return 1;

  return 0;
}
#endif
#endif

static int
meta_input_device_x11_get_group_n_modes (ClutterInputDevice *device,
                                         int                 group)
{
#ifdef HAVE_LIBWACOM
  WacomDevice *wacom_device;

  wacom_device = meta_input_device_get_wacom_device (META_INPUT_DEVICE (device));

  if (wacom_device)
    {
      if (group == 0)
        {
          if (libwacom_get_num_rings (wacom_device) >= 1)
            return libwacom_get_ring_num_modes (wacom_device);
          else if (libwacom_get_num_strips (wacom_device) >= 1)
            return libwacom_get_strips_num_modes (wacom_device);
        }
      else if (group == 1)
        {
          if (libwacom_get_num_rings (wacom_device) >= 2)
            return libwacom_get_ring2_num_modes (wacom_device);
          else if (libwacom_get_num_strips (wacom_device) >= 2)
            return libwacom_get_strips_num_modes (wacom_device);
        }
    }
#endif

  return -1;
}

#ifdef HAVE_LIBWACOM
static int
meta_input_device_x11_get_button_group (ClutterInputDevice *device,
                                        uint32_t            button)
{
  WacomDevice *wacom_device;

  wacom_device = meta_input_device_get_wacom_device (META_INPUT_DEVICE (device));

  if (wacom_device)
    {
      WacomButtonFlags flags;

      if (button >= libwacom_get_num_buttons (wacom_device))
        return -1;

      flags = libwacom_get_button_flag (wacom_device, 'A' + button);

      if (flags &
          (WACOM_BUTTON_RING_MODESWITCH |
           WACOM_BUTTON_TOUCHSTRIP_MODESWITCH))
        return 0;
      if (flags &
          (WACOM_BUTTON_RING2_MODESWITCH |
           WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH))
        return 1;
    }

  return -1;
}
#endif

static gboolean
meta_input_device_x11_is_mode_switch_button (ClutterInputDevice *device,
                                             uint32_t            group,
                                             uint32_t            button)
{
  int button_group = -1;

#ifdef HAVE_LIBWACOM
  button_group = meta_input_device_x11_get_button_group (device, button);
#endif

  return button_group == (int) group;
}

static gboolean
meta_input_device_x11_get_dimensions (ClutterInputDevice *device,
                                      unsigned int       *width,
                                      unsigned int       *height)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (device);
  ClutterSeat *seat = clutter_input_device_get_seat (device);
  MetaSeatX11 *seat_x11 = META_SEAT_X11 (seat);
  MetaBackendX11 *backend_x11 =
    META_BACKEND_X11 (meta_seat_x11_get_backend (seat_x11));
  Display *xdisplay =
    meta_backend_x11_get_xdisplay (backend_x11);
  XIDeviceInfo *info;
  uint *value, w, h;
  int i, n_info;
  static gboolean atoms_initialized = FALSE;
  static Atom abs_axis_atoms[4] = { 0, };

  mtk_x11_error_trap_push (xdisplay);

  info = XIQueryDevice (xdisplay, device_x11->device_id, &n_info);
  *width = *height = w = h = 0;

  if (mtk_x11_error_trap_pop_with_return (xdisplay))
    return FALSE;

  if (!info)
    return FALSE;

  if (G_UNLIKELY (!atoms_initialized))
    {
      const char *abs_axis_atom_names[4] = {
        "Abs X",
        "Abs MT Position X",
        "Abs Y",
        "Abs MT Position Y",
      };

      XInternAtoms (xdisplay,
                    (char **) abs_axis_atom_names,
                    G_N_ELEMENTS (abs_axis_atom_names),
                    False,
                    abs_axis_atoms);
      atoms_initialized = TRUE;
    }

  for (i = 0; i < info->num_classes; i++)
    {
      XIValuatorClassInfo *valuator_info;

      if (info->classes[i]->type != XIValuatorClass)
        continue;

      valuator_info = (XIValuatorClassInfo *) info->classes[i];

      if (valuator_info->label == abs_axis_atoms[0] || /* Abs X */
          valuator_info->label == abs_axis_atoms[1]) /* Abs MT X */
        value = &w;
      else if (valuator_info->label == abs_axis_atoms[2] || /* Abs Y */
               valuator_info->label == abs_axis_atoms[3]) /* Abs MT Y */
        value = &h;
      else
        continue;

      *value = (unsigned int) ((valuator_info->max - valuator_info->min) *
                               1000 /
                               valuator_info->resolution);
    }

  *width = w;
  *height = h;

  XIFreeDeviceInfo (info);

  return (w != 0 && h != 0);
}

static void
meta_input_device_x11_class_init (MetaInputDeviceX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterInputDeviceClass *device_class = CLUTTER_INPUT_DEVICE_CLASS (klass);

  gobject_class->constructed = meta_input_device_x11_constructed;
  gobject_class->finalize = meta_input_device_x11_finalize;
  gobject_class->set_property = meta_input_device_x11_set_property;
  gobject_class->get_property = meta_input_device_x11_get_property;

  device_class->is_grouped = meta_input_device_x11_is_grouped;
  device_class->get_group_n_modes = meta_input_device_x11_get_group_n_modes;
  device_class->is_mode_switch_button = meta_input_device_x11_is_mode_switch_button;
  device_class->get_dimensions = meta_input_device_x11_get_dimensions;

  props[PROP_ID] =
    g_param_spec_int ("id", NULL, NULL,
                      -1, G_MAXINT,
                      0,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, N_PROPS, props);
}

static void
meta_input_device_x11_init (MetaInputDeviceX11 *self)
{
}

void
meta_input_device_x11_update_tool (ClutterInputDevice     *device,
                                   ClutterInputDeviceTool *tool)
{
  MetaInputDeviceX11 *device_xi2 = META_INPUT_DEVICE_X11 (device);
  g_set_object (&device_xi2->current_tool, tool);
}

ClutterInputDeviceTool *
meta_input_device_x11_get_current_tool (ClutterInputDevice *device)
{
  MetaInputDeviceX11 *device_xi2 = META_INPUT_DEVICE_X11 (device);
  return device_xi2->current_tool;
}

static gboolean
meta_input_device_x11_query_pointer_location (MetaInputDeviceX11 *device_xi2)
{
  ClutterInputDevice *device = CLUTTER_INPUT_DEVICE (device_xi2);
  ClutterSeat *seat = clutter_input_device_get_seat (device);
  MetaSeatX11 *seat_x11 = META_SEAT_X11 (seat);
  MetaBackendX11 *backend_x11 =
    META_BACKEND_X11 (meta_seat_x11_get_backend (seat_x11));
  Display *xdisplay = meta_backend_x11_get_xdisplay (backend_x11);
  Window xroot_window, xchild_window;
  double xroot_x, xroot_y, xwin_x, xwin_y;
  XIButtonState button_state = { 0 };
  XIModifierState mod_state;
  XIGroupState group_state;
  int result;

  mtk_x11_error_trap_push (xdisplay);
  result = XIQueryPointer (meta_backend_x11_get_xdisplay (backend_x11),
                           device_xi2->device_id,
                           meta_backend_x11_get_root_xwindow (backend_x11),
                           &xroot_window,
                           &xchild_window,
                           &xroot_x, &xroot_y,
                           &xwin_x, &xwin_y,
                           &button_state,
                           &mod_state,
                           &group_state);
  mtk_x11_error_trap_pop (xdisplay);

  g_free (button_state.mask);

  if (!result)
    return FALSE;

  device_xi2->current_x = (float) xroot_x;
  device_xi2->current_y = (float) xroot_y;

  return TRUE;
}

static void
clear_inhibit_pointer_query_cb (gpointer data)
{
  MetaInputDeviceX11 *device_xi2 = META_INPUT_DEVICE_X11 (data);

  device_xi2->inhibit_pointer_query_timer = 0;
}

gboolean
meta_input_device_x11_get_pointer_location (ClutterInputDevice *device,
                                            float              *x,
                                            float              *y)

{
  MetaInputDeviceX11 *device_xi2 = META_INPUT_DEVICE_X11 (device);

  g_return_val_if_fail (META_IS_INPUT_DEVICE_X11 (device), FALSE);
  g_return_val_if_fail (clutter_input_device_get_device_type (device) ==
                        CLUTTER_POINTER_DEVICE, FALSE);

  /* Throttle XServer queries and roundtrips using an idle timeout */
  if (device_xi2->inhibit_pointer_query_timer == 0)
    {
      device_xi2->query_status =
        meta_input_device_x11_query_pointer_location (device_xi2);
      device_xi2->inhibit_pointer_query_timer =
        g_idle_add_once (clear_inhibit_pointer_query_cb, device_xi2);
    }

  *x = device_xi2->current_x;
  *y = device_xi2->current_y;

  return device_xi2->query_status;
}

int
meta_input_device_x11_get_device_id (ClutterInputDevice *device)
{
  MetaInputDeviceX11 *device_xi2 = META_INPUT_DEVICE_X11 (device);

  g_return_val_if_fail (META_IS_INPUT_DEVICE_X11 (device), 0);

  return device_xi2->device_id;
}

void
meta_input_device_x11_reset_axes (ClutterInputDevice *device)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (device);

  g_clear_pointer (&device_x11->axes, g_array_unref);
}

int
meta_input_device_x11_add_axis (ClutterInputDevice *device,
                                ClutterInputAxis    axis,
                                double              minimum,
                                double              maximum,
                                double              resolution)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (device);
  MetaX11AxisInfo info;
  guint pos;

  if (device_x11->axes == NULL)
    device_x11->axes = g_array_new (FALSE, TRUE, sizeof (MetaX11AxisInfo));

  info.axis = axis;
  info.min_value = minimum;
  info.max_value = maximum;
  info.resolution = resolution;

  switch (axis)
    {
    case CLUTTER_INPUT_AXIS_X:
    case CLUTTER_INPUT_AXIS_Y:
      info.min_axis = 0;
      info.max_axis = 0;
      break;

    case CLUTTER_INPUT_AXIS_XTILT:
    case CLUTTER_INPUT_AXIS_YTILT:
      info.min_axis = -1;
      info.max_axis = 1;
      break;

    default:
      info.min_axis = 0;
      info.max_axis = 1;
      break;
    }

  g_array_append_val (device_x11->axes, info);
  pos = device_x11->axes->len - 1;

  return pos;
}

gboolean
meta_input_device_x11_get_axis (ClutterInputDevice *device,
                                int                 idx,
                                ClutterInputAxis   *use)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (device);
  MetaX11AxisInfo *info;

  if (device_x11->axes == NULL)
    return FALSE;

  if (idx < 0 || idx >= device_x11->axes->len)
    return FALSE;

  info = &g_array_index (device_x11->axes, MetaX11AxisInfo, idx);

  if (use)
    *use = info->axis;

  return TRUE;
}

gboolean
meta_input_device_x11_translate_axis (ClutterInputDevice *device,
                                      int                 idx,
                                      double              value,
                                      double             *axis_value)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (device);
  MetaX11AxisInfo *info;
  double width;
  double real_value;

  if (device_x11->axes == NULL || idx < 0 || idx >= device_x11->axes->len)
    return FALSE;

  info = &g_array_index (device_x11->axes, MetaX11AxisInfo, idx);

  if (info->axis == CLUTTER_INPUT_AXIS_X ||
      info->axis == CLUTTER_INPUT_AXIS_Y)
    return FALSE;

  if (fabs (info->max_value - info->min_value) < 0.0000001)
    return FALSE;

  width = info->max_value - info->min_value;
  real_value = (info->max_axis * (value - info->min_value)
             + info->min_axis * (info->max_value - value))
             / width;

  if (axis_value)
    *axis_value = real_value;

  return TRUE;
}

int
meta_input_device_x11_get_n_axes (ClutterInputDevice *device)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (device);

  return device_x11->axes->len;
}

void
meta_input_device_x11_add_scroll_info (ClutterInputDevice     *device,
                                       int                     idx,
                                       ClutterScrollDirection  direction,
                                       double                  increment)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (device);
  MetaX11ScrollInfo info;

  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));

  info.axis_id = idx;
  info.direction = direction;
  info.increment = increment;
  info.last_value_valid = FALSE;

  if (device_x11->scroll_info == NULL)
    {
      device_x11->scroll_info = g_array_new (FALSE,
                                             FALSE,
                                             sizeof (MetaX11ScrollInfo));
    }

  g_array_append_val (device_x11->scroll_info, info);
}

gboolean
meta_input_device_x11_get_scroll_delta (ClutterInputDevice     *device,
                                        int                     idx,
                                        double                  value,
                                        ClutterScrollDirection *direction_p,
                                        double                 *delta_p)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (device);
  int i;

  if (device_x11->scroll_info == NULL)
    return FALSE;

  for (i = 0; i < device_x11->scroll_info->len; i++)
    {
      MetaX11ScrollInfo *info = &g_array_index (device_x11->scroll_info,
                                                MetaX11ScrollInfo,
                                                i);

      if (info->axis_id == idx)
        {
          if (direction_p != NULL)
            *direction_p = info->direction;

          if (delta_p != NULL)
            *delta_p = 0.0;

          if (info->last_value_valid)
            {
              if (delta_p != NULL)
                {
                  *delta_p = (value - info->last_value)
                           / info->increment;
                }

              info->last_value = value;
            }
          else
            {
              info->last_value = value;
              info->last_value_valid = TRUE;
            }

          return TRUE;
        }
    }

  return FALSE;
}

void
meta_input_device_x11_reset_scroll_info (ClutterInputDevice *device)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (device);
  int i;

  if (device_x11->scroll_info == NULL)
    return;

  for (i = 0; i < device_x11->scroll_info->len; i++)
    {
      MetaX11ScrollInfo *info = &g_array_index (device_x11->scroll_info,
                                                MetaX11ScrollInfo,
                                                i);

      info->last_value_valid = FALSE;
    }
}

#ifdef HAVE_LIBWACOM
uint32_t
meta_input_device_x11_get_pad_group_mode (ClutterInputDevice *device,
                                          uint32_t            group)
{
  MetaInputDeviceX11 *device_xi2 = META_INPUT_DEVICE_X11 (device);

  if (group >= device_xi2->group_modes->len)
    return 0;

  return g_array_index (device_xi2->group_modes, uint32_t, group);
}

static gboolean
pad_switch_mode (ClutterInputDevice *device,
                 uint32_t            button,
                 uint32_t            group,
                 uint32_t           *mode)
{
  MetaInputDeviceX11 *device_x11 = META_INPUT_DEVICE_X11 (device);
  uint32_t n_buttons, n_modes, button_group, next_mode, i;
  WacomDevice *wacom_device;
  GList *switch_buttons = NULL;

  wacom_device =
    meta_input_device_get_wacom_device (META_INPUT_DEVICE (device));

  if (!wacom_device)
    return FALSE;

  n_buttons = libwacom_get_num_buttons (wacom_device);

  for (i = 0; i < n_buttons; i++)
    {
      button_group = meta_input_device_x11_get_button_group (device, i);
      if (button_group == group)
        switch_buttons = g_list_prepend (switch_buttons, GINT_TO_POINTER (button));
    }

  switch_buttons = g_list_reverse (switch_buttons);
  n_modes = clutter_input_device_get_group_n_modes (device, group);

  if (g_list_length (switch_buttons) > 1)
    {
      /* If there's multiple switch buttons, we don't toggle but assign a mode
       * to each of those buttons.
       */
      next_mode = g_list_index (switch_buttons, GINT_TO_POINTER (button));
    }
  else if (switch_buttons)
    {
      uint32_t cur_mode;

      /* If there is a single button, have it toggle across modes */
      cur_mode = g_array_index (device_x11->group_modes, uint32_t, group);
      next_mode = (cur_mode + 1) % n_modes;
    }
  else
    {
      return FALSE;
    }

  g_list_free (switch_buttons);

  if (next_mode < 0 || next_mode > n_modes)
    return FALSE;

  *mode = next_mode;
  return TRUE;
}

void
meta_input_device_x11_update_pad_state (ClutterInputDevice *device,
                                        uint32_t            button,
                                        uint32_t            state,
                                        uint32_t           *group,
                                        uint32_t           *mode)
{
  MetaInputDeviceX11 *device_xi2 = META_INPUT_DEVICE_X11 (device);
  uint32_t button_group, *group_mode;

  button_group = meta_input_device_x11_get_button_group (device, button);

  if (button_group < 0 || button_group >= device_xi2->group_modes->len)
    {
      if (group)
        *group = 0;
      if (mode)
        *mode = 0;
      return;
    }

  group_mode = &g_array_index (device_xi2->group_modes, uint32_t, button_group);

  if (state)
    {
      uint32_t next_mode;

      if (pad_switch_mode (device, button, button_group, &next_mode))
        *group_mode = next_mode;
    }

  if (group)
    *group = button_group;
  if (mode)
    *mode = *group_mode;
}
#endif
