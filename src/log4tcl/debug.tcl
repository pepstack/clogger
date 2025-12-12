puts "debug.tcl: start load liblog4tcl.so"

proc get_script_dir {} {
    set script_path [info script]
    while {[file type $script_path] eq "link"} {
        set link_target [file readlink $script_path]
        if {[file pathtype $link_target] eq "relative"} {
            set script_path [file join [file dirname $script_path] $link_target]
        } else {
            set script_path $link_target
        }
        set script_path [file normalize $script_path]
    }
    return [file dirname $script_path]
}
set script_dir [get_script_dir]

if {[catch {load /home/root1/Workspace/clogger/build/linux-x86_64/libs/liblog4tcl.so Log4tcl} err]} {
    puts "Warning: Failed to load local liblog4tcl.so ($err), trying system path..."
    load /usr/local/lib/liblog4tcl.so Log4tcl
}

puts "debug.tcl: init loggers"

# /etc/clogger/clogger.cfg 必须包含:
# [clogger: test test_clogger radius libmysqltcl liblog4tcl aaa dbpub]
log4tcl::init test test_clogger radius libmysqltcl liblog4tcl aaa dbpub
log4tcl::setlogger liblog4tcl

puts "debug.tcl: logging messages..."

log4tcl::trace "(radius) hello log4tcl"
log4tcl::debug "(radius) hello log4tcl"
log4tcl::info "(radius) hello log4tcl"
log4tcl::warn "(radius) hello log4tcl"
log4tcl::error "(radius) hello log4tcl"
log4tcl::fatal "(radius) hello log4tcl"

puts "debug.tcl: stop ok."
