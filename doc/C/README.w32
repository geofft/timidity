How to compile TiMidity++ for Windows

This file contains fllowing instructions.

1.Mingw in Msys environment
2.Turbo C++ in Msys environment
3.OpenWatcom C++ in Msys environment
4.Visual C++ in Msys environment
5.Digital Mars in Msys environment
6.Pelles C in Msys environment

1.Mingw in Msys environment

(1)Setup Mingw and MSYS
    1)Setup Mingw(http://sourceforge.net/projects/mingw/) 
      and MSYS (See MingwWiki(http://mingw.sourceforge.net/MinGWiki/))
(2)Setup needed libraries
        2-0)get pexports from Mingw
            "pexports" is included in mingw-utils-0.3.tar.gz, get it and extract it.
        2-1)pdcurses
            Get pdcurses-2.6.0-2003.07.21-1.exe from Mingw. and extract them.
            Move curses.h to include path and libcurses.a libarary path.
        2-2)oggvorbis(http://www.vorbis.com/)
            get OggVorbis-win32sdk-1.0.1.zip and extract it.
            make export libraries
                pexports xxxx.dll >xxxx.def
                dlltool --dllname xxxx.dll --input-def xxxx.def --output-lib libxxxx.a
            Change include\ogg\os_type.h line 29 as following
                (os_types.h)
                29 #  if !defined(__GNUC__) || defined(__MINGW32__)
            set environment variables in batch file like this.
                REM OggVorbis
                set PATH=\usr\local\oggvorbis-win32sdk-1.0.1\bin;\usr\local\oggvorbis-win32sdk-1.0.1\lib;%PATH%
                set C_INCLUDE_PATH=/usr/local/oggvorbis-win32sdk-1.0.1/include:%C_INCLUDE_PATH
                set LD_LIBRARY_PATH=/usr/local/oggvorbis-win32sdk-1.0.1/lib:%LD_LIBRARY_PATH%

        2-3)gogo no coder(http://www.marinecat.net/mct_top.htm)
            get gogo.dll from Gogo no Coder(http://www.marinecat.net/cgi/lcount/count.cgi?page=3112&name=wing311.exe&downex=wing311a.exe)
            get gogo.h files from Gogo noCoder source(http://www.marinecat.net/cgi/lcount/count.cgi?page=3111&name=petit311-src.lzh&downex=petit311.lzh)
            move gogo.h gogo\include\gogo
            move gogo.dll gogo\lib
            make export libraries
                pexports gogo.dll >gogo.def
                dlltool --dllname gogo.dll --input-def gogo.def --output-lib libgogo.a
            set environment variables in batch file like this.
                REM GOGO
                set PATH=\usr\local\gogo\bin;\usr\local\gogo\lib;%PATH%
                set C_INCLUDE_PATH=/usr/local/gogo/include:%C_INCLUDE_PATH%
                set LD_LIBRARY_PATH=/usr/local/gogo/lib:%LD_LIBRARY_PATH%
        2-4)flac(http://flac.sourceforge.net/)
            Get "http://downloads.sourceforge.net/flac/flac-1.2.1-devel-win.zip"  and extract it.
            Change include\*\export.h line 58 as following
              (export.h)
              58 #if defined(FLAC__NO_DLL) || !defined(_MSC_VER) \
              || !defined(__BORLANDC__) || !defined(__CYGWIN32__) || !defined(__MINGW32__)
            set environment variables in batch file like this.
              REM FLAC
              set PATH=\usr\local\flac-1.2.1-devel-win\lib;;%PATH%
              set C_INCLUDE_PATH=/usr/local/flac-1.2.1-devel-win/include:%C_INCLUDE_PATH%
              set LD_LIBRARY_PATH=/usr/local/flac-1.2.1-devel-win/bin:%LD_LIBRARY_PATH%	

        2-5)portaudio(http://www.portaudio.com/)
            Download portaudio v1.19. and extarct it.
            Move portaudio.h to include path.
            Get portaudio.dll from Csound5.08.2-gnu-win32-f.exe(http://sourceforge.net/project/showfiles.php?group_id=81968). 
			Rename portaudio.dll to libportaudio.dll and move it to library path.
            Only portaudo.h is needed for compiling TiMidity++.

(3)Make TiMidity++ binaries
        3-1)timw32g.exe
            (configure)
             CFLAGS="-O2" ./configure --enable-network --enable-w32gui --enable-spline=gauss \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
             strip timidity.exe
             mv timidity.exe timw32g.exe
        3-2)twsyng.exe
            (configure)
             CFLAGS="-O2" ./configure --enable-network --enable-winsyng --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
             strip timidity.exe
             mv timidity.exe twsyng.exe

        3-3)twsynsrv.exe
            (configure)
              CFLAGS="-O2" ./configure --enable-network --enable-winsyng --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
             add config.h following line
                #define TWSYNSRV 1
           (make)
             make
             strip timidity.exe
             mv timidity.exe twsynsrv.exe

        3-4)timidity.exe
            (configure)
             CFLAGS="-O2" ./configure --enable-interface=ncurses,vt100,winsyn --enable-network --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
             strip timidity.exe
        3-5)timiditydrv.dll
            (configure)
             CFLAGS="-O2" ./configure --enable-winsyn --enable-windrv --enable-spline=linear \
             --enable-audio=w32,portaudio
            (make)
             make
             cd windrv
             strip timiditydrv.dll


