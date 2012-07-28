#include <stdlib.h>
#include <attr/attributes.h>
#include <errno.h>
#include <string.h>

#include "ruby.h"

static void read_attrs( const char *filepath );
static void add_attribute_value_to_hash( const char *filepath, VALUE hash, const char *attribute_name );
static void read_attributes_into_hash( const char *filepath, VALUE hash );

static VALUE ea_init( VALUE self, VALUE given_path );
static VALUE ea_fetch( VALUE self, VALUE key );
static VALUE ea_set( VALUE self, VALUE key, VALUE value );
static VALUE ea_get_path( VALUE self );
static VALUE ea_get_attributes( VALUE self );
static VALUE ea_refresh_attributes( VALUE self );
void Init_extended_attributes(void);

VALUE cExtendedAttributes;

static VALUE ea_init( VALUE self, VALUE given_path )
{
VALUE attributes_hash = rb_hash_new();
VALUE path = rb_str_dup( given_path ); /* TODO: verify path is a string! */

  rb_iv_set( self, "@attributes_hash", attributes_hash );
  rb_iv_set( self, "@path", path );

  ea_refresh_attributes( self );

  return self;
}

static VALUE ea_fetch( VALUE self, VALUE key )
{
VALUE attributes_hash = rb_iv_get( self, "@attributes_hash" );
  return rb_hash_lookup( attributes_hash, key );
}

static VALUE ea_refresh_attributes( VALUE self ) 
{
VALUE attributes_hash = rb_iv_get( self, "@attributes_hash" );
VALUE path_value = rb_iv_get( self, "@path" );
VALUE path_string = StringValue( path_value );
char *path = strndup( RSTRING_PTR(path_string), RSTRING_LEN(path_string) );

  // NOTE: rb_hash_clear is not public for the C API because apparently
  //       nothing in the C API is public unless explicitly requested
  //       (q.v. http://www.ruby-forum.com/topic/4402535). 
  //      
  //       Until then, create a new hash instead and hope we don't leak memory
  //       because of this.
  // rb_hash_clear( attributes_hash );
  attributes_hash = rb_hash_new();
  rb_iv_set( self, "@attributes_hash", attributes_hash );

  read_attributes_into_hash( path, attributes_hash );

  free(path);
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

static VALUE ea_get_attributes( VALUE self ) 
{
  return rb_iv_get( self, "@attributes_hash" );
}

void Init_extended_attributes( void ) {
  cExtendedAttributes = rb_define_class( "ExtendedAttributes", rb_cObject );
  rb_define_method( cExtendedAttributes, "initialize", ea_init, 1 );
  rb_define_method( cExtendedAttributes, "fetch", ea_fetch, 1 );
  rb_define_method( cExtendedAttributes, "[]", ea_fetch, 1 );
  rb_define_method( cExtendedAttributes, "set", ea_set, 2 );
  rb_define_method( cExtendedAttributes, "[]=", ea_set, 2 );
  rb_define_method( cExtendedAttributes, "path", ea_get_path, 0 );
  rb_define_method( cExtendedAttributes, "attributes", ea_get_attributes, 0 );
  rb_define_method( cExtendedAttributes, "refresh_attributes", ea_refresh_attributes, 0 );
}

static void add_attribute_value_to_hash( const char *filepath, VALUE hash, const char *attribute_name )
{
/* FIXME: This function isn't thread safe! Serialize! */
int retval = 10;
static char *value_buffer = NULL;
int value_buffer_size = ATTR_MAX_VALUELEN;

  if( value_buffer == NULL ) {
    value_buffer = (char*)malloc( sizeof(char) * ATTR_MAX_VALUELEN );
  }

  memset( value_buffer, 0, value_buffer_size );

  retval = attr_get( filepath, attribute_name, value_buffer, &value_buffer_size, ATTR_DONTFOLLOW );
  if( retval == -1 ) {
    /* FIXME: Raise Errno Exception as soon as I figure out how */
    exit(-2);
  }

  rb_hash_aset( hash, rb_str_new_cstr( attribute_name ), 
                      rb_str_new_cstr( value_buffer ) );
}

static void read_attributes_into_hash( const char *filepath, VALUE hash )
{
int retval = 10;
int buffersize = ATTR_MAX_VALUELEN + sizeof(attrlist_t);
void *buffer = (void*)malloc( buffersize );
attrlist_cursor_t cursor;
attrlist_t *list = NULL;
int done = 0;
int entry_index = 0;

  memset( &cursor, 0, sizeof(attrlist_cursor_t) );
  while( !done ) {
    retval = attr_list( filepath, (char*)buffer, buffersize, ATTR_DONTFOLLOW, &cursor );

    if( retval == -1 ) {
      /* FIXME: Raise Errno Exception as soon as I figure out how */
      fprintf( stderr, "Error retrieving list: %s\n", strerror(errno) );
      exit(-1);
    }

    list = (attrlist_t*)buffer;

    for( entry_index = 0; entry_index < list->al_count; ++entry_index ) {
      attrlist_ent_t *list_entry = ATTR_ENTRY( (char*)buffer, entry_index );

      add_attribute_value_to_hash( filepath, hash, list_entry->a_name );
    }
    done = !list->al_more;
  }

  free( buffer );
}
