/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   readmidi.h

   */

#ifndef ___READMIDI_H_
#define ___READMIDI_H_

/* MIDI file types */
#define IS_ERROR_FILE	-1	/* Error file */
#define IS_OTHER_FILE	0	/* Not a MIDI file */
#define IS_SMF_FILE	101	/* Standard MIDI File */
#define IS_MCP_FILE	201	/* MCP */
#define IS_RCP_FILE	202	/* Recomposer */
#define IS_R36_FILE	203	/* Recomposer */
#define IS_G18_FILE	204	/* Recomposer */
#define IS_G36_FILE	205	/* Recomposer */
#define IS_SNG_FILE	301
#define IS_MM2_FILE	401
#define IS_MML_FILE	501
#define IS_FM_FILE	601
#define IS_FPD_FILE	602
#define IS_MOD_FILE	701	/* Pro/Fast/Star/Noise Tracker */
#define IS_669_FILE	702	/* Composer 669, UNIS669 */
#define IS_MTM_FILE	703	/* MultiModuleEdit */
#define IS_STM_FILE	704	/* ScreamTracker 2 */
#define IS_S3M_FILE	705	/* ScreamTracker 3 */
#define IS_ULT_FILE	706	/* UltraTracker */
#define IS_XM_FILE	707	/* FastTracker 2 */
#define IS_FAR_FILE	708	/* Farandole Composer */
#define IS_WOW_FILE	709	/* Grave Composer */
#define IS_OKT_FILE	710	/* Oktalyzer */
#define IS_DMF_FILE	711	/* X-Tracker */
#define IS_MED_FILE	712	/* MED/OctaMED */
#define IS_IT_FILE	713	/* Impulse Tracker */
#define IS_PTM_FILE	714	/* Poly Tracker */

enum play_system_modes
{
    DEFAULT_SYSTEM_MODE,
    GM_SYSTEM_MODE,
    GS_SYSTEM_MODE,
    XG_SYSTEM_MODE
};

enum {
    PCM_MODE_NON = 0,
    PCM_MODE_WAV,
    PCM_MODE_AIFF,
    PCM_MODE_AU,
    PCM_MODE_MP3
};

#define IS_CURRENT_MOD_FILE \
	(current_file_info && \
	current_file_info->file_type >= 700 && \
	current_file_info->file_type < 800)

typedef struct {
  MidiEvent event;
  void *next;
  void *prev;
} MidiEventList;

struct midi_file_info
{
    int readflag;
    char *filename;
    char *seq_name;
    char *karaoke_title;
    char *first_text;
    uint8 mid;	/* Manufacture ID (0x41/Roland, 0x43/Yamaha, etc...) */
    int16 hdrsiz;
    int16 format;
    int16 tracks;
    int32 divisions;
    int time_sig_n, time_sig_d, time_sig_c, time_sig_b;	/* Time signature */
    int drumchannels_isset;
    ChannelBitMask drumchannels;
    ChannelBitMask drumchannel_mask;
    int32 samples;
    int max_channel;
    struct midi_file_info *next;
    int compressed; /* True if midi_data is compressed */
    char *midi_data;
    int32 midi_data_size;
    int file_type;
    
    int pcm_mode;
    char *pcm_filename;
    struct timidity_file *pcm_tf;
};

struct delay_status_t
{
	uint8 level;		/* master level */
    uint8 level_center;
    uint8 level_left;
    uint8 level_right;
    int time_center;			/* (ms) */
    double time_ratio_left;		/* gs_val/24 */
    double time_ratio_right;	/* gs_val/24 */
	int time_left;
	int time_right;
    uint8 feed_back;	/* reserved. */
	uint8 pre_lpf;		/* reserved. */
	double level_ratio_center;
	double level_ratio_left;
	double level_ratio_right;
	int fb_loop;
	double fb_ratio;
};

struct reverb_status_t
{
	uint8 character;
	uint8 pre_lpf;
	uint8 level;
	uint8 time;
	uint8 delay_feedback;
	uint8 pre_delay_time;	/* (ms) */
};

struct chorus_status_t
{
    int status;
    uint8 voice_reserve[18];
    uint8 macro[3];
    uint8 pre_lpf[3];
    uint8 level[3];
    uint8 feed_back[3];
    uint8 delay[3];
    uint8 rate[3];
    uint8 depth[3];
    uint8 send_level[3];
};

struct insertion_effect_t
{
	uint8 type_lsb;
	uint8 type_msb;
	uint8 type;
	uint8 channel[16];	/* 0:BYPASS, 1:EFX */
	uint8 parameter[20];
	uint8 send_reverb;
	uint8 send_chorus;
	uint8 send_delay;
	uint8 control_source1;
	uint8 control_depth1;
	uint8 control_source2;
	uint8 control_depth2;
	uint8 send_eq_switch;
};


extern int32 readmidi_set_track(int trackno, int rewindp);
extern void readmidi_add_event(MidiEvent *newev);
extern void readmidi_add_ctl_event(int32 at, int ch, int control, int val);
extern int parse_sysex_event(uint8 *data, int32 datalen, MidiEvent *ev_ret);
extern int convert_midi_control_change(int chn, int type, int val,
				       MidiEvent *ev_ret);
extern char *readmidi_make_string_event(int type, char *string, MidiEvent *ev,
					int cnv);
extern MidiEvent *read_midi_file(struct timidity_file *mtf,
				 int32 *count, int32 *sp, char *file_name);
extern struct midi_file_info *get_midi_file_info(char *filename, int newp);
extern struct midi_file_info *new_midi_file_info(char *filename);
extern void free_all_midi_file_info(void);
extern int check_midi_file(char *filename);
extern char *get_midi_title(char *filename);
extern struct timidity_file *open_midi_file(char *name,
					    int decompress, int noise_mode);
extern int midi_file_save_as(char *in_name, char *out_name);
extern char *event2string(int id);
extern void change_system_mode(int mode);
extern int get_default_mapID(int ch);
extern int dump_current_timesig(MidiEvent *codes, int maxlen);

extern ChannelBitMask quietchannels;
extern struct midi_file_info *current_file_info;
extern int opt_trace_text_meta_event;
extern int opt_default_mid;
extern int opt_system_mid;
extern int ignore_midi_error;
extern int readmidi_error_flag;
extern int readmidi_wrd_mode;
extern int play_system_mode;
extern FLOAT_T tempo_adjust;

extern struct delay_status_t *get_delay_status();
extern struct reverb_status_t *get_reverb_status();
extern struct chorus_status_t *get_chorus_status();

#endif /* ___READMIDI_H_ */