2.Turbo C++ in Msys environment

(1)Setup BorlandC and MSYS
    1)Setup BorlandC
        Downlod Turbo C++ Explorer(http://www.codegear.com/jp/downloads/free/turbo)
        Write bcc32.cfg and ilink 32.cfg.
        (bcc32.cfg)
          -IC:\Borland\BDS\4.0\include
          -LC:\Borland\BDS\4.0\lib
          -LC:\Borland\BDS\4.0\lib\psdk
          -DWINVER=0x0400
          -D_WIN32_WINNT=0x0400
        (ilink32.cfg )
            -LC:\Borland\BDS\4.0\lib;C:\Borland\BDS\4.0\lib\psdk
        ** Caution !! **
          Folder name which include '-'s is not acceptable.
          ilink32 can't understand such folder name.
    2)Setup  MSYS (See MingwWiki(http://mingw.sourceforge.net/MinGWiki/))
        Remove mingw path from /etc/fstab.
        Add fllowing line at the top of Msys.bat and create bcc_env.bat.
              set PATH=C:\Borland\BDS\4.0\bin;%PATH%

(2)Setup needed libraries
        2-0-1)implib.exe(to make import library from dll)
           implib -a -c xxx.lib xxx.dll
        2-0-2)coff2omf.exe(VCC library to BCC library)
           coff2omf  xxxx.lib xxx_bcpp.lib.
        
        2-1)pdcurses
           pdcurses-2.6.0-src.tar.bz2 from Mingw(http://sourceforge.net/project/showfiles.php?group_id=2435)
           and make them.
           rename pdcurses.lib libpdcurses.lib
           Add include path and library path bcc32.cfg. 
           Add library path in ilink32.cfg
        2-2)oggvorbis(http://www.vorbis.com/)
           get OggVorbis-win32sdk-1.0.1.zip and extract it.
           Add include path and library path bcc32.cfg. 
           Add library path in ilink32.cfg
           Only hederfiles are need for compiling TiMidity++
        2-3)gogo no coder(http://www.marinecat.net/mct_top.htm)
           get gogo.dll from Gogo no Coder(http://www.marinecat.net/cgi/lcount/count.cgi?page=3112&name=wing311.exe&downex=wing311a.exe)
           get gogo.h files from Gogo no Coder source(http://www.marinecat.net/cgi/lcount/count.cgi?page=3111&name=petit311-src.lzh&downex=petit311.lzh)
           move gogo.h gogo\include\gogo
           (for timidity gogo.lib is not necessary)
           Add include path and library path bcc32.cfg. 
           Add library path in ilink32.cfg
           Only hederfiles are need for TiMidity++
        2-5)flac(http://flac.sourceforge.net/)
           get "http://downloads.sourceforge.net/flac/flac-1.2.1-devel-win.zip" and extract it.
           Change include\*\export.h line 58 as following
                (export.h)
                58 #if defined(FLAC__NO_DLL) || !defined(_MSC_VER) \
                    || !defined(__BORLANDC__) || !defined(__CYGWIN32__) || !defined(__MINGW32__)
           Add include path and library path bcc32.cfg. 
           Add library path in ilink32.cfg
        2-6)portaudio(http://www.portaudio.com/)
             I don't know how to comple portaudio with bcc commandline tools.
             Only portaudio.h is needed for compiling TiMidity++.
             You can get portaudio.dll from Csound5.08.2-gnu-win32-f.exe(http://sourceforge.net/project/showfiles.php?group_id=81968).
             
(3)Make TiMidity++ binaries
        3-0) perl -pe 's/CC\s-o\s\S\S*\s/CC /g' configure >configure_bc
                ( -o xxxx options are not work correctly with BCC)
                
        3-1)timw32g.exe
            (configure)
             CC="bcc32" CPP="cpp32" CFLAGS="" ./configure_bc  --enable-w32gui --enable-spline=gauss \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
             mv timidity.exe timw32g.exe
        3-2)twsyng.exe
            (configure)
            CC="bcc32" CPP="cpp32" CFLAGS="" \
            ./configure_bc --enable-network --enable-winsyng --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
             mv timidity.exe twsyng.exe
        3-3)twsynsrv.exe
            (configure)
             CC="bcc32" CPP="cpp32" CFLAGS=""\
             ./configure_bc --enable-network --enable-winsyng --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
             add config.h following line
                #define TWSYNSRV 1
             (make)
             make
             mv timidity.exe twsynsrv.exe
       3-4)timidity.exe
            (configure)
            CC="bcc32" CPP="cpp32" CFLAGS="" \
             ./configure_bc --enable-interface=vt100,winsyn,ncurses --enable-network --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
       3-5)timiditydrv.dll
            (configure)
            CC="bcc32" CPP="cpp32" CFLAGS="" \
             ./configure_bc  --enable-winsyn --enable-windrv --enable-spline=linear \
             --enable-audio=w32,portaudio
            (make)
             make


