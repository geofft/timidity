/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if type char is unsigned and you are not using gcc.  */
// #ifndef __CHAR_UNSIGNED__
// #undef __CHAR_UNSIGNED__
// #endif

/* Define to empty if the keyword does not work.  */
// #undef const

/* Define if you don't have vprintf but do have _doprnt.  */
#undef HAVE_DOPRNT

/* Define if you have a working `mmap' system call.  */
#undef HAVE_MMAP

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#undef HAVE_SYS_WAIT_H

/* Define if you have the vprintf function.  */
// #undef HAVE_VPRINTF
#define HAVE_VPRINTF

/* Define as __inline if that's what the C compiler calls it.  */
// #undef inline
#define inline __inline

/* Define to `long' if <sys/types.h> doesn't define.  */
// #undef off_t

/* Define to `int' if <sys/types.h> doesn't define.  */
// #undef pid_t

/* Define as the return type of signal handlers (int or void).  */
// #undef RETSIGTYPE
#define RETSIGTYPE int

/* Define if the setvbuf function takes the buffering type as its second
   argument and the buffer pointer as the third, as on System V
   before release 3.  */
// #undef SETVBUF_REVERSED

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
// #undef size_t

/* Define if you have the ANSI C header files.  */
// #undef STDC_HEADERS

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
// #undef TIME_WITH_SYS_TIME

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#undef WORDS_BIGENDIAN

/* Define if the X Window System is missing or not being used.  */
#undef X_DISPLAY_MISSING

/* Define if you have the XShmCreatePixmap function.  */
#undef HAVE_XSHMCREATEPIXMAP

/* Define if you have the XmuRegisterExternalAgent function.  */
#undef HAVE_XMUREGISTEREXTERNALAGENT

/* Define if you have the getcwd function.  */
// #undef HAVE_GETCWD
#define HAVE_GETCWD

/* Define if you have the gethostbyname function.  */
// #undef HAVE_GETHOSTBYNAME
#define HAVE_GETHOSTBYNAME

/* Define if you have the getopt function.  */
#undef HAVE_GETOPT

/* Define if you have the getpagesize function.  */
#undef HAVE_GETPAGESIZE

/* Define if you have the gettimeofday function.  */
#undef HAVE_GETTIMEOFDAY

/* Define if you have the isatty function.  */
// #undef HAVE_ISATTY
#define HAVE_ISATTY

/* Define if you have the popen function.  */
// #undef HAVE_POPEN
#ifdef _MSC_VER
#define HAVE_POPEN
#define popen _popen
#define pclose _pclose
#endif

/* Define if you have the select function.  */
// #undef HAVE_SELECT
#define HAVE_SELECT

/* Define if you have the signal function.  */
// #undef HAVE_SIGNAL
#define HAVE_SIGNAL

/* Define if you have the sleep function.  */
#undef HAVE_SLEEP

/* Define if you have the snprintf function.  */
// #undef HAVE_SNPRINTF
#ifdef _MSC_VER
#define HAVE_SNPRINTF
#define snprintf _snprintf
#endif

/* Define if you have the socket function.  */
// #undef HAVE_SOCKET
#define HAVE_SOCKET

/* Define if you have the strdup function.  */
// #undef HAVE_STRDUP
#define HAVE_STRDUP

/* Define if you have the strerror function.  */
// #undef HAVE_STRERROR
#define HAVE_STRERROR

/* Define if you have the strncasecmp function.  */
// #undef HAVE_STRNCASECMP
#define HAVE_STRNCASECMP

/* Define if you have the strstr function.  */
// #undef HAVE_STRSTR
#define HAVE_STRSTR

/* Define if you have the usleep function.  */
#undef HAVE_USLEEP

/* Define if you have the vsnprintf function.  */
// #undef HAVE_VSNPRINTF
#ifdef _MSC_VER
#define HAVE_VSNPRINTF
#define vsnprintf _vsnprintf
#endif

/* Define if you have the <X11/Xmu/ExtAgent.h> header file.  */
#undef HAVE_X11_XMU_EXTAGENT_H

/* Define if you have the <X11/extensions/XShm.h> header file.  */
#undef HAVE_X11_EXTENSIONS_XSHM_H

/* Define if you have the <curses.h> header file.  */
#undef HAVE_CURSES_H

