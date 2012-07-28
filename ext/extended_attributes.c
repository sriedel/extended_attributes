#include "ruby.h"

VALUE cExtendedAttributes;

static VALUE ea_init( VALUE self, VALUE given_path )
{
VALUE attributes_hash = rb_hash_new();
VALUE path = rb_str_dup( given_path ); /* TODO: verify path is a string! */

  rb_iv_set( self, "@attributes_hash", attributes_hash );
  rb_iv_set( self, "@path", path );

  return self;
}

static VALUE ea_fetch( VALUE self, VALUE key )
{
VALUE attributes_hash = rb_iv_get( self, "@attributes_hash" );
  return rb_hash_lookup( attributes_hash, key );
}

static VALUE ea_set( VALUE self, VALUE key, VALUE value )
{
/* FIXME: to_s key and value! */
VALUE attributes_hash = rb_iv_get( self, "@attributes_hash" );
  return rb_hash_aset( attributes_hash, key, value );
}

static VALUE ea_get_path( VALUE self ) 
{
  return rb_iv_get( self, "@path" );
}

void Init_extended_attributes() {
  cExtendedAttributes = rb_define_class( "ExtendedAttributes", rb_cObject );
  rb_define_method( cExtendedAttributes, "initialize", ea_init, 1 );
  rb_define_method( cExtendedAttributes, "fetch", ea_fetch, 1 );
  rb_define_method( cExtendedAttributes, "[]", ea_fetch, 1 );
  rb_define_method( cExtendedAttributes, "set", ea_set, 2 );
  rb_define_method( cExtendedAttributes, "[]=", ea_set, 2 );
  rb_define_method( cExtendedAttributes, "path", ea_get_path, 0 );
}
