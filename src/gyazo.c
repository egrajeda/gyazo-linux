#include <gtk/gtk.h>
#include <gdk/gdkx.h>

static void
select_area_button_press (XKeyEvent    *event,
                          GdkRectangle *rect,
                          GdkRectangle *draw_rect)
{
  rect->x = event->x_root;
  rect->y = event->y_root;

  draw_rect->x = rect->x;
  draw_rect->y = rect->y;
  draw_rect->width  = 0;
  draw_rect->height = 0;
}

static void
select_area_button_release (XKeyEvent    *event,
                            GdkRectangle *rect,
                            GdkRectangle *draw_rect,
                            GdkWindow    *root,
                            GdkGC        *gc)
{
  /* remove the old rectangle */
  if (draw_rect->width > 0 && draw_rect->height > 0)
    gdk_draw_rectangle (root, gc, FALSE, 
                        draw_rect->x, draw_rect->y,
                        draw_rect->width, draw_rect->height);

  rect->width  = ABS (rect->x - event->x_root);
  rect->height = ABS (rect->y - event->y_root);

  rect->x = MIN (rect->x, event->x_root);
  rect->y = MIN (rect->y, event->y_root);
}

static void
select_area_motion_notify (XKeyEvent    *event,
                           GdkRectangle *rect,
                           GdkRectangle *draw_rect,
                           GdkWindow    *root,
                           GdkGC        *gc)
{
  /* FIXME: draw some nice rubberband with cairo if composited */

  /* remove the old rectangle */
  if (draw_rect->width > 0 && draw_rect->height > 0)
    gdk_draw_rectangle (root, gc, FALSE, 
                        draw_rect->x, draw_rect->y,
                        draw_rect->width, draw_rect->height);

  draw_rect->width  = ABS (rect->x - event->x_root);
  draw_rect->height = ABS (rect->y - event->y_root);

  draw_rect->x = MIN (rect->x, event->x_root);
  draw_rect->y = MIN (rect->y, event->y_root);

  /* draw the new rectangle */
  if (draw_rect->width > 0 && draw_rect->height > 0)
    gdk_draw_rectangle (root, gc, FALSE, 
                        draw_rect->x, draw_rect->y,
                        draw_rect->width, draw_rect->height);
}

typedef struct {
  GdkRectangle  rect;
  GdkRectangle  draw_rect;
  gboolean      button_pressed;
  /* only needed because we're not using cairo to draw the rectangle */
  GdkWindow    *root;
  GdkGC        *gc;
} select_area_filter_data;

static GdkFilterReturn
select_area_filter (GdkXEvent *gdk_xevent,
                    GdkEvent  *event,
                    gpointer   user_data)
{
  select_area_filter_data *data = user_data;
  XEvent *xevent = (XEvent *) gdk_xevent;

  switch (xevent->type)
    {
    case ButtonPress:
      if (!data->button_pressed)
        {
          select_area_button_press (&xevent->xkey,
                                    &data->rect, &data->draw_rect);
          data->button_pressed = TRUE;
        }
      return GDK_FILTER_REMOVE;
    case ButtonRelease:
      if (data->button_pressed)
      {
        select_area_button_release (&xevent->xkey,
                                    &data->rect, &data->draw_rect,
                                    data->root, data->gc);
        gtk_main_quit ();
      }
      return GDK_FILTER_REMOVE;
    case MotionNotify:
      if (data->button_pressed)
        select_area_motion_notify (&xevent->xkey,
                                   &data->rect, &data->draw_rect,
                                   data->root, data->gc);
      return GDK_FILTER_REMOVE;
    case KeyPress:
      if (xevent->xkey.keycode == XKeysymToKeycode (gdk_display, XK_Escape))
        {
          data->rect.x = 0;
          data->rect.y = 0;
          data->rect.width  = 0;
          data->rect.height = 0;
          gtk_main_quit ();
          return GDK_FILTER_REMOVE;
        }
      break;
    default:
      break;
    }
 
  return GDK_FILTER_CONTINUE;
}