/* Define if you have the <dirent.h> header file.  */
// #undef HAVE_DIRENT_H
#ifdef _MSC_VER
#undef HAVE_DIRENT_H
#else
#define HAVE_DIRENT_H
#endif

/* Define if you have the <dlfcn.h> header file.  */
#undef HAVE_DLFCN_H

/* Define if you have the <errno.h> header file.  */
// #undef HAVE_ERRNO_H
#define HAVE_ERRNO_H

/* Define if you have the <fcntl.h> header file.  */
// #undef HAVE_FCNTL_H
#define HAVE_FCNTL_H

/* Define if you have the <getopt.h> header file.  */
#undef HAVE_GETOPT_H

/* Define if you have the <glob.h> header file.  */
#undef HAVE_GLOB_H

/* Define if you have the <limits.h> header file.  */
// #undef HAVE_LIMITS_H
#define HAVE_LIMITS_H

/* Define if you have the <machine/endian.h> header file.  */
#undef HAVE_MACHINE_ENDIAN_H

/* Define if you have the <malloc.h> header file.  */
#undef HAVE_MALLOC_H

/* Define if you have the <ncurses.h> header file.  */
#undef HAVE_NCURSES_H

/* Define if you have the <ncurses/curses.h> header file.  */
#undef HAVE_NCURSES_CURSES_H

/* Define if you have the <ndir.h> header file.  */
#undef HAVE_NDIR_H

/* Define if you have the <slang.h> header file.  */
#undef HAVE_SLANG_H

/* Define if you have the <slang/slang.h> header file.  */
#undef HAVE_SLANG_SLANG_H

/* Define if you have the <strings.h> header file.  */
#undef HAVE_STRINGS_H

/* Define if you have the <stropts.h> header file.  */
#undef HAVE_STROPTS_H

/* Define if you have the <sun/audioio.h> header file.  */
#undef HAVE_SUN_AUDIOIO_H

/* Define if you have the <sys/audioio.h> header file.  */
#undef HAVE_SYS_AUDIOIO_H

/* Define if you have the <sys/dir.h> header file.  */
#undef HAVE_SYS_DIR_H

/* Define if you have the <sys/ioctl.h> header file.  */
#undef HAVE_SYS_IOCTL_H

/* Define if you have the <sys/ipc.h> header file.  */
#undef HAVE_SYS_IPC_H

/* Define if you have the <sys/ndir.h> header file.  */
#undef HAVE_SYS_NDIR_H

/* Define if you have the <sys/param.h> header file.  */
#undef HAVE_SYS_PARAM_H

/* Define if you have the <sys/shm.h> header file.  */
#undef HAVE_SYS_SHM_H

/* Define if you have the <sys/soundcard.h> header file.  */
#undef HAVE_SYS_SOUNDCARD_H

/* Define if you have the <sys/time.h> header file.  */
#undef HAVE_SYS_TIME_H

/* Define if you have the <sys/types.h> header file.  */
// #undef HAVE_SYS_TYPES_H
#define HAVE_SYS_TYPES_H

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H

/* Define if you have the ICE library (-lICE).  */
#undef HAVE_LIBICE

/* Define if you have the X11 library (-lX11).  */
#undef HAVE_LIBX11

/* Define if you have the asound library (-lasound).  */
#undef HAVE_LIBASOUND

/* Define if you have the m library (-lm).  */
#undef HAVE_LIBM

/* Define if you have the socket library (-lsocket).  */
#undef HAVE_LIBSOCKET

/* Define if you have the vorbis library (-lvorbis).  */
#define HAVE_LIBVORBIS

/* Define if you have the vorbis library (-lgogo).  */
#define HAVE_LIBGOGO

/* Define if you havee the <mmsystem.h> header file for Win32. */
#define HAVE_MMSYSTEM_H

/* In VDS Macro AAA=BBB is not available. */
#define __W32__
#define TIMID_VERSION	"2.12.0-pre1"
#define DEFAULT_PATH	".\\"
#define AU_W32
#define AU_VORBIS
#define AU_GOGO
#define AU_GOGO_DLL
#define WINSOCK
#define __W32READDIR__
// #define URL_DIR_CACHE_ENABLE
#define ANOTHER_MAIN
#define __W32G__	/* for Win32 GUI */
#define SUPPORT_SOCKET
/*
  for Visual Studio Project Option 
  LIB: mmsystem.lib comdlg32.lib
  MACRO: _MT, _WINDOWS
  Multithread library
  */
