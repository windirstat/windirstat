src = \
	lua_int64.c \
	lua_mtutil.c \
	lua_tstring.c \
	luawin_dllerror.c \
	win_privileges.c \
	win_registry.c \
	win_trace.c \
	winreg.c \

obj = \
	lua_int64.obj \
	lua_mtutil.obj \
	lua_tstring.obj \
	luawin_dllerror.obj \
	win_privileges.obj \
	win_registry.obj \
	win_trace.obj \
	winreg.obj \

!IFNDEF outfile 
outfile=winreg.dll 
!ENDIF 


# Edit this!
!IFDEF Lua5
lualib = \usr\local\lib\lua\5.0\lua50.lib
luainc = -I "\usr\local\include\lua\5.0"
!ELSE
lualib = \usr\local\lib\lua\5.1\lua51.lib
luainc = -I "\usr\local\include\lua\5.1"
!ENDIF

!IFDEF unicode
cuflags = -D UNICODE -D _UNICODE -UMBS -U_MBS
!ENDIF

!IFDEF nodebug
cdebug = -O2 -DNDEBUG -ML
ldebug = -DEBUG -OPT:REF -OPT:ICF
!ELSE
cdebug = -Z7 -Od -D_DEBUG -MLd
ldebug = -debug:full -debugtype:cv
!ENDIF

cvars = -DWIN32 -D_WIN32 -D_WINDOWS -DWIN32_LEAN_AND_MEAN -D_WINDLL -D_USRDLL
cflags = -EHsc -nologo -c -W4 -DCRTAPI1=_cdecl -DCRTAPI2=_cdecl -D_X86_=1 -D_WIN32_IE=0x0300 -DWINVER=0x0400 -I .
lflags = -INCREMENTAL:NO -NOLOGO -subsystem:windows,4.0 -DLL
libs = kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib version.lib

echo:
	@echo src = $(src)
	@echo obj = $(obj)
	@echo outfile = $(outfile)

build: compile link

compile:
	cl $(cdebug) $(cflags) $(cvars) $(cuflags) $(ciflags) $(luainc) $(src)

link:
	link $(ldebug) $(lflags) $(libs) $(obj) $(lualib) /OUT:"$(outfile)"

pack:
	$(packcmd) $(outfile)

clean:
	-del $(outfile)
	-del *.obj
	-del *.lib
	-del *.exp