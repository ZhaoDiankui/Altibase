/* libconfig.h.  Generated by configure.  */
/* libconfig.h.in.  Generated from configure.ac by autoheader.  */

#include <acpConfigPlatform.h>

/* Define to 1 if the `closedir' function returns void instead of `int'. */
/* #undef CLOSEDIR_VOID */

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
/* #undef CRAY_STACKSEG_END */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* Define to 1 if you have `alloca', as a function or macro. */
/* #define HAVE_ALLOCA 1 */

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
#if defined(ALTI_CFG_OS_HPUX) || defined(ALTI_CFG_OS_HPUX) ||  defined(ALTI_CFG_OS_SOLARIS )
 #define HAVE_ALLOCA_H 1
#endif

/* Define to 1 if you have the <curses.h> header file. */
#define HAVE_CURSES_H 1

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #define HAVE_DLFCN_H 1 */

/* Define to 1 if you have the `endpwent' function. */
/* #define HAVE_ENDPWENT 1 */

/* Define to 1 if you have the <fcntl.h> header file. */
/* #define HAVE_FCNTL_H 1 */

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the <inttypes.h> header file. */
/* #define HAVE_INTTYPES_H 1 */

/* Define to 1 if you have the `isascii' function. */
/* #define HAVE_ISASCII 1 */

/* Define to 1 if you have the `issetugid' function. */
/* #undef HAVE_ISSETUGID */

/* Define to 1 if you have the `curses' library (-lcurses). */
/* #define HAVE_LIBCURSES 1 */

/* Define to 1 if you have the `ncurses' library (-lncurses). */
/* #undef HAVE_LIBNCURSES */

/* Define to 1 if you have the <limits.h> header file. */
/* #define HAVE_LIMITS_H 1 */

/* Define to 1 if you have the <malloc.h> header file. */
/* #define HAVE_MALLOC_H 1 */

/* Define to 1 if you have the `memchr' function. */
/* #define HAVE_MEMCHR 1 */

/* Define to 1 if you have the <memory.h> header file. */
/* #define HAVE_MEMORY_H 1 */

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the <ncurses.h> header file. */
/* #define HAVE_NCURSES_H 1 */

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the `regcomp' function. */
/* #define HAVE_REGCOMP 1 */

/*  Define to 1 if you have the `re_comp' function. */ 
/* #define HAVE_RE_COMP 1 */

/* Define to 1 if `stat' has the bug that it succeeds when given the
   zero-length file name argument. */
/* #undef HAVE_STAT_EMPTY_STRING_BUG */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strcspn' function. */
#define HAVE_STRCSPN 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
/* #define HAVE_STRINGS_H 1 */

/* Define to 1 if you have the <string.h> header file. */ 
/* #define HAVE_STRING_H 1 */

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the <sys/cdefs.h> header file. */
/* #define HAVE_SYS_CDEFS_H 1 */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
/* #define HAVE_SYS_IOCTL_H 1 */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #define HAVE_SYS_PARAM_H 1 */

/* Define to 1 if you have the <sys/stat.h> header file. */
/* #define HAVE_SYS_STAT_H 1 */

/* Define to 1 if you have the <sys/types.h> header file. */
/* #define HAVE_SYS_TYPES_H 1 */

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
/* #define HAVE_SYS_WAIT_H 1 */

/* Define to 1 if you have the <termios.h> header file. */
/* #define HAVE_TERMIOS_H 1 */

/* Define to 1 if you have the <term.h> header file. */

/* BUG-13782 A4-AT-W 160 Warnings 2006.4.6 modified by Shawn
 * terms.h 를 include 하지 않아서 tputs, tgoto, tgetent, tgetflag, 등의
 * 함수가 implicit declaration warning 에 걸립니다. HAVE_TERM_H 를 사용하는
 * 곳은 ul 에서도 libedit 밖에 없는데다가,
 * 이 파일, libconfig.h 를 직접 include 하는 것 또한 ul 밖에 없는고로
 * HAVE_TERM_H 를 주석풀고 1 로 세팅합니다.
 */
#define HAVE_TERM_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if the system has the type `u_int32_t'. */
#define HAVE_U_INT32_T 1

/* Define to 1 if you have the `vfork' function. */
#define HAVE_VFORK 1

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef HAVE_VFORK_H */

/* Define to 1 if `fork' works. */
#define HAVE_WORKING_FORK 1

/* Define to 1 if `vfork' works. */
#define HAVE_WORKING_VFORK 1

/* Define to 1 if `lstat' dereferences a symlink specified with a trailing
   slash. */
#define LSTAT_FOLLOWS_SLASHED_SYMLINK 1

/* Name of package */
#define PACKAGE "libedit-20050311"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "libedit"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libedit 2.9"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libedit-20050311"

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.9"

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "2.9"

/* Solaris's term.h does horrid things. */
#if defined(SPARC_SOLARIS) || defined(X86_SOLARIS)
#define  _SUNOS
#endif

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define as `fork' if `vfork' does not work. */
/* #undef vfork */

#include <sys.h>
