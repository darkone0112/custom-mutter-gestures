/*
 * Wayland Support
 *
 * Copyright (C) 2016 Red Hat
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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <wayland-server.h>

#include "backends/meta-input-settings-private.h"
#include "core/display-private.h"
#include "compositor/meta-surface-actor-wayland.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-tablet-pad-group.h"
#include "wayland/meta-wayland-tablet-pad-ring.h"
#include "wayland/meta-wayland-tablet-pad-strip.h"
#include "wayland/meta-wayland-tablet-pad-dial.h"
#include "wayland/meta-wayland-tablet-pad.h"
#include "wayland/meta-wayland-tablet-seat.h"
#include "wayland/meta-wayland-tablet.h"

#include "tablet-v2-server-protocol.h"

static MetaDisplay *
display_from_pad (MetaWaylandTabletPad *pad)
{
  MetaWaylandCompositor *compositor =
    meta_wayland_seat_get_compositor (pad->tablet_seat->seat);
  MetaContext *context = meta_wayland_compositor_get_context (compositor);

  return meta_context_get_display (context);
}

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
}

static void
pad_handle_focus_surface_destroy (struct wl_listener *listener,
                                  void               *data)
{
  MetaWaylandTabletPad *pad = wl_container_of (listener, pad, focus_surface_listener);

  meta_wayland_tablet_pad_set_focus (pad, NULL);
}

static void
group_rings_strips_dials (MetaWaylandTabletPad *pad)
{
  gint n_group, n_elem;
  GList *g, *l;

  for (n_group = 0, g = pad->groups; g; g = g->next)
    {
      MetaWaylandTabletPadGroup *group = g->data;

      for (n_elem = 0, l = pad->rings; l; l = l->next)
        {
          MetaWaylandTabletPadRing *ring = l->data;

          if (clutter_input_device_get_pad_feature_group (pad->device,
                                                          CLUTTER_PAD_FEATURE_RING,
                                                          n_elem) == n_group)
            meta_wayland_tablet_pad_ring_set_group (ring, group);

          n_elem++;
        }

      for (n_elem = 0, l = pad->strips; l; l = l->next)
        {
          MetaWaylandTabletPadStrip *strip = l->data;

          if (clutter_input_device_get_pad_feature_group (pad->device,
                                                          CLUTTER_PAD_FEATURE_STRIP,
                                                          n_elem) == n_group)
            meta_wayland_tablet_pad_strip_set_group (strip, group);

          n_elem++;
        }

      for (n_elem = 0, l = pad->dials; l; l = l->next)
        {
          MetaWaylandTabletPadDial *dial = l->data;

          if (clutter_input_device_get_pad_feature_group (pad->device,
                                                          CLUTTER_PAD_FEATURE_DIAL,
                                                          n_elem) == n_group)
            meta_wayland_tablet_pad_dial_set_group (dial, group);

          n_elem++;
        }

      n_group++;
    }
}

MetaWaylandTabletPad *
meta_wayland_tablet_pad_new (ClutterInputDevice    *device,
                             MetaWaylandTabletSeat *tablet_seat)
{
  MetaWaylandTabletPad *pad;
  guint n_elems, i;

  pad = g_new0 (MetaWaylandTabletPad, 1);
  wl_list_init (&pad->resource_list);
  wl_list_init (&pad->focus_resource_list);
  pad->focus_surface_listener.notify = pad_handle_focus_surface_destroy;
  pad->device = device;
  pad->tablet_seat = tablet_seat;

  pad->feedback = g_hash_table_new_full (NULL, NULL, NULL,
                                         (GDestroyNotify) g_free);

  pad->n_buttons = clutter_input_device_get_n_buttons (device);

  n_elems = clutter_input_device_get_n_mode_groups (pad->device);

  for (i = 0; i < n_elems; i++)
    {
      pad->groups = g_list_prepend (pad->groups,
                                    meta_wayland_tablet_pad_group_new (pad));
    }

  n_elems = clutter_input_device_get_n_rings (pad->device);

  for (i = 0; i < n_elems; i++)
    {
      MetaWaylandTabletPadRing *ring;

      ring = meta_wayland_tablet_pad_ring_new (pad);
      pad->rings = g_list_prepend (pad->rings, ring);
    }

  n_elems = clutter_input_device_get_n_strips (pad->device);

  for (i = 0; i < n_elems; i++)
    {
      MetaWaylandTabletPadStrip *strip;

      strip = meta_wayland_tablet_pad_strip_new (pad);
      pad->strips = g_list_prepend (pad->strips, strip);
    }

  n_elems = clutter_input_device_get_n_dials (pad->device);

  for (i = 0; i < n_elems; i++)
    {
      MetaWaylandTabletPadDial *dial;

      dial = meta_wayland_tablet_pad_dial_new (pad);
      pad->dials = g_list_prepend (pad->dials, dial);
    }

  group_rings_strips_dials (pad);

  return pad;
}

void
meta_wayland_tablet_pad_free (MetaWaylandTabletPad *pad)
{
  struct wl_resource *resource, *next;

  meta_wayland_tablet_pad_set_focus (pad, NULL);

  wl_resource_for_each_safe (resource, next, &pad->resource_list)
    {
      zwp_tablet_pad_v2_send_removed (resource);
      wl_list_remove (wl_resource_get_link (resource));
      wl_list_init (wl_resource_get_link (resource));
    }

  g_list_free_full (pad->groups,
                    (GDestroyNotify) meta_wayland_tablet_pad_group_free);
  g_list_free_full (pad->rings,
                    (GDestroyNotify) meta_wayland_tablet_pad_ring_free);
  g_list_free_full (pad->strips,
                    (GDestroyNotify) meta_wayland_tablet_pad_strip_free);
  g_list_free_full (pad->dials,
                    (GDestroyNotify) meta_wayland_tablet_pad_dial_free);

  g_hash_table_destroy (pad->feedback);

  g_free (pad);
}

static MetaWaylandTabletPadGroup *
tablet_pad_lookup_button_group (MetaWaylandTabletPad *pad,
                                guint                 button)
{
  GList *l;

  for (l = pad->groups; l; l = l->next)
    {
      MetaWaylandTabletPadGroup *group = l->data;

      if (meta_wayland_tablet_pad_group_has_button (group, button))
        return group;
    }

  return NULL;
}

static void
tablet_pad_set_feedback (struct wl_client   *client,
                         struct wl_resource *resource,
                         uint32_t            button,
                         const char         *str,
                         uint32_t            serial)
{
  MetaWaylandTabletPad *pad = wl_resource_get_user_data (resource);
  MetaWaylandTabletPadGroup *group = tablet_pad_lookup_button_group (pad, button);
  MetaPadActionMapper *mapper;

  if (!group || group->mode_switch_serial != serial)
    return;

  mapper = display_from_pad (pad)->pad_action_mapper;

  if (meta_pad_action_mapper_is_button_grabbed (mapper, pad->device, button))
    return;

  if (meta_wayland_tablet_pad_group_is_mode_switch_button (group, button))
    return;

  g_hash_table_insert (pad->feedback, GUINT_TO_POINTER (button), g_strdup (str));
}

static void
tablet_pad_destroy (struct wl_client   *client,
                    struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_tablet_pad_v2_interface pad_interface = {
  tablet_pad_set_feedback,
  tablet_pad_destroy,
};

void
meta_wayland_tablet_pad_notify (MetaWaylandTabletPad  *pad,
                                struct wl_resource    *resource)
{
  struct wl_client *client = wl_resource_get_client (resource);
  const gchar *node_path;
  GList *l;

  node_path = clutter_input_device_get_device_node (pad->device);
  if (node_path)
    zwp_tablet_pad_v2_send_path (resource, node_path);

  zwp_tablet_pad_v2_send_buttons (resource, pad->n_buttons);

  for (l = pad->groups; l; l = l->next)
    {
      MetaWaylandTabletPadGroup *group = l->data;
      struct wl_resource *group_resource;

      group_resource = meta_wayland_tablet_pad_group_create_new_resource (group,
                                                                          client,
                                                                          resource,
                                                                          0);
      zwp_tablet_pad_v2_send_group (resource, group_resource);
      meta_wayland_tablet_pad_group_notify (group, group_resource);
    }

  zwp_tablet_pad_v2_send_done (resource);
}

struct wl_resource *
meta_wayland_tablet_pad_create_new_resource (MetaWaylandTabletPad *pad,
                                             struct wl_client     *client,
                                             struct wl_resource   *seat_resource,
                                             uint32_t              id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &zwp_tablet_pad_v2_interface,
                                 wl_resource_get_version (seat_resource), id);
  wl_resource_set_implementation (resource, &pad_interface,
                                  pad, unbind_resource);
  wl_resource_set_user_data (resource, pad);
  wl_list_insert (&pad->resource_list, wl_resource_get_link (resource));

  return resource;
}

struct wl_resource *
meta_wayland_tablet_pad_lookup_resource (MetaWaylandTabletPad *pad,
                                         struct wl_client     *client)
{
  struct wl_resource *resource;

  resource = wl_resource_find_for_client (&pad->resource_list, client);

  if (!resource)
    resource = wl_resource_find_for_client (&pad->focus_resource_list, client);

  return resource;
}

static gboolean
handle_pad_button_event (MetaWaylandTabletPad *pad,
                         const ClutterEvent   *event)
{
  enum zwp_tablet_pad_v2_button_state button_state;
  struct wl_list *focus_resources = &pad->focus_resource_list;
  struct wl_resource *resource;
  ClutterEventType event_type;

  if (wl_list_empty (focus_resources))
    return FALSE;

  event_type = clutter_event_type (event);

  if (event_type == CLUTTER_PAD_BUTTON_PRESS)
    button_state = ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED;
  else if (event_type == CLUTTER_PAD_BUTTON_RELEASE)
    button_state = ZWP_TABLET_PAD_V2_BUTTON_STATE_RELEASED;
  else
    return FALSE;

  wl_resource_for_each (resource, focus_resources)
    {
      zwp_tablet_pad_v2_send_button (resource,
                                     clutter_event_get_time (event),
                                     clutter_event_get_button (event),
                                     button_state);
    }

  return TRUE;
}

static gboolean
meta_wayland_tablet_pad_handle_event_action (MetaWaylandTabletPad *pad,
                                             const ClutterEvent   *event)
{
  MetaPadActionMapper *mapper;
  ClutterInputDevice *device;

  device = clutter_event_get_source_device (event);
  mapper = display_from_pad (pad)->pad_action_mapper;

  if (meta_pad_action_mapper_is_button_grabbed (mapper, device,
                                                clutter_event_get_button (event)))
    return TRUE;

  return FALSE;
}

gboolean
meta_wayland_tablet_pad_handle_event (MetaWaylandTabletPad *pad,
                                      const ClutterEvent   *event)
{
  MetaWaylandTabletPadGroup *group;
  gboolean handled = FALSE;
  guint n_group;

  n_group = clutter_event_get_mode_group (event);
  group = g_list_nth_data (pad->groups, n_group);

  switch (clutter_event_type (event))
    {
    case CLUTTER_PAD_BUTTON_PRESS:
    case CLUTTER_PAD_BUTTON_RELEASE:
      if (group)
        handled |= meta_wayland_tablet_pad_group_handle_event (group, event);

      handled |= meta_wayland_tablet_pad_handle_event_action (pad, event);

      if (handled)
        return TRUE;

      return handle_pad_button_event (pad, event);
    case CLUTTER_PAD_RING:
    case CLUTTER_PAD_STRIP:
    case CLUTTER_PAD_DIAL:
      if (group)
        return meta_wayland_tablet_pad_group_handle_event (group, event);
      G_GNUC_FALLTHROUGH;
    default:
      return FALSE;
    }
}

static void
move_resources (struct wl_list *destination, struct wl_list *source)
{
  wl_list_insert_list (destination, source);
  wl_list_init (source);
}

static void
move_resources_for_client (struct wl_list *destination,
			   struct wl_list *source,
			   struct wl_client *client)
{
  struct wl_resource *resource, *tmp;

  wl_resource_for_each_safe (resource, tmp, source)
    {
      if (wl_resource_get_client (resource) == client)
        {
          wl_list_remove (wl_resource_get_link (resource));
          wl_list_insert (destination, wl_resource_get_link (resource));
        }
    }
}

static void
meta_wayland_tablet_pad_update_groups_focus (MetaWaylandTabletPad *pad)
{
  GList *l;

  for (l = pad->groups; l; l = l->next)
    meta_wayland_tablet_pad_group_sync_focus (l->data);
}

static void
meta_wayland_tablet_pad_broadcast_enter (MetaWaylandTabletPad *pad,
                                         uint32_t              serial,
                                         MetaWaylandTablet    *tablet,
                                         MetaWaylandSurface   *surface)
{
  struct wl_resource *resource, *tablet_resource;
  struct wl_client *client;

  client = wl_resource_get_client (pad->focus_surface->resource);
  tablet_resource = meta_wayland_tablet_lookup_resource (tablet, client);

  wl_resource_for_each (resource, &pad->focus_resource_list)
    {
      zwp_tablet_pad_v2_send_enter (resource, serial,
                                    tablet_resource,
                                    surface->resource);
    }
}

static void
meta_wayland_tablet_pad_broadcast_leave (MetaWaylandTabletPad *pad,
                                         uint32_t              serial,
                                         MetaWaylandSurface   *surface)
{
  struct wl_resource *resource;

  wl_resource_for_each (resource, &pad->focus_resource_list)
    {
      zwp_tablet_pad_v2_send_leave (resource, serial,
                                    surface->resource);
    }
}

void
meta_wayland_tablet_pad_set_focus (MetaWaylandTabletPad *pad,
                                   MetaWaylandSurface   *surface)
{
  MetaWaylandTablet *tablet;

  if (pad->focus_surface == surface)
    return;

  g_hash_table_remove_all (pad->feedback);

  if (pad->focus_surface != NULL)
    {
      struct wl_client *client = wl_resource_get_client (pad->focus_surface->resource);
      struct wl_list *focus_resources = &pad->focus_resource_list;

      if (!wl_list_empty (focus_resources))
        {
          struct wl_display *display = wl_client_get_display (client);
          uint32_t serial = wl_display_next_serial (display);

          meta_wayland_tablet_pad_broadcast_leave (pad, serial,
                                                   pad->focus_surface);
          move_resources (&pad->resource_list, &pad->focus_resource_list);
        }

      wl_list_remove (&pad->focus_surface_listener.link);
      pad->focus_surface = NULL;
    }

  tablet = meta_wayland_tablet_seat_lookup_paired_tablet (pad->tablet_seat,
                                                          pad);

  if (tablet != NULL && surface != NULL && surface->resource != NULL)
    {
      struct wl_client *client;

      pad->focus_surface = surface;
      wl_resource_add_destroy_listener (pad->focus_surface->resource,
                                        &pad->focus_surface_listener);

      client = wl_resource_get_client (pad->focus_surface->resource);
      move_resources_for_client (&pad->focus_resource_list,
                                 &pad->resource_list, client);

      if (!wl_list_empty (&pad->focus_resource_list))
        {
          struct wl_display *display = wl_client_get_display (client);

          pad->focus_serial = wl_display_next_serial (display);
          meta_wayland_tablet_pad_broadcast_enter (pad, pad->focus_serial,
                                                   tablet, pad->focus_surface);
        }
    }

  meta_wayland_tablet_pad_update_groups_focus (pad);
}

void
meta_wayland_tablet_pad_update (MetaWaylandTabletPad *pad,
                                const ClutterEvent   *event)
{
  MetaWaylandTabletPadGroup *group;
  guint n_group;

  n_group = clutter_event_get_mode_group (event);
  group = g_list_nth_data (pad->groups, n_group);

  if (group)
    meta_wayland_tablet_pad_group_update (group, event);
}

static gchar *
meta_wayland_tablet_pad_label_mode_switch_button (MetaWaylandTabletPad *pad,
                                                  guint                 button)
{
  MetaWaylandTabletPadGroup *group;
  GList *l;

  for (l = pad->groups; l; l = l->next)
    {
      group = l->data;

      if (meta_wayland_tablet_pad_group_is_mode_switch_button (group, button))
        return g_strdup_printf (_("Mode Switch: Mode %d"), group->current_mode + 1);
    }

  return NULL;
}

char *
meta_wayland_tablet_pad_get_button_label (MetaWaylandTabletPad *pad,
                                          int                   button)
{
  const char *label = NULL;
  char *mode_label;

  mode_label = meta_wayland_tablet_pad_label_mode_switch_button (pad, button);
  if (mode_label)
    return mode_label;

  label = g_hash_table_lookup (pad->feedback, GUINT_TO_POINTER (button));
  return g_strdup (label);
}

char *
meta_wayland_tablet_pad_get_feature_label (MetaWaylandTabletPad *pad,
                                           MetaPadFeatureType    feature,
                                           int                   action)
{
  const char *label = NULL;

  switch (feature)
    {
    case META_PAD_FEATURE_RING:
      {
        MetaWaylandTabletPadRing *ring;

        ring = g_list_nth_data (pad->rings, action);
        if (ring)
          label = ring->feedback;
        break;
      }
    case META_PAD_FEATURE_STRIP:
      {
        MetaWaylandTabletPadStrip *strip;

        strip = g_list_nth_data (pad->strips, action);
        if (strip)
          label = strip->feedback;
        break;
      }
    case META_PAD_FEATURE_DIAL:
      {
        MetaWaylandTabletPadDial *dial;

        dial = g_list_nth_data (pad->dials, action);
        if (dial)
          label = dial->feedback;
        break;
      }
    }

  return g_strdup (label);
}