GdkPixbuf *
screenshot_get_pixbuf (GdkRectangle *rectangle)
{
  GdkWindow *root;
  GdkPixbuf *screenshot;
  gint x_orig, y_orig;
  gint width, height;

  root = gdk_get_default_root_window ();

  x_orig = rectangle->x;
  y_orig = rectangle->y;
  width  = rectangle->width;
  height = rectangle->height;
  
  screenshot = gdk_pixbuf_get_from_drawable (NULL, root, NULL,
                                             x_orig, y_orig, 0, 0,
                                             width, height);
  return screenshot;
}

gboolean
screenshot_select_area (int *px,
                        int *py,
                        int *pwidth,
                        int *pheight)
{
  GdkWindow               *root;
  GdkCursor               *cursor;
  select_area_filter_data  data;
  GdkGCValues              values;
  GdkColor                 color;

  root = gdk_get_default_root_window ();
  cursor = gdk_cursor_new (GDK_CROSSHAIR);

  if (gdk_pointer_grab (root, FALSE,
                        GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK,
                        NULL, cursor,
                        GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
    {
      gdk_cursor_unref (cursor);
      return FALSE;
    }

  if (gdk_keyboard_grab (root, FALSE, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
    {
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
      gdk_cursor_unref (cursor);
      return FALSE;
    }

  gdk_window_add_filter (root, (GdkFilterFunc) select_area_filter, &data);

  gdk_flush ();

  data.rect.x = 0;
  data.rect.y = 0;
  data.rect.width  = 0;
  data.rect.height = 0;
  data.button_pressed = FALSE;
  data.root = root;

  values.function = GDK_XOR;
  values.fill = GDK_SOLID;
  values.clip_mask = NULL;
  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  values.clip_x_origin = 0;
  values.clip_y_origin = 0;
  values.graphics_exposures = 0;
  values.line_width = 0;
  values.line_style = GDK_LINE_SOLID;
  values.cap_style = GDK_CAP_BUTT;
  values.join_style = GDK_JOIN_MITER;

  data.gc = gdk_gc_new_with_values (root, &values,
                                    GDK_GC_FUNCTION | GDK_GC_FILL |
                                    GDK_GC_CLIP_MASK | GDK_GC_SUBWINDOW |
                                    GDK_GC_CLIP_X_ORIGIN |
                                    GDK_GC_CLIP_Y_ORIGIN | GDK_GC_EXPOSURES |
                                    GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE |
                                    GDK_GC_CAP_STYLE | GDK_GC_JOIN_STYLE);
  gdk_color_parse ("white", &color);
  gdk_gc_set_rgb_fg_color (data.gc, &color);
  gdk_color_parse ("black", &color);
  gdk_gc_set_rgb_bg_color (data.gc, &color);

  gtk_main ();

  g_object_unref (data.gc);

  gdk_window_remove_filter (root, (GdkFilterFunc) select_area_filter, &data);

  gdk_keyboard_ungrab (GDK_CURRENT_TIME);
  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gdk_cursor_unref (cursor);

  *px = data.rect.x;
  *py = data.rect.y;
  *pwidth  = data.rect.width;
  *pheight = data.rect.height;

  return TRUE;
}

int
main (int    argc,
      char** argv)
{
    GdkRectangle *rectangle;
    GdkPixbuf    *screenshot;

    gtk_init (&argc, &argv);

    rectangle = g_new0 (GdkRectangle, 1);
    screenshot_select_area (&rectangle->x, &rectangle->y, 
                            &rectangle->width, &rectangle->height);

    screenshot = screenshot_get_pixbuf (rectangle);
    gdk_pixbuf_savev (screenshot, "/tmp/foo.png", "png", NULL, NULL, NULL);

    return 0;
}
