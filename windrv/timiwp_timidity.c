#include "config.h"
#include <windows.h>
#include <sys/stat.h>
#include <io.h>

#ifndef __bool_true_false_are_defined
# ifdef bool
#  undef bool
# endif
# ifdef ture
#  undef ture
# endif
# ifdef false
#  undef false
# endif
# define bool int
# define false ((bool)0)
# define true (!false)
# define __bool_true_false_are_defined true
#endif 
/* C99 _Bool hack */

#include "arc.h"
#include "tmdy_getopt.h"
#include "rtsyn.h"

/* MAIN_INTERFACE */
extern void timidity_start_initialize(void);
extern int timidity_pre_load_configuration(void);
extern int timidity_post_load_configuration(void);
extern void timidity_init_player(void);
extern void timidity_init_aq_buff(void);
extern int set_tim_opt_long(int, char *, int);

extern void tmdy_free_config(void);

extern  const char *optcommands;
extern  const struct option longopts[];

#define INTERACTIVE_INTERFACE_IDS "kmqagrwAWP"

extern int got_a_configuration;

extern int def_prog;
extern char def_instr_name[256];

extern CRITICAL_SECTION critSect;
extern int opt_evil_mode;





#include "timiwp_timidity.h"


static BOOL WINAPI handler(DWORD dw)
{
//    printf ("***BREAK" NLS); fflush(stdout);
    intr++;
    return TRUE;
}

static	RETSIGTYPE sigterm_exit(int sig)
{
    char s[4];

    /* NOTE: Here, fprintf is dangerous because it is not re-enterance
     * function.  It is possible coredump if the signal is called in printf's.
     */
/*
    write(2, "Terminated sig=0x", 17);
    s[0] = "0123456789abcdef"[(sig >> 4) & 0xf];
    s[1] = "0123456789abcdef"[sig & 0xf];
    s[2] = '\n';
    write(2, s, 3);
*/
    safe_exit(1);
}

static inline bool directory_p(const char* path)
{
    struct stat st;
    if(stat(path, &st) != -1) return S_ISDIR(st.st_mode);
    return false;
}

static inline void canonicalize_path(char* path)
{
    int len = strlen(path);
    if(!len || path[len-1]==PATH_SEP) return;
    path[len] = PATH_SEP;
    path[len+1] = '\0';
}





int timiwp_main_ini(int argc, char **argv)
{
    int c, err;
    int nfiles;
    char **files;
    int main_ret;
    int longind;
	
	
    program_name=argv[0];
    timidity_start_initialize();

    for(c = 1; c < argc; c++)
    {
	if(directory_p(argv[c]))
	{
	    char *p;
	    p = (char *)safe_malloc(strlen(argv[c]) + 2);
	    strcpy(p, argv[c]);
	    canonicalize_path(p);
		free(argv[c]);
	    argv[c] = p;
	}
    }

#if !defined(IA_WINSYN) && !defined(IA_PORTMIDISYN) && !defined(IA_W32G_SYN)
    if((err = timidity_pre_load_configuration()) != 0)
	return err;
#else
	 opt_sf_close_each_file = 0;
#endif

	optind = longind = 0;
#if defined(__MINGW32__) || defined(__CYGWIN__)
	optreset=1;
#endif
    while ((c = getopt_long(argc, argv, optcommands, longopts, &longind)) > 0)
	if ((err = set_tim_opt_long(c, optarg, longind)) != 0)
	    break;

#if defined(IA_WINSYN) || defined(IA_PORTMIDISYN) || defined(IA_W32G_SYN)
	if(got_a_configuration != 1){
	if((err = timidity_pre_load_configuration()) != 0)
	return err;
	}
#endif
	
	err += timidity_post_load_configuration();

	/* If there were problems, give up now */
    if(err || (optind >= argc &&
	       !strchr(INTERACTIVE_INTERFACE_IDS, ctl->id_character)))
    {
	if(!got_a_configuration)
	{
	    char config1[1024];
	    char config2[1024];

	    memset(config1, 0, sizeof(config1));
	    memset(config2, 0, sizeof(config2));
	    GetWindowsDirectory(config1, 1023 - 13);
	    strcat(config1, "\\TIMIDITY.CFG");
	    if(GetModuleFileName(NULL, config2, 1023))
	    {
		char *strp;
		config2[1023] = '\0';
		if(strp = strrchr(config2, '\\'))
		{
		    *(++strp)='\0';
		    strncat(config2,"TIMIDITY.CFG",sizeof(config2)-strlen(config2)-1);
		}
	    }

	    ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
		      "%s: Can't read any configuration file.\nPlease check "
		      "%s or %s", program_name, config1, config2);

	}
	else
	{
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		      "Try %s -h for help", program_name);
	}
    	
    }


	timidity_init_player();

    nfiles = argc - optind;
    files  = argv + optind;
    if(nfiles > 0 && ctl->id_character != 'r' && ctl->id_character != 'A' && ctl->id_character != 'W' && ctl->id_character != 'P')
	files = expand_file_archives(files, &nfiles);
    if(dumb_error_count)
	Sleep(1);

    main_ret = timiwp_play_main_ini(nfiles, files);

	return main_ret;

}

