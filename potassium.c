/*
 * potassium.c
 *
 * Clutter based music player using libmozart
 *
 * Copyright (C) 2010 Andrew Clayton
 *
 * License: GPLv2. See COPYING
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <clutter/clutter.h>
#include <gst/gst.h>
#include <glib.h>

#include <libmozart.h>


static void generate_playlist(char *playlist, char *name);
static char *get_position(char *time_info);
static gboolean update_display(ClutterActor *stage);
static void input_events_cb(ClutterActor *stage, ClutterEvent *event,
							gpointer user_data);
static gboolean set_status_icons(GstBus *mozart_bus, gpointer user_data,
							ClutterActor *stage);
static void toggle_repeat(ClutterActor *stage, char *type);
static void toggle_shuffle(ClutterActor *stage);
static void init_icons(ClutterActor *stage);
static gboolean write_checkpoint_data();
static void read_checkpoint_data();

/*
 * Reads in a playlist file or just creates a playlist from a
 * single file passed on the command line.
 */
static void generate_playlist(char *playlist, char *name)
{
	if (name != NULL)
		mozart_init_playlist(name);

	if (g_str_has_suffix(playlist, ".m3u"))
		mozart_add_m3u_to_playlist(playlist, name);
	else
		mozart_add_uri_to_playlist(playlist, name);
}

/* Get track position information */
static char *get_position(char *time_info)
{
	int hours, thours, minutes, tminutes, seconds, tseconds;
	char *buf;

	mozart_get_stream_position_hms(&hours, &minutes, &seconds);
	mozart_get_stream_duration_hms(&thours, &tminutes, &tseconds);

	buf = malloc(sizeof(char) * 22);
	sprintf(buf, "%d:%.2d:%.2d / %d:%.2d:%.2d", hours, minutes,
					seconds, thours, tminutes, tseconds);

	strcpy(time_info, buf);
	free(buf);

	return time_info;
}

/* Update the clutter display with track information */
static gboolean update_display(ClutterActor *stage)
{
	static ClutterColor actor_color = { 0xff, 0xff, 0xff, 0xff };
	static ClutterActor *artist_text, *album_text, *title_text, *time_text;
	char time_info[22];
	char time_data[80];

	if (!artist_text) {
		artist_text = clutter_text_new_full("Sans 14", NULL, 
								&actor_color);
		clutter_actor_set_position(artist_text, 10, 0);
		clutter_container_add_actor(CLUTTER_CONTAINER(stage), 
								artist_text);
		clutter_actor_show(artist_text);
	}

	if (!album_text) {
		album_text = clutter_text_new_full("Sans 14", NULL,
								&actor_color);
		clutter_actor_set_position(album_text, 20, 22);
		clutter_container_add_actor(CLUTTER_CONTAINER(stage),
								album_text);
		clutter_actor_show(album_text);
	}

	if (!title_text) {
		title_text = clutter_text_new_full("Sans 14", NULL,
								&actor_color);
		clutter_actor_set_position(title_text, 30, 44);
		clutter_container_add_actor(CLUTTER_CONTAINER(stage),
								title_text);
		clutter_actor_show(title_text);
	}

	if (!time_text) {
		time_text = clutter_text_new_full("Sans 14", NULL, 
								&actor_color);
		clutter_actor_set_position(time_text, 10, 100);
		clutter_container_add_actor(CLUTTER_CONTAINER(stage), 
								time_text);
		clutter_actor_show(time_text);
	}
	
	if (mozart_tags_updated()) {
		clutter_text_set_text(CLUTTER_TEXT(artist_text), 
						mozart_get_tag_artist());
		clutter_text_set_text(CLUTTER_TEXT(album_text),
						mozart_get_tag_album());
		clutter_text_set_text(CLUTTER_TEXT(title_text),
						mozart_get_tag_title());
		mozart_set_got_tags();
	}

	sprintf(time_data, "%s [%6.2f%%] [%2d / %d]", get_position(time_info),
						mozart_get_stream_progress(),
						mozart_get_playlist_position(),
						mozart_get_playlist_size());
	
	clutter_text_set_text(CLUTTER_TEXT(time_text), time_data);
	
	return TRUE;
}

