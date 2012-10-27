require 'mkmf'

if with_config("test")
  $defs << "-DSCRYM_TEST"
  $CFLAGS << " -O0"
end

if have_macro("FL_RESERVED2")
  $defs << "-DHAVE_FL_RESERVED2"
end

dir_config("scrym_ext")
create_header
create_makefile("scrym_ext")
