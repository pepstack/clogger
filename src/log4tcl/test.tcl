load /usr/local/lib/liblog4tcl.so Log4tcl

source public.tcl
source test2.tcl

log4tcl::setlogger test

log4tcl::trace "(test) hello log4tcl"
log4tcl::debug "(test) hello log4tcl"
log4tcl::info "(test) hello log4tcl"
log4tcl::warn "(test) hello log4tcl"
log4tcl::error "(test) hello log4tcl"
log4tcl::fatal "(test) hello log4tcl"