3.OpenWatcom C++ in Msys environment

(1)OpenWatcom and MSYS
    1)Set OpenWatcom(http://www.openwatcom.org)
       1-1)Download OpenWatcom, install and configure them
       1-2)Download Microsoft Platform SDK install and configure them.
            Replace rc.exe of OpenWatom with the one of Platform SDK's one.
            or get wrc.exe from wine-prgs-0.9.5.zip(http://www.winehq.com/), and rename rc.exe
            (OpenWatcom's rc.exe can't use).
   2)Setup Mngw and MSYS (See MingwWiki(http://mingw.sourceforge.net/MinGWiki/))
        Remove mingw path from /etc/fstab.
        Add fllowing line at the top of Msys.bat and create wcc_env.bat.
              call wcc_env.bat
        wcc_env.bat is like follow ing
            @echo off
            set LIB=
            set INCLUDE=
            call i:\watcom\setvars.bat
                 ----
               ( setteing of various env. val. s )
                  ----
(2)Setup needed libraries(The wcc386 option -5s is recomended !! Because of TiMidity++ compatibility.!!)
        2-0) make import Libray from dll
           mlib -n -b out.lib in.dll
        2-1)pdcurses
           Get  pdcurses-2.5.0  from GnuWin32(http://sourceforge.net/projects/gnuwin32/)
           and make import Libraries from dlls.
           "libpdcurses.lib"  is needed (not pdcurses.lib).
           Move curses.h to include path and libpdcurses.lib libarary path.
        2-2)oggvorbis(http://www.vorbis.com/)
           Get OggVorbis-win32sdk-1.0.1.zip and extract it.
           Edit include\ogg\os_types.h.
              (os_types.h)
              29 #  if defined(__WATCOMC__)
              30 /* MSVC/Borland */
              31 typedef __int64 ogg_int64_t;
              32 typedef int ogg_int32_t;
              33 typedef unsigned int ogg_uint32_t;
              34 typedef short ogg_int16_t;
              35 typedef unsigned short ogg_uint16_t;
              36 #  else
                   -----
              52 #  endif
           Make inport Libraries from dll
           Set environment variables in batch file like this.
                REM OggVorbis
                set PATH=\usr\local\oggvorbis-win32sdk-1.0.1_wcc\bin;\usr\local\oggvorbis-win32sdk-1.0.1_wcc\lib;%PATH%
                set C_INCLUDE_PATH=/usr/local/oggvorbis-win32sdk-1.0.1_wcc/include:%C_INCLUDE_PATH
                set LIB=\usr\local\oggvorbis-win32sdk-1.0.1_wcc\lib:%LIB%
           Only *.h files are needed for TiMidity++.
        2-3)gogo no coder(http://www.marinecat.net/mct_top.htm)
           Get gogo.dll from Gogo no Coder(http://www.marinecat.net/cgi/lcount/count.cgi?page=3112&name=wing311.exe&downex=wing311a.exe)
            Get gogo.h files from Gogo noCoder source(http://www.marinecat.net/cgi/lcount/count.cgi?page=3111&name=petit311-src.lzh&downex=petit311.lzh)
            Move gogo.h gogo\include\gogo
            (for timidity gogo.lib not necessary)
           Set environment variables in batch file like this.
                REM GOGO
                set PATH=\usr\local\gogo_wcc\bin;\usr\local\gogo_wcc\lib;%PATH%
                set C_INCLUDE_PATH=/usr/local/gogo_wcc/include:%C_INCLUDE_PATH%
                set LIB=\usr\local\gogo_wcc\lib:%LIB%
           Only *.h files are needed for compiling TiMidity++.
        2-5)flac(http://flac.sourceforge.net/)
            get "http://downloads.sourceforge.net/flac/flac-1.2.1-devel-win.zip" and extract it.
            make inport Libraries from dll
            set environment variables in batch file like this.
                REM FLAC
                set PATH=\usr\local\flac-1.2.1-win_wcc\bin;%PATH%
                set C_INCLUDE_PATH=/usr/local/flac-1.2.1-win_wcc/include:%C_INCLUDE_PATH%
                set LIB=\usr\local\flac-1.2.1-win_wcc\bin:%LIB%
           Only *.h files are needed for compiling TiMidity++.
        2-6)portaudio(http://www.portaudio.com/)
            I don't know how to comple portaudio with OpenWatcom commandline tools.
            Only port audio.h is needed for compling TiMidity++.
            You can get portaudio.dll from Csound5.08.2-gnu-win32-f.exe(http://sourceforge.net/project/showfiles.php?group_id=81968).

(3)Make TiMidity++ binaries
        3-0-1)wcc386_w.sh & wpp386_w.sh( They are in scripts/directory )
            Wcc386 is not familier to GNU autoconf tools,
            Use my wrapper wcc386_w.sh, instead of wcc386.exe
        3-1)timw32g.exe
            (configure)
            CC="wcc386_w.sh" CPP="wcc386_w.sh -p"  CFLAGS="-d0 -obll+riemcht" \
           ./configure --enable-network --enable-w32gui --enable-spline=gauss \
            --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio \
            --disable-oggtest --disable-vorbistest --disable-libFLACtest --disable-libOggFLACtest
            (make)
             make
             mv timidity.exe timw32g.exe
        3-2)twsyng.exe
            (configure)
            CC="wcc386_w.sh" CPP="wcc386_w.sh -p"  CFLAGS="-d0 -obll+riemcht" \
            ./configure --enable-network --enable-winsyng --enable-spline=linear \
            --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio \
            --disable-oggtest --disable-vorbistest --disable-libFLACtest --disable-libOggFLACtest
             (make)
             make
             mv timidity.exe twsyng.exe
        3-3)twsynsrv.exe
            (configure)
            CC="wcc386_w.sh" CPP="wcc386_w.sh -p"  CFLAGS="-d0 -obll+riemcht" \
            ./configure --enable-network --enable-winsyng --enable-spline=linear \
            --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio \
            --disable-oggtest --disable-vorbistest --disable-libFLACtest --disable-libOggFLACtest
            add config.h following line
                #define TWSYNSRV 1
             (make)
             make
             mv timidity.exe twsynsrv.exe
        3-4)timidity.exe
            (configure)
            CC="wcc386_w.sh" CPP="wcc386_w.sh -p"  CFLAGS="-d0 -obll+riemcht" \
            ./configure --enable-interface=ncurses,vt100,winsyn --enable-network --enable-spline=linear \
            --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio \
            --disable-oggtest --disable-vorbistest --disable-libFLACtest --disable-libOggFLACtest
            (make)
            make
       3-5)timiditydrv.dll
            (configure)
            CC="wcc386_w.sh" CPP="wcc386_w.sh -p"  CFLAGS="-d0 -obll+riemcht" \
             ./configure  --enable-winsyn --enable-windrv --enable-spline=linear \
             --enable-audio=w32,portaudio
            (make)
             make


