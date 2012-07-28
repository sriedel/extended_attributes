#include <stdlib.h>
#include <attr/attributes.h>
#include <errno.h>
#include <string.h>

#include "ruby.h"

typedef struct { 
  char *name; 
  char *value; 
} key_value_t;

typedef struct _key_value_list_t {
  key_value_t *entry;
  struct _key_value_list_t *next;
} key_value_list_t;

static key_value_list_t *read_attrs( const char *filepath );
static key_value_t *list_attribute( const char *filepath, const char *attribute_name );
static key_value_list_t *add_key_value_to_list( key_value_list_t *list, key_value_t *entry );

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
key_value_list_t *attribute_list = read_attrs( path );
key_value_list_t *iter = NULL;
key_value_list_t *iter_next = NULL;

  // NOTE: rb_hash_clear is not public for the C API because apparently
  //       nothing in the C API is public unless explicitly requested
  //       (q.v. http://www.ruby-forum.com/topic/4402535). 
  //      
  //       Until then, create a new hash instead and hope we don't leak memory
  //       because of this.
  // rb_hash_clear( attributes_hash );
  attributes_hash = rb_hash_new();
  rb_iv_set( self, "@attributes_hash", attributes_hash );

  for( iter = attribute_list; iter != NULL; iter = iter_next ) {
    key_value_t *entry = iter->entry;
    iter_next = iter->next;

    rb_hash_aset( attributes_hash, rb_str_new_cstr(entry->name), rb_str_new_cstr(entry->value) );
    free( entry->name );
    free( entry->value );
    free( entry );

    free( iter );
  }

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

static key_value_t *list_attribute( const char *filepath, const char *attribute_name )
{
/* FIXME: This function isn't thread safe! Serialize! */
int retval = 10;
static char *value_buffer = NULL;
int value_buffer_size = ATTR_MAX_VALUELEN;
key_value_t *key_value = NULL;

  if( value_buffer == NULL ) {
    value_buffer = (char*)malloc( sizeof(char) * ATTR_MAX_VALUELEN );
  }
  memset( value_buffer, 0, value_buffer_size );

  retval = attr_get( filepath, attribute_name, value_buffer, &value_buffer_size, ATTR_DONTFOLLOW );
  if( retval == -1 ) {
    /* FIXME: Raise Errno Exception as soon as I figure out how */
    exit(-2);
  }

  key_value = (key_value_t*)malloc( sizeof( key_value_t ) );
  key_value->name = strdup( attribute_name );
  key_value->value = strdup( value_buffer );

  return key_value;
}

static key_value_list_t *read_attrs( const char *filepath )
{
int retval = 10;
int buffersize = ATTR_MAX_VALUELEN + sizeof(attrlist_t);
void *buffer = (void*)malloc( buffersize );
attrlist_cursor_t cursor;
attrlist_t *list = NULL;
int done = 0;
int entry_index = 0;
key_value_t *key_value = NULL;
key_value_list_t *key_value_list = NULL;

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

      key_value = list_attribute( filepath, list_entry->a_name );
      key_value_list = add_key_value_to_list( key_value_list, key_value );
    }
    done = !list->al_more;
  }

  free( buffer );
  return key_value_list;
}

static key_value_list_t *add_key_value_to_list( key_value_list_t *list, key_value_t *entry ) 
{
key_value_list_t *new_list_head = (key_value_list_t*)malloc( sizeof( key_value_list_t ) );
  new_list_head->next = list;
  new_list_head->entry = entry;

  return new_list_head;
}
