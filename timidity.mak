#
# Borland C++ IDE generated makefile
#
.AUTODEPEND


#
# Borland C++ tools
#
IMPLIB  = Implib
BCC32   = Bcc32 +BccW32.cfg 
TLINK32 = TLink32
TLIB    = TLib
BRC32   = Brc32
TASM32  = Tasm32
#
# IDE macros
#


#
# Options
#
IDE_LFLAGS32 =  -LD:\BC45\LIB
LC32No_debug = 
RC32No_debug = 
BC32No_debug =
LNATC32_timiditydexe = $(LC32No_debug)
RNATC32_timiditydexe = $(RC32No_debug)
BNATC32_timiditydexe = $(BC32No_debug)
LLATC32_timiditydexe =  -Tpe -ap -c
RLATC32_timiditydexe =  -w32
BLATC32_timiditydexe = 
CNIEAT_timiditydexe = -ID:\BC45\INCLUDE -DAU_WIN32;TIMID_VERSION="0.2i"
LNIEAT_timiditydexe = -x
LEAT_timiditydexe = $(LNATC32_timiditydexe) $(LLATC32_timiditydexe)
REAT_timiditydexe = $(RNATC32_timiditydexe) $(RLATC32_timiditydexe)
BEAT_timiditydexe = $(BNATC32_timiditydexe) $(BLATC32_timiditydexe)
CC32Optimized_lSizer =  -O-d -O -Ob -Os -Ol -k- -Z -4 -OW
LC32Optimized_lSizer =
RC32Optimized_lSizer = 
BC32Optimized_lSizer = 
CNATC32_timiditydc = $(CC32Optimized_lSizer)
LNATC32_timiditydc = $(LC32Optimized_lSizer)
RNATC32_timiditydc = $(RC32Optimized_lSizer)
BNATC32_timiditydc = $(BC32Optimized_lSizer)
CEAT_timiditydc = $(CEAT_timiditydexe) $(CNATC32_timiditydc) 
CNIEAT_timiditydc = -ID:\BC45\INCLUDE -DAU_WIN32;TIMID_VERSION="0.2i"
LNIEAT_timiditydc = -x
LEAT_timiditydc = $(LEAT_timiditydexe) $(LNATC32_timiditydc) 
REAT_timiditydc = $(REAT_timiditydexe) $(RNATC32_timiditydc) 
BEAT_timiditydc = $(BEAT_timiditydexe) $(BNATC32_timiditydc) 

#
# Dependency List
#
Dep_timidity = \
   timidity.exe

timidity : BccW32.cfg $(Dep_timidity)
  echo MakeNode 

Dep_timiditydexe = \
   common.obj\
   controls.obj\
	dumb_c.obj\
   filter.obj\
   getopt.obj\
   instrum.obj\
   mix.obj\
   output.obj\
   playmidi.obj\
   raw_a.obj\
   readmidi.obj\
   resample.obj\
   tables.obj\
   timidity.obj\
   wave_a.obj\
	win_a.obj\
	D:\BC45\LIB\32bit\wildargs.obj

timidity.exe : $(Dep_timiditydexe)
  $(TLINK32) @&&|
  $(IDE_LFLAGS32) $(LEAT_timiditydexe) $(LNIEAT_timiditydexe) +
D:\BC45\LIB\c0x32.obj+
common.obj+
controls.obj+
dumb_c.obj+
filter.obj+
getopt.obj+
instrum.obj+
mix.obj+
output.obj+
playmidi.obj+
raw_a.obj+
readmidi.obj+
resample.obj+
tables.obj+
timidity.obj+
wave_a.obj+
win_a.obj+
D:\BC45\LIB\32bit\wildargs.obj
$<,$*
D:\BC45\LIB\import32.lib+
D:\BC45\LIB\cw32.lib

|

common.obj :  common.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ common.c
|

controls.obj :  controls.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ controls.c
|

dumb_c.obj :  dumb_c.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ dumb_c.c
|

filter.obj :  filter.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ filter.c
|

getopt.obj :  getopt.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ getopt.c
|

instrum.obj :  instrum.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ instrum.c
|

mix.obj :  mix.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ mix.c
|

output.obj :  output.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ output.c
|

playmidi.obj :  playmidi.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ playmidi.c
|

raw_a.obj :  raw_a.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ raw_a.c
|

readmidi.obj :  readmidi.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ readmidi.c
|

resample.obj :  resample.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ resample.c
|

tables.obj :  tables.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ tables.c
|

timidity.obj :  timidity.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydc) $(CNIEAT_timiditydc) -o$@ timidity.c
|

wave_a.obj :  wave_a.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ wave_a.c
|

win_a.obj :  win_a.c
  $(BCC32) -P- -c @&&|
 $(CEAT_timiditydexe) $(CNIEAT_timiditydexe) -o$@ win_a.c
|

# Compiler configuration file
BccW32.cfg : 
   Copy &&|
-R
-v
-vi
-H
-H=timidity.csm
-H-
-v-
-R-
-WC
-ps
| $@