/* Process keyboard/mouse events */
static void input_events_cb(ClutterActor *stage, ClutterEvent *event,
							gpointer user_data)
{
	/* Only handle keyboard events */
	if (event->type != CLUTTER_KEY_PRESS)
		return;

	guint sym;

	sym = clutter_event_get_key_symbol(event);
	switch (sym) {
	case CLUTTER_Escape:
	case CLUTTER_q:
		clutter_main_quit();
		break;
	case CLUTTER_Right:
		mozart_next_track();
		break;
	case CLUTTER_Left:
		mozart_prev_track();
		break;
	case CLUTTER_Down:
		mozart_replay_track();
		break;
	case CLUTTER_space:
		mozart_play_pause();
		break;
	case CLUTTER_r:
		toggle_repeat(stage, "single");
		break;
	case CLUTTER_a:
		toggle_repeat(stage, "all");
		break;
	case CLUTTER_s:
		toggle_shuffle(stage);
		break;
	case CLUTTER_Page_Up:
		mozart_player_seek("seek-fwd");
		break;
	case CLUTTER_Page_Down:
		mozart_player_seek("seek-bwd");
		break;
	}
}

/*
 * Set an icon for playing/paused depending on stream status.
 * Disable the repeat single icon in the case of switching from one track
 * to another.
 */
static gboolean set_status_icons(GstBus *mozart_bus, gpointer user_data,
							ClutterActor *stage)
{
	ClutterActor *play_img;
	ClutterActor *pause_img;
	ClutterActor *ra_img;
	ClutterActor *rs_img;
	GstState state;
	
	state = mozart_get_player_state();

	play_img = clutter_container_find_child_by_name(
					CLUTTER_CONTAINER(stage), "play_img");
	pause_img = clutter_container_find_child_by_name(
					CLUTTER_CONTAINER(stage), "pause_img");

	if (state == GST_STATE_PLAYING) {
		clutter_actor_hide(pause_img);
		clutter_actor_show(play_img);
	} else {
		clutter_actor_hide(play_img);
		clutter_actor_show(pause_img);
	}

	if (!mozart_get_repeat_all()) {
		ra_img = clutter_container_find_child_by_name(
				CLUTTER_CONTAINER(stage), "ra_img");
		clutter_actor_hide(ra_img);
	}

	if (!mozart_get_repeat_single()) {
		rs_img = clutter_container_find_child_by_name(
					CLUTTER_CONTAINER(stage), "rs_img");
		clutter_actor_hide(rs_img);
	}

	return TRUE;
}

/* Set an icon for repeat single or repeat all */
static void toggle_repeat(ClutterActor *stage, char *type)
{
	ClutterActor *rs_img;
	ClutterActor *ra_img;

	rs_img = clutter_container_find_child_by_name(
					CLUTTER_CONTAINER(stage), "rs_img");
	ra_img = clutter_container_find_child_by_name(
					CLUTTER_CONTAINER(stage), "ra_img");

	if (strcmp(type, "single") == 0) {
		if (mozart_get_repeat_all()) {
			mozart_toggle_repeat_all();
			clutter_actor_hide(ra_img);
			clutter_actor_show(rs_img);
			mozart_toggle_repeat_single();
		} else if (mozart_get_repeat_single()) {
			clutter_actor_hide(rs_img);
			clutter_actor_hide(ra_img);
			mozart_toggle_repeat_single();
		} else {
			mozart_toggle_repeat_single();
			clutter_actor_hide(ra_img);
			clutter_actor_show(rs_img);
		}	
	} else if (strcmp(type, "all") == 0) {
		if (mozart_get_repeat_single()) {
			mozart_toggle_repeat_single();
			clutter_actor_hide(rs_img);
			clutter_actor_show(ra_img);
			mozart_toggle_repeat_all();
		} else if (mozart_get_repeat_all()) {
			clutter_actor_hide(ra_img);
			clutter_actor_hide(rs_img);
			mozart_toggle_repeat_all();
		} else {
			mozart_toggle_repeat_all();
			clutter_actor_hide(rs_img);
			clutter_actor_show(ra_img);
		}
	}
}

/* Set an icon for shuffle mode */
static void toggle_shuffle(ClutterActor *stage)
{
	ClutterActor *shuffle_img;

	shuffle_img = clutter_container_find_child_by_name(
				CLUTTER_CONTAINER(stage), "shuffle_img");

	if (mozart_playlist_shuffled(NULL)) {
		clutter_actor_hide(shuffle_img);
		mozart_unshuffle(NULL);
	} else {
		clutter_actor_show(shuffle_img);
		mozart_shuffle(NULL);
	}
}

