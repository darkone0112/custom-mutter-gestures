/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2009 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <atk/atk.h>
#include <clutter/clutter.h>
#include <clutter/clutter-pango.h>

#include "cally-examples-util.h"

#define WIDTH 800
#define HEIGHT 600

static ClutterActor *text_actor = NULL;
static ClutterActor *text_editable_actor = NULL;

/*
 * Test AtkText interface
 */
static void
test_atk_text (ClutterActor *actor)
{
  AtkObject       *object              = NULL;
  AtkEditableText *cally_editable_text = NULL;
  gint             pos                 = 0;

  object = atk_gobject_accessible_for_object (G_OBJECT (actor));
  cally_editable_text = ATK_EDITABLE_TEXT (object);

  if (cally_editable_text != NULL) {
    atk_editable_text_set_text_contents (cally_editable_text, "New text");
    atk_editable_text_delete_text (cally_editable_text, 0, 3);
    pos = 3;
    atk_editable_text_insert_text (cally_editable_text, "New", 0, &pos);

    /* Not implemented in cally, just checking that we can call this
       functions */
    atk_editable_text_copy_text (cally_editable_text, 0, -1);
    atk_editable_text_paste_text (cally_editable_text, 5);
    atk_editable_text_cut_text (cally_editable_text, 0, -1);
  }
}

static gboolean
insert_text_press_cb (ClutterActor *actor,
                      ClutterButtonEvent *event,
                      gpointer data)
{
  AtkObject       *object              = NULL;
  AtkEditableText *cally_editable_text = NULL;
  gint             pos                 = 0;

  object = atk_gobject_accessible_for_object (G_OBJECT (text_editable_actor));
  cally_editable_text = ATK_EDITABLE_TEXT (object);

  pos = 3;
  atk_editable_text_insert_text (cally_editable_text, "New", 0, &pos);

  return TRUE;
}

static gboolean
delete_text_press_cb (ClutterActor *actor,
                      ClutterButtonEvent *event,
                      gpointer data)
{
  AtkObject       *object              = NULL;
  AtkEditableText *cally_editable_text = NULL;

  object = atk_gobject_accessible_for_object (G_OBJECT (text_editable_actor));
  cally_editable_text = ATK_EDITABLE_TEXT (object);

  atk_editable_text_delete_text (cally_editable_text, 0, 3);

  return TRUE;
}

static gboolean
set_text_press_cb (ClutterActor *actor,
                   ClutterButtonEvent *event,
                   gpointer data)
{
  AtkObject       *object              = NULL;
  AtkEditableText *cally_editable_text = NULL;

  object = atk_gobject_accessible_for_object (G_OBJECT (text_editable_actor));
  cally_editable_text = ATK_EDITABLE_TEXT (object);

  atk_editable_text_set_text_contents (cally_editable_text, "New text");

  return TRUE;
}

static gboolean
activate_deactivate_press_cb (ClutterActor *actor,
                              ClutterButtonEvent *event,
                              gpointer data)
{
  gboolean active = FALSE;

  active = clutter_text_get_activatable (CLUTTER_TEXT (text_editable_actor));
  clutter_text_set_activatable (CLUTTER_TEXT (text_editable_actor), !active);

  return TRUE;
}

static gboolean
print_cursor_position_press_cb (ClutterActor *actor,
                                ClutterButtonEvent *event,
                                gpointer data)
{
  gint pos = 0;

  pos =  clutter_text_get_cursor_position (CLUTTER_TEXT (text_editable_actor));

  g_print ("current cursor position %i\n", pos);

  return TRUE;
}

static void
activate_cb (ClutterActor *actor,
             gpointer data)
{
  g_print ("Actor activated\n");
}