4.Visual C++ in Msys environment

(1)Setup VisualC++ and MSYS
    1)Setup Visual C++
        Download
            Visual C++ 2008 Express Edition(http://www.microsoft.com/japan/msdn/vstudio/express/default.aspx)
            Microsoft Platform SDK
        install and configure them.

   2)Setup Mngw and MSYS (See MingwWiki(http://mingw.sourceforge.net/MinGWiki/))
        Remove mingw path from /etc/fstab.
        Add fllowing line at the top of Msys.bat and create bcc_env.bat.
              call vc_msys_env.bat
        vc_msys_env.bat is like follow ing
            @echo off
            call c:\"Program Files\Microsoft Platform SDK"\SetEnv.Cmd /2000 /RETAIL
            call c:"\Program Files\Microsoft Visual Studio 9.0"\Common7\Tools\vsvars32.bat

            Set INCLUDE=c:\DXSDK\include;%INCLUDE%
            Set LIB=c:\DXSDK\lib;%LIB%

                  ----
               ( setteing of various env. val. s )
                  ----
(2)Setup needed libraries
        2-1)pdcurses
           pdcurses-2.6.0-src.tar.bz2 from Mingw(http://sourceforge.net/project/showfiles.php?group_id=2435)
           and make them.
           rename pdcurses.lib libpdcurses.lib
           move curses.h to include path and libpdcurses.lib libarary path.
        2-2)oggvorbis(http://www.vorbis.com/)
           get OggVorbis-win32sdk-1.0.1.zip and extract it.
           set environment variables in batch file like this.
                REM OggVorbis
                set PATH=\usr\local\oggvorbis-win32sdk-1.0.1\bin;\usr\local\oggvorbis-win32sdk-1.0.1\lib;%PATH%
                set C_INCLUDE_PATH=/usr/local/oggvorbis-win32sdk-1.0.1/include:%C_INCLUDE_PATH
                set LIB=\usr\local\oggvorbis-win32sdk-1.0.1\lib:%LIB%

        2-3)gogo no coder(http://www.marinecat.net/mct_top.htm)
           Get gogo.dll from Gogo no Coder(http://www.marinecat.net/cgi/lcount/count.cgi?page=3112&name=wing311.exe&downex=wing311a.exe)
           Get gogo.h files from Gogo noCoder source(http://www.marinecat.net/cgi/lcount/count.cgi?page=3111&name=petit311-src.lzh&downex=petit311.lzh)
           Move gogo.h gogo\include\gogo
            (for timidity gogo.lib not necessary)
           Set environment variables in batch file like this.
                REM GOGO
                set PATH=\usr\local\gogo\bin;\usr\local\gogo\lib;%PATH%
                set C_INCLUDE_PATH=/usr/local/gogo/include:%C_INCLUDE_PATH%
                set LIB=\usr\local\gogo\lib:%LIB%
        2-5)flac(http://flac.sourceforge.net/)
            Get "http://downloads.sourceforge.net/flac/flac-1.2.1-devel-win.zip"  and extract it.
            Set environment variables in batch file like this.
                REM FLAC
                set PATH=\usr\local\flac-1.2.1-win\bin;;%PATH%
                set C_INCLUDE_PATH=/usr/local/flac-1.2.1-win/include:%C_INCLUDE_PATH%
                set LIB=\usr\local\flac-1.2.1-win\bin:%LIB%
        2-6)portaudio(http://www.portaudio.com/).
             I don't know how to comple portaudio with vc commandline tools.
            But for compling TiMidity++ only portaudio.h is needed.
            You can get portaudio.dll from Csound5.08.2-gnu-win32-f.exe(http://sourceforge.net/project/showfiles.php?group_id=81968).

(3)Make TiMidity++ binaries
        3-1)timw32g.exe
            (configure)
             CC="cl" CPP="cl.exe -EP"  CFLAGS="-O2" \
            ./configure --enable-network --enable-w32gui --enable-spline=gauss \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
            make
             mv timidity.exe timw32g.exe
        3-2)twsyng.exe
            (configure)
            CC="cl" CPP="cl.exe -EP"  CFLAGS="-O2" \
            ./configure --enable-network --enable-winsyng --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
             mv timidity.exe twsyng.exe
        3-3)twsynsrv.exe
            (configure)
             CC="cl" CPP="cl.exe -EP"  CFLAGS="-O2"\
             ./configure --enable-network --enable-winsyng --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
             add config.h following line
                #define TWSYNSRV 1
             (make)
             make
             mv timidity.exe twsynsrv.exe
        3-4)timidity.exe
            (configure)
            CC="cl" CPP="cl.exe -EP"  CFLAGS="-O2" \
             ./configure --enable-interface=ncurses,vt100,winsyn --enable-network --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
        3-5)timiditydrv.dll
            (configure)
            CC="cl" CXX="cl" CPP="cl.exe -EP"  CFLAGS="-O2" \
              ./configure --enable-winsyn --enable-windrv --enable-spline=linear \
               --enable-audio=w32,portaudio
            (make)
             make


