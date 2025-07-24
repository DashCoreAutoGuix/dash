/* confdefs.h */
#define PACKAGE_NAME "Dash Core"
#define PACKAGE_TARNAME "dashcore"
#define PACKAGE_VERSION "23.0.0"
#define PACKAGE_STRING "Dash Core 23.0.0"
#define PACKAGE_BUGREPORT "https://github.com/dashpay/dash/issues"
#define PACKAGE_URL "https://dash.org/"
#define HAVE_CXX20 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define STDC_HEADERS 1
#define HAVE_DLFCN_H 1
#define LT_OBJDIR ".libs/"
#define ENABLE_MINER 1
#define HAVE_EXECINFO_H 1
#define ENABLE_STACKTRACES 1
#define ENABLE_ARM_SHANI 1
#define HAVE_PTHREAD_PRIO_INHERIT 1
#define HAVE_PTHREAD 1
#define HAVE_DECL_STRERROR_R 1
#define HAVE_STRERROR_R 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_VMMETER_H 1
#define HAVE_DECL_GETIFADDRS 1
#define HAVE_DECL_FREEIFADDRS 1
#define HAVE_DECL_FORK 1
#define HAVE_DECL_SETSID 1
#define HAVE_DECL_PIPE2 0
#define HAVE_DEFAULT_VISIBILITY_ATTRIBUTE 1
#define HAVE_THREAD_LOCAL 1
#define HAVE_GETENTROPY_RAND 1
#define HAVE_SYSCTL 1
#define HAVE_FDATASYNC 0
#define HAVE_O_CLOEXEC 1
#define HAVE_SOCKADDR_UN 1
#define HAVE_SYSTEM 1
#define QT_QPA_PLATFORM_MINIMAL 1
#define QT_QPA_PLATFORM_COCOA 1
/* end confdefs.h.  */

        #include <db_cxx.h>

int
main (void)
{

        #if !((DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 8) || DB_VERSION_MAJOR > 4)
          #error "failed to find bdb 4.8+"
        #endif

  ;
  return 0;
}
