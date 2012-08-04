require 'mkmf'

have_library( "attr" )
$configure_args["--with-cflags"] = "-g"
create_makefile( "extended_attributes_ext" )