5.Digital Mars in Msys environment

(1) Digtal Mars & MSYS 
	1) Installing up Digtal Mars(http://www.digitalmars.com/)
		1-1) Download Digtal Mars.
		2-2) replace rc.exe by that from Microsoft Platform SDK.
		（ rc.exe included  Digital Mars package is not suitable for compiling TiMidity.）
	2)Setting up MSYS(http://sourceforge.net/projects/mingw/　http://mingw.sourceforge.net/MinGWiki/))
		Remove mingw concening path from "/etc/fstab".
		Make dm_env.bat(like following file) is called msys.bat.
		(Add top of msys.bat）
			call dm_env.bat
		(dm_env.bat example）
			set LIB=
			set INCLUDE=

			Set PATH=i:\dm\bin;%PATH%
			Set INCLUDE=i:\dm\include;i:\dm\include\win32;%INCLUDE%
			Set LIB=i:\dm\lib;%LIB%
			
			Set PATH=i:\usr\local\gogo\bin;%PATH%
			Set INCLUDE=i:\usr\local\gogo\include;%INCLUDE%
			Set LIB=i:\usr\local\gogo\lib;%LIB%
				（続く）
				 ----
(2) Setup Various Libraries
	2-0) How to make import libraries from dlls
		implib out.lib in.dll
	2-1) pcurses
		"Get pdcurses-2.5.0" from GnuWin32(http://sourceforge.net/projects/gnuwin32/).
		Change curses.h as following
		281 #if defined( _MSC_VER )|| defined(__DMC__)       /* defined by compiler */
		977 #if !defined(PDC_STATIC_BUILD) && (defined(_MSC_VER) || defined(__DMC__))&& defined(WIN32) && !defined(CURSES_LIBRARY)
		988 # if !defined(PDC_STATIC_BUILD) && (defined(_MSC_VER) || defined(__DMC__)) && defined(WIN32)
		Make import library with system option as follows.
			$implib /system libpdcurses.lib pdcurses.dll
		Change name of pccurses.lil to libpdcuses.lib/

    2-2) oggvorbis(http://www.vorbis.com/)
    	Use "OggVorbis-win32sdk-1.0.1.zip".
           Edit include\ogg\os_types.h as follows.
              (os_types.h)
              36 #  elif defined(__MINGW32__) || defined(__DMC__)
		Make import libraries from dlls

    2-3) Gogo no Coder (mp3 encoding dll) (http://www.marinecat.net/mct_top.htm)
           Get gogo.dll from Gogo no Coder(http://www.marinecat.net/cgi/lcount/count.cgi?page=3112&name=wing311.exe&downex=wing311a.exe)
           Get gogo.h files from Gogo noCoder source(http://www.marinecat.net/cgi/lcount/count.cgi?page=3111&name=petit311-src.lzh&downex=petit311.lzh)
		   Make gogo.lib from gogo.dll
    	   Move gogo.h gogo\include\gogo
    	   Move gogo.dll libgogo.a gogo\lib
		
	2-4) flac(http://flac.sourceforge.net/)
		Use "http://downloads.sourceforge.net/flac/flac-1.2.1-devel-win.zip" 
		Edit line 58 of include\*\export.h s.
			(export.h)
			58 #if defined(FLAC__NO_DLL) || !defined(_MSC_VER) \
				|| !defined(__BORLANDC__) || !defined(__CYGWIN32__) || !defined(__MINGW32__) \
				|| !defined(__WATCOMC__) || !defined(__DMC__)
		Make import libraries from dlls.

	2-5) portaudio(http://www.portaudio.com/)
		Only portaudio.h is required for compiling of TiMidity++
        You can get portaudio.dll from Csound5.08.2-gnu-win32-f.exe(http://sourceforge.net/project/showfiles.php?group_id=81968).


(3) Compiling TiMIdity++
        3-0-1) "LF -> CRLF" conversion
           $sh script/unix2dos.sh
        3-0-1) perl -pe 's/CC\s-o\s\S\S*\s/CC /g' configure |perl -pe 's/CXX\s-o\s\S\S*\s/CXX /g' - >configure_dm
                (because "-o xxxx" is not suitbable dmc.exe )
        3-0-2) Get cpp.exe from Free Pascal Compiler(http://www.freepascal.org/),
             and rename it as fpcpp.exe. 
            （preprocessor of dmc.exe is not compatible "gnu autotools"）

        3-1)timw32g.exe
            (configure)
            CC="dmc -Jm -w -mn -5 -o" CPP="fpcpp -D__NT__ -I/i/dm/include" \
             ./configure_dm --enable-network --enable-w32gui --enable-spline=gauss \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
             mv timidity.exe timw32g.exe
        3-2)twsyng.exe
            (configure)
            CC="dmc -Jm -w -mn -5 -o" CPP="fpcpp -D__NT__ -I/i/dm/include" \
             ./configure_dm --enable-network --enable-winsyng --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
             (make)
             make
             mv timidity.exe twsyng.exe
        3-3)twsynsrv.exe
            (configure)
            CC="dmc -Jm -w -mn -5 -o" CPP="fpcpp -D__NT__ -I/i/dm/include" \
             ./configure_dm --enable-network --enable-winsyng --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            add config.h の最後に以下の行を追加。
                #define TWSYNSRV 1
             (make)
             make
             mv timidity.exe twsynsrv.exe
        3-4)timidity.exe
            (configure)
            CC="dmc -Jm -w -mn -5 -o" CPP="fpcpp -D__NT__ -I/i/dm/include" \
             ./configure_dm --enable-interface=ncurses,vt100,winsyn --enable-network --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
            make
       3-5)timiditydrv.dll
            (configure)
            CC="dmc -Jm -w -mn -5 -o" CPP="fpcpp -D__NT__ -I/i/dm/include" \
             ./configure_dm --enable-interface=windrv,winsyn --enable-network --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
            make
              commentout timiditydrv.h 23:
              23 //#error this stub requires an updated version of <rpcndr.h>
           	make



