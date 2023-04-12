BEGIN {
D["PACKAGE_NAME"]=" \"Dash Core\""
D["PACKAGE_TARNAME"]=" \"dashcore\""
D["PACKAGE_VERSION"]=" \"23.0.0\""
D["PACKAGE_STRING"]=" \"Dash Core 23.0.0\""
D["PACKAGE_BUGREPORT"]=" \"https://github.com/dashpay/dash/issues\""
D["PACKAGE_URL"]=" \"https://dash.org/\""
D["HAVE_CXX20"]=" 1"
D["HAVE_STDIO_H"]=" 1"
D["HAVE_STDLIB_H"]=" 1"
D["HAVE_STRING_H"]=" 1"
D["HAVE_INTTYPES_H"]=" 1"
D["HAVE_STDINT_H"]=" 1"
D["HAVE_STRINGS_H"]=" 1"
D["HAVE_SYS_STAT_H"]=" 1"
D["HAVE_SYS_TYPES_H"]=" 1"
D["HAVE_UNISTD_H"]=" 1"
D["STDC_HEADERS"]=" 1"
D["HAVE_DLFCN_H"]=" 1"
D["LT_OBJDIR"]=" \".libs/\""
D["ENABLE_MINER"]=" 1"
D["HAVE_EXECINFO_H"]=" 1"
D["ENABLE_STACKTRACES"]=" 1"
D["ENABLE_ARM_SHANI"]=" 1"
D["HAVE_PTHREAD_PRIO_INHERIT"]=" 1"
D["HAVE_PTHREAD"]=" 1"
D["HAVE_DECL_STRERROR_R"]=" 1"
D["HAVE_STRERROR_R"]=" 1"
D["HAVE_SYS_SELECT_H"]=" 1"
D["HAVE_SYS_VMMETER_H"]=" 1"
D["HAVE_DECL_GETIFADDRS"]=" 1"
D["HAVE_DECL_FREEIFADDRS"]=" 1"
D["HAVE_DECL_FORK"]=" 1"
D["HAVE_DECL_SETSID"]=" 1"
D["HAVE_DECL_PIPE2"]=" 0"
D["HAVE_DEFAULT_VISIBILITY_ATTRIBUTE"]=" 1"
D["HAVE_THREAD_LOCAL"]=" 1"
D["HAVE_GETENTROPY_RAND"]=" 1"
D["HAVE_SYSCTL"]=" 1"
D["HAVE_FDATASYNC"]=" 0"
D["HAVE_O_CLOEXEC"]=" 1"
D["HAVE_SOCKADDR_UN"]=" 1"
D["HAVE_SYSTEM"]=" 1"
D["QT_QPA_PLATFORM_MINIMAL"]=" 1"
D["QT_QPA_PLATFORM_COCOA"]=" 1"
D["USE_BDB"]=" 1"
D["USE_SQLITE"]=" 1"
D["HAVE_MINIUPNPC_MINIUPNPC_H"]=" 1"
D["HAVE_MINIUPNPC_UPNPCOMMANDS_H"]=" 1"
D["HAVE_MINIUPNPC_UPNPERRORS_H"]=" 1"
D["HAVE_NATPMP_H"]=" 1"
D["HAVE_BOOST"]=" /**/"
D["ENABLE_ZMQ"]=" 1"
D["HAVE_CONSENSUS_LIB"]=" 1"
D["ENABLE_WALLET"]=" 1"
D["USE_UPNP"]=" 1"
D["USE_NATPMP"]=" 1"
D["USE_QRCODE"]=" 1"
D["CLIENT_VERSION_MAJOR"]=" 23"
D["CLIENT_VERSION_MINOR"]=" 0"
D["CLIENT_VERSION_BUILD"]=" 0"
D["CLIENT_VERSION_IS_RELEASE"]=" false"
D["COPYRIGHT_YEAR"]=" 2025"
D["COPYRIGHT_HOLDERS"]=" \"The %s developers\""
D["COPYRIGHT_HOLDERS_SUBSTITUTION"]=" \"Dash Core\""
D["COPYRIGHT_HOLDERS_FINAL"]=" \"The Dash Core developers\""
  for (key in D) D_is_set[key] = 1
  FS = ""
}
/^[\t ]*#[\t ]*(define|undef)[\t ]+[_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ][_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789]*([\t (]|$)/ {
  line = $ 0
  split(line, arg, " ")
  if (arg[1] == "#") {
    defundef = arg[2]
    mac1 = arg[3]
  } else {
    defundef = substr(arg[1], 2)
    mac1 = arg[2]
  }
  split(mac1, mac2, "(") #)
  macro = mac2[1]
  prefix = substr(line, 1, index(line, defundef) - 1)
  if (D_is_set[macro]) {
    # Preserve the white space surrounding the "#".
    print prefix "define", macro P[macro] D[macro]
    next
  } else {
    # Replace #undef with comments.  This is necessary, for example,
    # in the case of _POSIX_SOURCE, which is predefined and required
    # on some systems where configure will not decide to define it.
    if (defundef == "undef") {
      print "/*", prefix defundef, macro, "*/"
      next
    }
  }
}
{ print }
