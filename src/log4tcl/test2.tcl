load /usr/local/lib/liblog4tcl.so Log4tcl

source public.tcl

log4tcl::setlogger test_clogger


log4tcl::trace "(test2) hello log4tcl"
log4tcl::debug "(test2) hello log4tcl"
log4tcl::info "(test2) hello log4tcl"
log4tcl::warn "(test2) hello log4tcl"
log4tcl::error "(test2) hello log4tcl"
log4tcl::fatal "(test2) hello log4tcl"