static ClutterActor*
_create_button (const gchar *text)
{
  ClutterActor *button     = NULL;
  ClutterActor *rectangle  = NULL;
  ClutterActor *label      = NULL;

  button = clutter_actor_new ();
  rectangle = clutter_actor_new ();
  clutter_actor_set_background_color (rectangle, &COGL_COLOR_INIT (255, 0, 255, 255));
  clutter_actor_set_size (rectangle, 375, 35);

  label = clutter_text_new_full ("Sans Bold 32px",
                                 text,
                                 &COGL_COLOR_INIT (0, 0, 0, 255));
  clutter_actor_add_child (button, rectangle);
  clutter_actor_add_child (button, label);
  clutter_actor_set_reactive (button, TRUE);

  return button;
}

static void
make_ui (ClutterActor *stage)
{
  ClutterActor *button      = NULL;

  clutter_actor_set_background_color (CLUTTER_ACTOR (stage),
                                      &COGL_COLOR_INIT (255, 255, 255, 255));
  clutter_actor_set_size (stage, WIDTH, HEIGHT);

  /* text */
  text_actor = clutter_text_new_full ("Sans Bold 32px",
                                      "Lorem ipsum dolor sit amet",
                                      &COGL_COLOR_INIT (255, 0, 0, 255));
  clutter_actor_add_child (stage, text_actor);

  /* text_editable */
  text_editable_actor = clutter_text_new_full ("Sans Bold 32px",
                                               "consectetur adipisicing elit",
                                               &COGL_COLOR_INIT (255, 0, 0, 255));
  clutter_actor_set_position (text_editable_actor, 0, 100);
  clutter_text_set_editable (CLUTTER_TEXT (text_editable_actor), TRUE);
  clutter_text_set_selectable (CLUTTER_TEXT (text_editable_actor), TRUE);
  clutter_text_set_selection_color (CLUTTER_TEXT (text_editable_actor),
                                    &COGL_COLOR_INIT (0, 255, 0, 255));
  clutter_text_set_activatable (CLUTTER_TEXT (text_editable_actor),
                                TRUE);
  clutter_text_set_line_wrap (CLUTTER_TEXT (text_editable_actor), TRUE);
  clutter_actor_grab_key_focus (text_editable_actor);
  clutter_actor_set_reactive (text_editable_actor, TRUE);

  clutter_actor_add_child (stage, text_editable_actor);
  g_signal_connect (text_editable_actor, "activate",
                    G_CALLBACK (activate_cb), NULL);

  /* test buttons */
  button = _create_button ("Set");
  clutter_actor_set_position (button, 100, 200);

  g_signal_connect_after (button, "button-press-event",
                          G_CALLBACK (set_text_press_cb), NULL);

  clutter_actor_add_child (stage, button);

  button = _create_button ("Delete");
  clutter_actor_set_position (button, 100, 250);

  g_signal_connect_after (button, "button-press-event",
                          G_CALLBACK (delete_text_press_cb), NULL);

  clutter_actor_add_child (stage, button);

  button = _create_button ("Insert");
  clutter_actor_set_position (button, 100, 300);

  g_signal_connect_after (button, "button-press-event",
                          G_CALLBACK (insert_text_press_cb), NULL);

  clutter_actor_add_child (stage, button);

  button = _create_button ("Activate/Deactivate");
  clutter_actor_set_position (button, 100, 350);

  g_signal_connect_after (button, "button-press-event",
                          G_CALLBACK (activate_deactivate_press_cb), NULL);

  clutter_actor_add_child (stage, button);

  button = _create_button ("Cursor position");
  clutter_actor_set_position (button, 100, 450);

  g_signal_connect_after (button, "button-press-event",
                          G_CALLBACK (print_cursor_position_press_cb), NULL);

  clutter_actor_add_child (stage, button);

}

int
main (int argc, char *argv[])
{
  ClutterActor *stage         = NULL;

  g_set_application_name ("AtkEditableText");

  cally_util_a11y_init (&argc, &argv);

  stage = clutter_test_get_stage ();
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_test_quit), NULL);

  make_ui (stage);

  clutter_actor_show (stage);

  test_atk_text (text_actor);
  test_atk_text (text_editable_actor);

  clutter_test_main ();

  return 0;
}
