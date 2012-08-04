#include <stdlib.h>
#include <attr/attributes.h>
#include <attr/xattr.h> /* for ENOATTR */
#include <errno.h>
#include <string.h>

#include "ruby.h"

static inline char* extract_ruby_string( VALUE ruby_string );

static void read_attrs( const char *filepath );
static void add_attribute_value_to_hash( const char *filepath, VALUE hash, const char *attribute_name );
static void read_attributes_into_hash( const char *filepath, VALUE hash );
static void get_next_attribute_list_entries( const char *filepath, char *buffer, int buffersize, attrlist_cursor_t *cursor );
static void get_attribute_value_for_name( const char *filepath, const char *attribute_name, char *buffer, int *buffer_size );
static void store_attribute_changes_to_file( const char *filepath, VALUE change_hash );

static void set_changed_attributes( const char *filepath, VALUE change_hash );
static int set_command_iterator( VALUE key, VALUE value, VALUE extra );

static void remove_removed_attributes( const char *filepath, VALUE change_hash );
static int remove_command_iterator( VALUE key, VALUE value, VALUE extra );

static VALUE ea_refresh_attributes( VALUE self );
static VALUE ea_store_attributes( VALUE self );

void Init_extended_attributes_ext(void);

VALUE cExtendedAttributes;

static inline char *extract_ruby_string( VALUE ruby_string )
{
VALUE string = StringValue( ruby_string );
  return strndup( RSTRING_PTR(string), RSTRING_LEN(string) );
}

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
char *path = extract_ruby_string( path_value );

  read_attributes_into_hash( path, original_attributes_hash );
  rb_iv_set( self, "@original_attributes_hash", original_attributes_hash );

  free(path);

  return rb_funcall( self, rb_intern("reset"), 0 );
}

static VALUE ea_store_attributes( VALUE self ) 
{
VALUE path_value = rb_iv_get( self, "@path" );
char *path = extract_ruby_string( path_value );

VALUE changes_hash = rb_funcall( self, rb_intern( "attribute_changes" ), 0 );

  store_attribute_changes_to_file( path, changes_hash );

  free( path );
  return ea_refresh_attributes( self );
}

void Init_extended_attributes_ext( void ) {
  cExtendedAttributes = rb_define_class( "ExtendedAttributes", rb_cObject );
  rb_define_method( cExtendedAttributes, "refresh_attributes", ea_refresh_attributes, 0 );
  rb_define_method( cExtendedAttributes, "store", ea_store_attributes, 0 );
  rb_define_const( cExtendedAttributes, "MAX_VALUE_LENGTH", LONG2FIX(ATTR_MAX_VALUELEN) );
}

static void get_attribute_value_for_name( const char *filepath, const char *attribute_name, char *buffer, int *buffer_size )
{
int retval = 10;

  memset( buffer, 0, *buffer_size );

  retval = attr_get( filepath, attribute_name, buffer, buffer_size, 0 );
  if( retval == -1 ) {
    rb_sys_fail( filepath );
  }
}

static void add_attribute_value_to_hash( const char *filepath, VALUE hash, const char *attribute_name )
{
char *value_buffer = (char*)malloc( sizeof(char) * ATTR_MAX_VALUELEN );
int value_buffer_size = ATTR_MAX_VALUELEN;

  get_attribute_value_for_name( filepath, attribute_name, value_buffer, &value_buffer_size );

  if( value_buffer_size > 0 ) {
    rb_hash_aset( hash, rb_str_new_cstr( attribute_name ), 
                        rb_str_new_cstr( value_buffer ) );
  }

  free( value_buffer );
}

static void get_next_attribute_list_entries( const char *filepath, char *buffer, int buffersize, attrlist_cursor_t *cursor )
{
int retval = attr_list( filepath, (char*)buffer, buffersize, 0, cursor );

  if( retval == -1 ) {
    rb_sys_fail( filepath );
  }
}

static void read_attributes_into_hash( const char *filepath, VALUE hash )
{
int buffersize = ATTR_MAX_VALUELEN + sizeof(attrlist_t);
void *buffer = malloc( buffersize );
attrlist_cursor_t cursor;
attrlist_t *list = NULL;
int done = 0;
int entry_index = 0;

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

  free( buffer );
}

static void store_attribute_changes_to_file( const char *filepath, VALUE change_hash )
{
  set_changed_attributes( filepath, change_hash );
  remove_removed_attributes( filepath, change_hash );
}

static void remove_removed_attributes( const char *filepath, VALUE change_hash )
{
VALUE removed_values_hash = rb_hash_aref( change_hash, rb_str_new_cstr( "removed" ) );
int hash_entries = rb_hash_tbl(removed_values_hash)->num_entries;

  rb_hash_foreach( removed_values_hash, remove_command_iterator, (VALUE)filepath );
}

static void set_changed_attributes( const char *filepath, VALUE change_hash ) 
{
VALUE changed_values_hash = rb_hash_aref( change_hash, rb_str_new_cstr( "changed" ) );
VALUE added_values_hash   = rb_hash_aref( change_hash, rb_str_new_cstr( "added" ) );
  
  rb_hash_foreach( changed_values_hash, set_command_iterator, (VALUE)filepath );
  rb_hash_foreach( added_values_hash,   set_command_iterator, (VALUE)filepath );
}

static int set_command_iterator( VALUE key, VALUE value, VALUE extra ) 
{
const char *filepath = (const char*)extra;

char *key_string = extract_ruby_string( key );
char *value_string = extract_ruby_string( value );
int retval = 0;

  retval = attr_set( filepath, key_string, value_string, strlen(value_string), 0 );
  if( retval == -1 ) {
    rb_sys_fail( filepath );
  }
  
  free( key_string );
  free( value_string );

  return ST_CONTINUE;
}

static int remove_command_iterator( VALUE key, VALUE value, VALUE extra ) 
{
const char *filepath = (const char*)extra;

char *key_string = extract_ruby_string( key );
int retval = 0;

  retval = attr_remove( filepath, key_string, 0 );
  if( retval == -1 && errno != ENOATTR ) {
    rb_sys_fail( filepath );
  }
  /* TODO: Decide on what to do in the ENOATTR case */

  free( key_string );

  return ST_CONTINUE;
}