6.Pelles C in Msys environment

(1)Setting up  Pelles C and MSYS
	1) Pelles C
		Get Pelles C  from （http://www.smorgasbordet.com/pellesc/).
	２）MSYS(http://sourceforge.net/projects/mingw/  http://mingw.sourceforge.net/MinGWiki/))
		Remove pathes concerning with MINGW from "/etc/fstab".
		Call new file "pocc_env.bat" from msys.bat.
		(Top of msys.bat）
			call pocc_env.bat
		("vcc_env.bat" example）
			@echo off
			call call c:\PellesC\bin\povars32.bat

			Set INCLUDE=c:\DXSDK\include;%INCLUDE%
			Set LIB=c:\DXSDK\lib;%LIB%
				... continue ...
				 ----
(2) Setting up various libraries
	2-1) pcurses
		Get "pdcurses-2.6.0-src.tar.bz2" from "Mingw' web site", and compile it.
		Rname pccurses.lib to libpdcuses.lib.

    2-2) oggvorbis(http://www.vorbis.com/)
    	Get "OggVorbis-win32sdk-1.0.1.zip”(http://www.vorbis.com/files/1.0.1/windows/OggVorbis-win32sdk-1.0.1.zip)

    2-3) Gogo no Coder (mp3 encoding dll) (http://www.marinecat.net/mct_top.htm)
           Get gogo.dll from Gogo no Coder(http://www.marinecat.net/cgi/lcount/count.cgi?page=3112&name=wing311.exe&downex=wing311a.exe)
           Get gogo.h files from Gogo noCoder source(http://www.marinecat.net/cgi/lcount/count.cgi?page=3111&name=petit311-src.lzh&downex=petit311.lzh)
		   Make gogo.lib from gogo.dll
    	   Move gogo.h gogo\include\gogo
    	   Move gogo.dll libgogo.a gogo\lib
			
	2-4) flac(http://flac.sourceforge.net/)
		Use "http://downloads.sourceforge.net/flac/flac-1.2.1-devel-win.zip".
		Edit "line 58"s of include\*\export.h as follows.
			(export.h)
			58  #if defined(FLAC__NO_DLL) || !defined(_MSC_VER) \
				|| !defined(__BORLANDC__) || !defined(__CYGWIN32__) || !defined(__MINGW32__) \
				|| !defined(__WATCOMC__) || !defined(__DMC__)
		
	2-5) portaudio(http://www.portaudio.com/)
		Only "portaudio.h" is requierd for compiling.
        You can get portaudio.dll from Csound5.08.2-gnu-win32-f.exe(http://sourceforge.net/project/showfiles.php?group_id=81968).

(3)Compiling TiMIdity++
		3-0) perl -pe 's/CC\s-o\s\S\S*\s/CC /g' configure >configure_pocc
                (" -o xxxx " in configure is not acceptable by pocc)
        3-1)timw32g.exe
            (configure)
            CC="cc" CPP="pocc.exe -E"  CFLAGS="-MT" ./configure_pocc  \
            --enable-network --enable-w32gui --enable-spline=gauss \
            --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
            make
             mv timidity.exe timw32g.exe
        3-2)twsyng.exe
            (configure)
            CC="cc" CPP="pocc.exe -E"  CFLAGS="-MT" ./configure_pocc  \
            --enable-network --enable-winsyng --enable-spline=linear \
            --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
             mv timidity.exe twsyng.exe
        3-3)twsynsrv.exe
            (configure)
             CC="cc" CPP="pocc.exe -E"  CFLAGS="-MT" ./configure_pocc  \
             --enable-network --enable-winsyng --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
             add config.h following line
                #define TWSYNSRV 1
             (make)
             make
             mv timidity.exe twsynsrv.exe
        3-4)timidity.exe
            (configure)
            CC="cc" CPP="pocc.exe -E"  CFLAGS="-MT" ./configure_pocc  \
            --enable-interface=ncurses,vt100,winsyn --enable-network --enable-spline=linear \
             --enable-audio=w32,vorbis,gogo,ogg,flac,portaudio
            (make)
             make
        3-5)timiditydrv.dll
            (configure)
            CC="cc" CPP="pocc.exe -E"  CFLAGS="-MT" ./configure_pocc  \
            --enable-winsyn --enable-windrv --enable-spline=linear \
              --enable-audio=w32,portaudio
            (make)
             make


2008.4.10 Keishi Suenaga(skeishi@mutt.freemail.ne.jp)
