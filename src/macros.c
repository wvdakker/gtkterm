/***********************************************************************/
/* macros.c                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Functions for the management of the macros                     */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : file creation by Julien                             */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "widgets.h"
#include "macros.h"

#include <config.h>
#include <glib/gi18n.h>

enum
  {
    COLUMN_SHORTCUT,
    COLUMN_ACTION,
    NUM_COLUMNS
  };

macro_t *macros = NULL;
static GtkWidget *window = NULL;

macro_t *get_shortcuts(gint *size)
{
  gint i = 0;

  if(macros != NULL)
    {
      while(macros[i].shortcut != NULL)
	i++;
    }
  *size = i;
  return macros;
}


static void shortcut_callback(gpointer *number)
{
  gchar *string;
  gchar *str;
  gint i, length;
  guchar a;
  guint val_read;

  string = macros[(long)number].action;
  length = strlen(string);

  for(i = 0; i < length; i++)
    {
      if(string[i] == '\\')
        {
	  if(g_unichar_isdigit((gunichar)string[i + 1]))
	    {
	      if((string[i + 1] == '0') && (string[i + 2] != 0))
		{
		  if(g_unichar_isxdigit((gunichar)string[i + 3]))
		    {
		      str = &string[i + 2];
		      i += 3;
		    }
		  else
		    {
		      str = &string[i + 1];
		      if(g_unichar_isxdigit((gunichar)string[i + 2]))
			i += 2;
		      else
			i++;
		    }
		}
	      else
		{
		  str = &string[i + 1];
		  if(g_unichar_isxdigit((gunichar)string[i + 2]))
		    i += 2;
		  else
		    i++;
		}
	      if(sscanf(str, "%02X", &val_read) == 1)
		a = (guchar)val_read;
	      else
		a = '\\';
	    }
	  else
	    {
	      switch(string[i + 1])
		{
		case 'a':
		  a = '\a';
		  break;
		case 'b':
		  a = '\b';
		  break;
		case 't':
		  a = '\t';
		  break;
		case 'n':
		  a = '\n';
		  break;
		case 'v':
		  a = '\v';
		  break;
		case 'f':
		  a = '\f';
		  break;
		case 'r':
		  a = '\r';
		  break;
		case '\\':
		  a = '\\';
		  break;
		default:
		  a = '\\';
		  i--;
		  break;
		}
	      i++;
	    }
	  send_serial((gchar*)&a, 1);
	}
      else
	{
	  send_serial(&string[i], 1);
	}
    }

  str = g_strdup_printf(_("Macro \"%s\" sent !"), macros[(long)number].shortcut);
  Put_temp_message(str, 800);
  g_free(str);
}

void create_shortcuts(macro_t *macro, gint size)
{
  macros = g_malloc((size + 1) * sizeof(macro_t));
  if(macros != NULL)
    {
      memcpy(macros, macro, size * sizeof(macro_t));
      macros[size].shortcut = NULL;
      macros[size].action = NULL;
    }
  else
    perror("malloc");
}

void add_shortcuts(void)
{
  long i = 0;
  guint acc_key;
  GdkModifierType mod;

  if(macros == NULL)
    return;

  while(macros[i].shortcut != NULL)
    {
      macros[i].closure = g_cclosure_new_swap(G_CALLBACK(shortcut_callback), (gpointer)i, NULL);
      gtk_accelerator_parse(macros[i].shortcut, &acc_key, &mod);
      if(acc_key != 0)
	gtk_accel_group_connect(shortcuts, acc_key, mod, GTK_ACCEL_MASK, macros[i].closure);
      i++;
    }
}

static void macros_destroy(void)
{
  gint i = 0;

  if(macros == NULL)
    return;

  while(macros[i].shortcut != NULL)
    {
      g_free(macros[i].shortcut);
      g_free(macros[i].action);
      /*
      g_closure_unref(macros[i].closure);
      */
      i++;
    }
  g_free(macros);
  macros = NULL;
}

void remove_shortcuts(void)
{
  gint i = 0;

  if(macros == NULL)
    return;

  while(macros[i].shortcut != NULL)
    {
      gtk_accel_group_disconnect(shortcuts, macros[i].closure);
      i++;
    }

  macros_destroy();
}