int timiwp_main_close(void)
{
	int i;
#if 0
	timiwp_play_main_close();

	free_instruments(0);
    free_global_mblock();
    free_all_midi_file_info();
	free_userdrum();
	free_userinst();
    tmdy_free_config();
	free_effect_buffers();
	for (i = 0; i < MAX_CHANNELS; i++) {free_drum_effect(i);}
#endif
#ifdef SUPPORT_SOCKET
	if (url_user_agent)
		free(url_user_agent);
	if (url_http_proxy_host)
		free(url_http_proxy_host);
	if (url_ftp_proxy_host)
		free(url_ftp_proxy_host);
	if (user_mailaddr)
		free(user_mailaddr);
#endif
#if 0
	if (pcm_alternate_file)
		free(pcm_alternate_file);
	if (opt_output_name)
		free(opt_output_name);
	if (opt_aq_max_buff)
		free(opt_aq_max_buff);
	if (opt_aq_fill_buff)
		free(opt_aq_fill_buff);
	if (output_text_code)
		free(output_text_code);
	if (wrdt_open_opts)
		free(wrdt_open_opts);
	if (nfiles > 0
			&& ctl->id_character != 'r' && ctl->id_character != 'A'
			&& ctl->id_character != 'W' && ctl->id_character != 'N'  && ctl->id_character != 'P') {
		free(files_nbuf);
		free(files);
	}
#endif
//#if 0
	free_soft_queue();
	free_instruments(0);
	free_soundfonts();
	free_cache_data();
	free_wrd();
	free_readmidi();

	free_global_mblock();
	tmdy_free_config();
	free_reverb_buffer();

	free_effect_buffers();
	free(voice);
	free_gauss_table();
	for (i = 0; i < MAX_CHANNELS; i++)
		free_drum_effect(i);
	return 0;
}


static inline int set_default_program(int prog)
{
	int bank;
	Instrument *ip;
	
	bank = (special_tonebank >= 0) ? special_tonebank : default_tonebank;
	if ((ip = play_midi_load_instrument(0, bank, prog)) == NULL)
		return 1;
	default_instrument = ip;
	return 0;
}

int timiwp_play_main_ini(int nfiles, char **files)
{
    int output_fail = 0;


    if(ctl->open(0, 0))
    {
/*	fprintf(stderr, "Couldn't open %s (`%c')" NLS,
		ctl->id_name, ctl->id_character);
*/
	play_mode->close_output();
	return 3;
    }

	
	signal(SIGTERM, sigterm_exit);
	SetConsoleCtrlHandler(handler, TRUE);

	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		  "Initialize for Critical Section");
	InitializeCriticalSection(&critSect);
	if(opt_evil_mode)
	    if(!SetThreadPriority(GetCurrentThread(),
				  THREAD_PRIORITY_ABOVE_NORMAL))
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Error raising process priority");


	/* Open output device */
	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		  "Open output: %c, %s",
		  play_mode->id_character,
		  play_mode->id_name);

	if (play_mode->flag & PF_PCM_STREAM) {
	    play_mode->extra_param[1] = aq_calc_fragsize();
	    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		      "requesting fragment size: %d",
		      play_mode->extra_param[1]);
	}
	if(play_mode->open_output() < 0)
	{
	    ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
		      "Couldn't open %s (`%c')",
		      play_mode->id_name, play_mode->id_character);
	    output_fail = 1;
	    ctl->close();
	    return 2;
	}

	if(!control_ratio)
	{
	    control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
	    if(control_ratio < 1)
		control_ratio = 1;
	    else if (control_ratio > MAX_CONTROL_RATIO)
		control_ratio = MAX_CONTROL_RATIO;
	}

	init_load_soundfont();
	if(!output_fail)
	{
	    aq_setup();
	    timidity_init_aq_buff();
	}
	if(allocate_cache_size > 0)
	    resamp_cache_reset();

	if (def_prog >= 0)
		set_default_program(def_prog);
	if (*def_instr_name)
		set_default_instrument(def_instr_name);

 return 0;
}


int timiwp_play_main_close (void)
{

	if(intr)
	    aq_flush(1);
	play_mode->close_output();
	ctl->close();
	DeleteCriticalSection (&critSect);
    free_archive_files();
	
    return 0;
}



