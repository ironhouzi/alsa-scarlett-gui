// SPDX-FileCopyrightText: 2022-2024 Geoffrey D. Bennett <g@b4.vu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtk/gtk.h>
#include <stdio.h>

#include "gtkhelper.h"
#include "stringhelper.h"
#include "widget-gain.h"
#include "window-mixer.h"

static struct routing_snk *get_mixer_r_snk(
  struct alsa_card *card,
  int               input_num
) {
  for (int i = 0; i < card->routing_snks->len; i++) {
    struct routing_snk *r_snk = &g_array_index(
      card->routing_snks, struct routing_snk, i
    );
    if (r_snk->port_category != PC_MIX)
      continue;
    if (r_snk->elem->lr_num == input_num)
      return r_snk;
  }
  return NULL;
}

GtkWidget *create_mixer_controls(struct alsa_card *card) {
  GtkWidget *top = gtk_frame_new(NULL);
  gtk_widget_add_css_class(top, "window-frame");

  GtkWidget *mixer_top = gtk_grid_new();
  gtk_widget_add_css_class(mixer_top, "window-content");
  gtk_widget_add_css_class(mixer_top, "top-level-content");
  gtk_widget_add_css_class(mixer_top, "window-mixer");
  gtk_frame_set_child(GTK_FRAME(top), mixer_top);

  gtk_widget_set_halign(mixer_top, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(mixer_top, GTK_ALIGN_CENTER);
  gtk_grid_set_column_homogeneous(GTK_GRID(mixer_top), TRUE);

  GArray *elems = card->elems;

  // create the Mix X labels on the left and right of the grid
  for (int i = 0; i < card->routing_in_count[PC_MIX]; i++) {
    // sane naming hack
    // char name[10];
    char name[16];
    snprintf(name, 10, "Mix %c", i + 'A');
    // sane naming hack
    if (strcmp(name, "Mix A") == 0) {
      char *human_name = "Speaker L";
      snprintf(name, strlen(human_name) + 1, "%s", human_name);
    } else if (strcmp(name, "Mix B") == 0) {
      char *human_name = "Speaker R";
      snprintf(name, strlen(human_name) + 1, "%s", human_name);
    } else if (strcmp(name, "Mix C") == 0) {
      char *human_name = "Headphones L";
      snprintf(name, strlen(human_name) + 1, "%s", human_name);
    } else if (strcmp(name, "Mix D") == 0) {
      char *human_name = "Headphones R";
      snprintf(name, strlen(human_name) + 1, "%s", human_name);
    }

    GtkWidget *l_left = gtk_label_new(name);
    gtk_grid_attach(
      GTK_GRID(mixer_top), l_left,
      0, i + 2, 1, 1
    );

    GtkWidget *l_right = gtk_label_new(name);
    gtk_grid_attach(
      GTK_GRID(mixer_top), l_right,
      card->routing_out_count[PC_MIX] + 1, i + 2, 1, 1
    );
  }

  // go through each element and create the mixer
  for (int i = 0; i < elems->len; i++) {
    struct alsa_elem *elem = &g_array_index(elems, struct alsa_elem, i);

    // if no card entry, it's an empty slot
    if (!elem->card)
      continue;

    // looking for "Mix X Input Y Playback Volume" elements
    if (strncmp(elem->name, "Mix ", 4) != 0)
      continue;
    if (!strstr(elem->name, "Playback Volume"))
      continue;

    // hack: remove unused ports
    if (strstr(elem->name, "07")) continue;
    if (strstr(elem->name, "08")) continue;
    if (strstr(elem->name, "09")) continue;
    if (strstr(elem->name, "10")) continue;

    // extract the mix number and input number from the element name
    int mix_num = elem->name[4] - 'A';
    int input_num = get_num_from_string(elem->name) - 1;

    if (mix_num >= MAX_MIX_OUT) {
      printf("mix_num %d >= MAX_MIX_OUT %d\n", mix_num, MAX_MIX_OUT);
      continue;
    }

    // create the gain control and attach to the grid
    GtkWidget *w = make_gain_alsa_elem(elem, 1, WIDGET_GAIN_TAPER_LOG, 0);
    gtk_grid_attach(GTK_GRID(mixer_top), w, input_num + 1, mix_num + 2, 1, 1);

    // look up the r_snk entry for the mixer input number
    struct routing_snk *r_snk = get_mixer_r_snk(card, input_num + 1);
    if (!r_snk) {
      printf("missing mixer input %d\n", input_num);
      continue;
    }

    // lookup the top label for the mixer input
    GtkWidget *l_top = r_snk->mixer_label_top;

    // if the top label doesn't already exist the bottom doesn't
    // either; create them both and attach to the grid
    if (!l_top) {
      l_top = r_snk->mixer_label_top = gtk_label_new("");
      GtkWidget *l_bottom = r_snk->mixer_label_bottom = gtk_label_new("");

      gtk_grid_attach(
        GTK_GRID(mixer_top), l_top,
        input_num, (input_num + 1) % 2, 3, 1
      );
      gtk_grid_attach(
        GTK_GRID(mixer_top), l_bottom,
        input_num, card->routing_in_count[PC_MIX] + input_num % 2 + 2, 3, 1
      );
    }
  }

  update_mixer_labels(card);

  return top;
}

void update_mixer_labels(struct alsa_card *card) {
  for (int i = 0; i < card->routing_snks->len; i++) {
    struct routing_snk *r_snk = &g_array_index(
      card->routing_snks, struct routing_snk, i
    );

    if (r_snk->port_category != PC_MIX)
      continue;

    struct alsa_elem *elem = r_snk->elem;

    int routing_src_idx = alsa_get_elem_value(elem);

    struct routing_src *r_src = &g_array_index(
      card->routing_srcs, struct routing_src, routing_src_idx
    );

    // hack: skip unused ports
    if (strcmp(r_src->name, "PCM 3") == 0) continue;
    if (strcmp(r_src->name, "PCM 4") == 0) continue;
    if (strcmp(r_src->name, "PCM 5") == 0) continue;
    if (strcmp(r_src->name, "PCM 6") == 0) continue;

    if (r_snk->mixer_label_top) {
      // sane naming hack!
      char *name = strdup(r_src->name);

      if (strcmp(name, "DSP 1") == 0) {
        char *human_name = "P-Bass";
        snprintf(name, strlen(human_name) + 1, "%s", human_name);
      } else if (strcmp(name, "DSP 2") == 0) {
        char *human_name = "Telecaster";
        snprintf(name, strlen(human_name) + 1, "%s", human_name);
      } else if (strcmp(name, "Analogue 3") == 0) {
        char *human_name = "KO-II L";
        snprintf(name, strlen(human_name) + 1, "%s", human_name);
      } else if (strcmp(name, "Analogue 4") == 0) {
        char *human_name = "KO-II R";
        snprintf(name, strlen(human_name) + 1, "%s", human_name);
      } else if (strcmp(name, "PCM 1") == 0) {
        char *human_name = "Computer L";
        snprintf(name, strlen(human_name) + 1, "%s", human_name);
      } else if (strcmp(name, "PCM 2") == 0) {
        char *human_name = "Computer R";
        snprintf(name, strlen(human_name) + 1, "%s", human_name);
      }

      // gtk_label_set_text(GTK_LABEL(r_snk->mixer_label_top), r_src->name);
      // gtk_label_set_text(GTK_LABEL(r_snk->mixer_label_bottom), r_src->name);
      gtk_label_set_text(GTK_LABEL(r_snk->mixer_label_top), name);
      gtk_label_set_text(GTK_LABEL(r_snk->mixer_label_bottom), name);
    }
  }
}