static GtkTreeModel *create_model(void)
{
  gint i = 0;
  GtkListStore *store;
  GtkTreeIter iter;

  /* create list store */
  store = gtk_list_store_new (NUM_COLUMNS,
			      G_TYPE_STRING,
			      G_TYPE_STRING,
			      G_TYPE_BOOLEAN,
			      G_TYPE_BOOLEAN);

  /* add data to the list store */
  if(macros != NULL)
    {
      while(1)
	{
	  if(macros[i].shortcut == NULL)
	    break;
	  gtk_list_store_append (store, &iter);
	  gtk_list_store_set (store, &iter,
			      COLUMN_SHORTCUT, macros[i].shortcut,
			      COLUMN_ACTION, macros[i].action,
			      -1);
	  i++;
	}
    }

  return GTK_TREE_MODEL(store);
}

static gboolean
shortcut_edited (GtkCellRendererText *cell,
		 const gchar         *path_string,
		 const gchar         *new_text,
		 gpointer             data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;

  gtk_tree_model_get_iter(model, &iter, path);

  gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_ACTION, new_text, -1);
  gtk_tree_path_free (path);

  return TRUE;
}

static void
add_columns (GtkTreeView *treeview)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"),
						     renderer,
						     "text",
						     COLUMN_SHORTCUT,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_SHORTCUT);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  g_signal_connect (renderer, "edited", G_CALLBACK(shortcut_edited), model);
  column = gtk_tree_view_column_new_with_attributes ("Action", renderer, "text", COLUMN_ACTION, NULL);
  g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_ACTION);
  gtk_tree_view_append_column (treeview, column);
}

static gint Add_shortcut(GtkWidget *button, gpointer pointer)
{
  GtkTreeIter iter;
  GtkTreeModel *model = (GtkTreeModel *)pointer;


  gtk_list_store_append(GTK_LIST_STORE(model), &iter);

  gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_SHORTCUT, "None", -1);

  return FALSE;
}

static gboolean Delete_shortcut(GtkWidget *button, gpointer pointer)
{
  GtkTreeIter iter;
  GtkTreeView *treeview = (GtkTreeView *)pointer;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gint i;
      GtkTreePath *path;

      path = gtk_tree_model_get_path(model, &iter);
      i = gtk_tree_path_get_indices(path)[0];
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

      gtk_tree_path_free (path);
    }

  return FALSE;
}

static gboolean Save_shortcuts(GtkWidget *button, gpointer pointer)
{
  GtkTreeIter iter;
  GtkTreeView *treeview = (GtkTreeView *)pointer;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  gint i = 0;

  remove_shortcuts();

  if(gtk_tree_model_get_iter_first(model, &iter))
    {
      do
	{
	  i++;
	} while(gtk_tree_model_iter_next(model, &iter));

      gtk_tree_model_get_iter_first(model, &iter);

      macros = g_malloc((i + 1) * sizeof(macro_t));
      i = 0;
      if(macros != NULL)
	{
	  do
	    {
	      gtk_tree_model_get(model, &iter, COLUMN_SHORTCUT, &(macros[i].shortcut), \
				 COLUMN_ACTION, &(macros[i].action), \
				 -1);
	      i++;
	    } while(gtk_tree_model_iter_next(model, &iter));

	  macros[i].shortcut = NULL;
	  macros[i].action = NULL;
	}
    }

  add_shortcuts();

  return FALSE;
}

static gboolean key_pressed(GtkWidget *window, GdkEventKey *key, gpointer pointer)
{
  GtkTreeIter iter;
  GtkTreeView *treeview = (GtkTreeView *)pointer;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
  gchar *str = NULL;

  switch(key->keyval)
    {
    case GDK_Shift_L:
    case GDK_Shift_R:
    case GDK_Control_L:
    case GDK_Control_R:
    case GDK_Caps_Lock:
    case GDK_Shift_Lock:
    case GDK_Meta_L:
    case GDK_Meta_R:
    case GDK_Alt_L:
    case GDK_Alt_R:
    case GDK_Super_L:
    case GDK_Super_R:
    case GDK_Hyper_L:
    case GDK_Hyper_R:
    case GDK_Mode_switch:
      return FALSE;
    default:
      break;
    }

  if(gtk_tree_selection_get_selected(selection, NULL, &iter))
    {
      gint i;
      GtkTreePath *path;

      path = gtk_tree_model_get_path(model, &iter);
      i = gtk_tree_path_get_indices(path)[0];
      str = gtk_accelerator_name(key->keyval, key->state & ~GDK_MOD2_MASK);
      gtk_list_store_set(GTK_LIST_STORE (model), &iter, COLUMN_SHORTCUT, str, -1);

      gtk_tree_path_free(path);
      g_free(str);

      g_signal_handlers_disconnect_by_func(window, G_CALLBACK(key_pressed), pointer);
    }
  return FALSE;
}


