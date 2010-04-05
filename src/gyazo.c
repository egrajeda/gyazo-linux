#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <curl/curl.h>

static size_t
on_image_uploaded (void   *buffer,
                   size_t  size,
                   size_t  nmemb,
                   void   *userp)
{
  char  command[1024];
  sprintf (command, "xdg-open %s", buffer);
  system (command);
}

static void
upload_image (const char *filename)
{
  CURL                 *handle;
  struct curl_httppost *post = NULL, *last = NULL;

  handle = curl_easy_init ();

  curl_easy_setopt (handle, CURLOPT_PROXY, "http://proxy.udb.edu.sv");
  curl_easy_setopt (handle, CURLOPT_PROXYPORT, 8080);
  curl_easy_setopt (handle, CURLOPT_URL, "http://gyazo.com/upload.cgi");
  curl_easy_setopt (handle, CURLOPT_WRITEFUNCTION, on_image_uploaded);

  curl_formadd (&post, &last, CURLFORM_COPYNAME, "id",
                CURLFORM_COPYCONTENTS, "123", CURLFORM_END);
  curl_formadd (&post, &last, CURLFORM_COPYNAME, "imagedata",
                CURLFORM_FILE, filename, CURLFORM_END);
  curl_easy_setopt (handle, CURLOPT_HTTPPOST, post);

  curl_easy_perform (handle);
  curl_easy_cleanup (handle);
}

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

static GdkPixbuf *
get_screenshot_rectangle (GdkRectangle *rectangle)
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

static GdkRectangle *
select_area ()
{
  GdkWindow               *root;
  GdkCursor               *cursor;
  select_area_filter_data  data;
  GdkGCValues              values;
  GdkColor                 color;
  GdkRectangle            *rectangle;

  root   = gdk_get_default_root_window ();
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

  rectangle = g_new0 (GdkRectangle, 1);
  rectangle->x = data.rect.x;
  rectangle->y = data.rect.y;
  rectangle->width  = data.rect.width + 1;
  rectangle->height = data.rect.height + 1;

  return rectangle;
}

/* -- Screenshot -- */



/* -- Status icon -- */

static void
on_status_icon_activate ()
{
  GdkRectangle *rectangle;
  GdkPixbuf    *screenshot;

  rectangle  = select_area ();
  screenshot = get_screenshot_rectangle (rectangle);

  gdk_pixbuf_savev (screenshot, "/tmp/.gyazo.png", "png", NULL, NULL, NULL);
  g_free (rectangle);
  
  upload_image ("/tmp/.gyazo.png");
}

static void
create_status_icon ()
{
  GtkStatusIcon *icon;

  icon = gtk_status_icon_new_from_file ("gyazo.png");
  g_signal_connect (G_OBJECT (icon), "activate", 
                    G_CALLBACK (on_status_icon_activate), NULL);
}

/* -- Main -- */

int
main (int    argc,
      char** argv)
{
  gtk_init (&argc, &argv);

/*  create_status_icon ();

  gtk_main ();
*/
  GdkRectangle *rectangle;
  GdkPixbuf    *screenshot;

  rectangle  = select_area ();
  screenshot = get_screenshot_rectangle (rectangle);

  gdk_pixbuf_savev (screenshot, "/tmp/.gyazo.png", "png", NULL, NULL, NULL);
  g_free (rectangle);
  
  upload_image ("/tmp/.gyazo.png");

  return 0;
}
