#include <stdlib.h>
#include <attr/attributes.h>
#include <errno.h>
#include <string.h>

#include "ruby.h"

static void read_attrs( const char *filepath );
static void add_attribute_value_to_hash( const char *filepath, VALUE hash, const char *attribute_name );
static void read_attributes_into_hash( const char *filepath, VALUE hash );
static void get_next_attribute_list_entries( const char *filepath, char *buffer, int buffersize, attrlist_cursor_t *cursor );
static void get_attribute_value_for_name( const char *filepath, const char *attribute_name, char *buffer, int *buffer_size );

static VALUE ea_refresh_attributes( VALUE self );

void Init_extended_attributes_ext(void);

VALUE cExtendedAttributes;

static VALUE ea_refresh_attributes( VALUE self ) 
{
// NOTE: rb_hash_clear is not public for the C API because apparently
//       nothing in the C API is public unless explicitly requested
//       (q.v. http://www.ruby-forum.com/topic/4402535). 
//      
//       Until then, create a new hash instead and hope we don't leak memory
//       because of this.
// rb_hash_clear( attributes_hash );
//
VALUE original_attributes_hash = rb_hash_new();

VALUE path_value = rb_iv_get( self, "@path" );
VALUE path_string = StringValue( path_value );
char *path = strndup( RSTRING_PTR(path_string), RSTRING_LEN(path_string) );

  read_attributes_into_hash( path, original_attributes_hash );
  rb_iv_set( self, "@original_attributes_hash", original_attributes_hash );

  free(path);

  return rb_funcall( self, rb_intern("reset"), 0 );
}

void Init_extended_attributes_ext( void ) {
  cExtendedAttributes = rb_define_class( "ExtendedAttributes", rb_cObject );
  rb_define_method( cExtendedAttributes, "refresh_attributes", ea_refresh_attributes, 0 );
}

static void get_attribute_value_for_name( const char *filepath, const char *attribute_name, char *buffer, int *buffer_size )
{
int retval = 10;

  memset( buffer, 0, *buffer_size );

  retval = attr_get( filepath, attribute_name, buffer, buffer_size, ATTR_DONTFOLLOW );
  if( retval == -1 ) {
    /* FIXME: Raise Errno Exception as soon as I figure out how */
    exit(-2);
  }
}

static void add_attribute_value_to_hash( const char *filepath, VALUE hash, const char *attribute_name )
{
/* FIXME: This function isn't thread safe! Serialize! */
static char *value_buffer = NULL;
int value_buffer_size = ATTR_MAX_VALUELEN;

  if( value_buffer == NULL ) {
    value_buffer = (char*)malloc( sizeof(char) * ATTR_MAX_VALUELEN );
  }

  get_attribute_value_for_name( filepath, attribute_name, value_buffer, &value_buffer_size );

  if( value_buffer_size > 0 ) {
    rb_hash_aset( hash, rb_str_new_cstr( attribute_name ), 
                        rb_str_new_cstr( value_buffer ) );
  }
}

static void get_next_attribute_list_entries( const char *filepath, char *buffer, int buffersize, attrlist_cursor_t *cursor )
{
int retval = attr_list( filepath, (char*)buffer, buffersize, ATTR_DONTFOLLOW, cursor );

  if( retval == -1 ) {
    /* FIXME: Raise Errno Exception as soon as I figure out how */
    fprintf( stderr, "Error retrieving list: %s\n", strerror(errno) );
    exit(-1);
  }
}

static void read_attributes_into_hash( const char *filepath, VALUE hash )
{
int buffersize = ATTR_MAX_VALUELEN + sizeof(attrlist_t);
static void *buffer = NULL;
attrlist_cursor_t cursor;
attrlist_t *list = NULL;
int done = 0;
int entry_index = 0;

  if( buffer == NULL ) {
    buffer = (void*)malloc( buffersize );
  }

  memset( &cursor, 0, sizeof(attrlist_cursor_t) );
  while( !done ) {
    get_next_attribute_list_entries( filepath, (char*)buffer, buffersize, &cursor );
    list = (attrlist_t*)buffer;

    for( entry_index = 0; entry_index < list->al_count; ++entry_index ) {
      attrlist_ent_t *list_entry = ATTR_ENTRY( (char*)buffer, entry_index );
      add_attribute_value_to_hash( filepath, hash, list_entry->a_name );
    }
    done = !list->al_more;
  }
}