/* Setup various status icons */
static void init_icons(ClutterActor *stage)
{
	ClutterActor *rs_img;
	ClutterActor *ra_img;
	ClutterActor *shuffle_img;
	ClutterActor *play_img;
	ClutterActor *pause_img;

	rs_img = clutter_texture_new_from_file("icons/repeat-single.png", NULL);
	clutter_actor_set_name(rs_img, "rs_img");
	clutter_actor_set_size(rs_img, 20, 20);
	clutter_actor_set_position(rs_img, 430, 102);
	if (!mozart_get_repeat_single())
		clutter_actor_hide(rs_img);
	clutter_container_add_actor(CLUTTER_CONTAINER(stage), rs_img);

	ra_img = clutter_texture_new_from_file("icons/repeat-all.png", NULL);
	clutter_actor_set_name(ra_img, "ra_img");
	clutter_actor_set_size(ra_img, 20, 20);
	clutter_actor_set_position(ra_img, 430, 102);
	if (!mozart_get_repeat_all())
		clutter_actor_hide(ra_img);
	clutter_container_add_actor(CLUTTER_CONTAINER(stage), ra_img);

	shuffle_img = clutter_texture_new_from_file("icons/shuffle.png", NULL);
	clutter_actor_set_name(shuffle_img, "shuffle_img");
	clutter_actor_set_size(shuffle_img, 20, 20);
	clutter_actor_set_position(shuffle_img, 460, 102);
	if (!mozart_playlist_shuffled(NULL))
		clutter_actor_hide(shuffle_img);
	clutter_container_add_actor(CLUTTER_CONTAINER(stage), shuffle_img);

	play_img = clutter_texture_new_from_file("icons/playing.png", NULL);
	clutter_actor_set_name(play_img, "play_img");
	clutter_actor_set_size(play_img, 20, 20);
	clutter_actor_set_position(play_img, 400, 102);
	clutter_actor_hide(play_img);
	clutter_container_add_actor(CLUTTER_CONTAINER(stage), play_img);

	pause_img = clutter_texture_new_from_file("icons/paused.png", NULL);
	clutter_actor_set_name(pause_img, "pause_img");
	clutter_actor_set_size(pause_img, 20, 20);
	clutter_actor_set_position(pause_img, 400, 102);
	clutter_actor_hide(pause_img);
	clutter_container_add_actor(CLUTTER_CONTAINER(stage), pause_img);
}

static gboolean write_checkpoint_data()
{
	char data[512];
	int fd;
	GstState state;

	state = mozart_get_player_state();
	if (state != GST_STATE_PLAYING)
		return TRUE;

	fd = creat("/tmp/potassium-checkpoint.tmp", 0666);

	sprintf(data, "%s\n%d\n%lu\n", mozart_get_active_playlist_name(),
					mozart_get_playlist_position(),
					mozart_get_stream_position_ns());
	write(fd, data, strlen(data));
	rename("/tmp/potassium-checkpoint.tmp", "/tmp/potassium-checkpoint");

	close(fd);

	return TRUE;
}

static void read_checkpoint_data()
{
	static FILE *fp;
	char data[512];
	int track;
	gint64 pos;

	fp = fopen("/tmp/potassium-checkpoint", "r");

	fgets(data, 512, fp);
	generate_playlist(g_strchomp(strdup(data)), g_strchomp(strdup(data)));
	mozart_switch_playlist(g_strchomp(data));

	fgets(data, 512, fp);
	track = atoi(data);

	fgets(data, 512, fp);
	pos = atoll(data);

	fclose(fp);

	/*
	 * track - 1 here as the playlist index starts at 0
	 */
	mozart_play_index_at_pos(track - 1, pos);
}

int main(int argc, char **argv)
{
	ClutterActor *stage;
	ClutterColor stage_clr = { 0x00, 0x00, 0x00, 0xff };
	const gchar *stage_title = { "potassium music player" };

	g_set_application_name("potassium music player");

	clutter_init(&argc, &argv);
	
	stage = clutter_stage_get_default();
	clutter_actor_set_size(stage, 512, 128);
	clutter_stage_set_color(CLUTTER_STAGE(stage), &stage_clr);
	clutter_stage_set_title(CLUTTER_STAGE(stage), stage_title);
	clutter_actor_set_name(stage, "stage");
	clutter_actor_show(stage);

	/* Handle keyboard/mouse events */
	g_signal_connect(stage, "event", G_CALLBACK(input_events_cb), NULL);

	mozart_init(argc, argv);
	
	if (argc == 2) {
		/*
		 * strdup() argv[1] here, as it seems to get mangled by
		 * generate_playlist()
		 */
		mozart_init_playlist(strdup(argv[1]));
		generate_playlist(strdup(argv[1]), strdup(argv[1]));
		mozart_switch_playlist(argv[1]);
		mozart_rock_and_roll();
	} else {
		read_checkpoint_data();
	}

	init_icons(stage);

	g_timeout_add(500, (GSourceFunc)update_display, stage);
	g_timeout_add_seconds(1, (GSourceFunc)write_checkpoint_data, NULL);
	g_signal_connect(mozart_bus, "message::state-changed",
					G_CALLBACK(set_status_icons), stage);
	
	clutter_main();

	mozart_destroy();
	exit(0);
}