static gboolean Capture_shortcut(GtkWidget *button, gpointer pointer)
{
  g_signal_connect_after(window, "key_press_event", G_CALLBACK(key_pressed), pointer);

  return FALSE;
}

static gboolean Help_screen(GtkWidget *button, gpointer pointer)
{
  GtkWidget *Dialogue, *Label, *Bouton, *Frame;

  Dialogue = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(Dialogue), _("Help on macros"));
  Bouton = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_signal_connect_object(GTK_OBJECT(Bouton), "clicked", (GtkSignalFunc)gtk_widget_destroy, GTK_OBJECT(Dialogue));
  gtk_signal_connect(GTK_OBJECT(Dialogue), "destroy", (GtkSignalFunc)gtk_widget_destroy, NULL);
  gtk_signal_connect(GTK_OBJECT(Dialogue), "delete_event", (GtkSignalFunc)gtk_widget_destroy, NULL);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Dialogue)->action_area), Bouton, TRUE, TRUE, 0);

  Label = gtk_label_new(_("The \"action\" field of a macro is the data to be sent on the port. Text can be entered, but also special chars, like \\n, \\t, \\r, etc. You can also enter hexadecimal data preceded by a '\\'. The hexadecimal data should not begin with a letter (eg. use \\0FF and not \\FF)\nExamples :\n\t\"Hello\\n\" sends \"Hello\" followed by a Line Feed\n\t\"Hello\\0A\" does the same thing but the LF is entered in hexadecimal"));
  gtk_label_set_line_wrap(GTK_LABEL (Label), TRUE);
  gtk_label_set_selectable(GTK_LABEL(Label), TRUE);
  gtk_misc_set_padding(GTK_MISC(Label), 10, 10);

  Frame = gtk_frame_new(NULL);
  gtk_container_set_border_width(GTK_CONTAINER(Frame), 5);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Dialogue)->vbox), Frame, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(Frame), Label);

  gtk_widget_show_all(Dialogue);

  return FALSE;
}


gint Config_macros(GtkWidget *wid, guint param)
{
  GtkWidget *vbox, *hbox;
  GtkWidget *sw;
  GtkTreeModel *model;
  GtkWidget *treeview;
  GtkWidget *button;
  GtkWidget *separator;

  /* create window, etc */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), _("Configure Macros"));

  g_signal_connect (window, "destroy",
		    G_CALLBACK (gtk_widget_destroyed), &window);
  gtk_container_set_border_width (GTK_CONTAINER (window), 8);

  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

  /* create tree model */
  model = create_model ();

  /* create tree view */
  treeview = gtk_tree_view_new_with_model (model);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (treeview),
				   COLUMN_SHORTCUT);

  g_object_unref (model);

  gtk_container_add (GTK_CONTAINER (sw), treeview);

  /* add columns to the tree view */
  add_columns (GTK_TREE_VIEW (treeview));

  hbox = gtk_hbox_new (TRUE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic (_("_Add"));
  g_signal_connect(button, "clicked", G_CALLBACK(Add_shortcut), (gpointer)model);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_mnemonic (_("_Delete"));
  g_signal_connect(button, "clicked", G_CALLBACK(Delete_shortcut), (gpointer)treeview);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_mnemonic (_("_Capture Shortcut"));
  g_signal_connect(button, "clicked", G_CALLBACK(Capture_shortcut), (gpointer)treeview);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  separator = gtk_hseparator_new();
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (TRUE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  button = gtk_button_new_from_stock (GTK_STOCK_HELP);
  g_signal_connect(button, "clicked", G_CALLBACK(Help_screen), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_from_stock (GTK_STOCK_OK);
  g_signal_connect(button, "clicked", G_CALLBACK(Save_shortcuts), (gpointer)treeview);
  g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_widget_destroy), (gpointer)window);
  gtk_box_pack_end (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_widget_destroy), (gpointer)window);
  gtk_box_pack_end (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  gtk_window_set_default_size (GTK_WINDOW(window), 300, 400);

  gtk_widget_show_all(window);

  return FALSE;
}

