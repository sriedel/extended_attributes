#include <stdlib.h>
#include <attr/attributes.h>
#include <errno.h>
#include <string.h>

/* For debugging purposes */
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
/* debugging end */

#include "ruby.h"

typedef struct {
  attr_multiop_t *commands;
  int total_commands;
  int next_command;
} command_list_t;

static void read_attrs( const char *filepath );
static void add_attribute_value_to_hash( const char *filepath, VALUE hash, const char *attribute_name );
static void read_attributes_into_hash( const char *filepath, VALUE hash );
static void get_next_attribute_list_entries( const char *filepath, char *buffer, int buffersize, attrlist_cursor_t *cursor );
static void get_attribute_value_for_name( const char *filepath, const char *attribute_name, char *buffer, int *buffer_size );
static void store_attribute_changes_to_file( const char *filepath, VALUE change_hash );

static command_list_t *build_command_list( VALUE change_hash );
static int add_change_command_iterator( VALUE key, VALUE value, VALUE extra );
static int add_add_command_iterator( VALUE key, VALUE value, VALUE extra );

static void remove_removed_attributes( const char *filepath, VALUE change_hash );
static int remove_command_iterator( VALUE key, VALUE value, VALUE extra );

static VALUE ea_refresh_attributes( VALUE self );
static VALUE ea_store_attributes( VALUE self );

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

static VALUE ea_store_attributes( VALUE self ) 
{
VALUE path_value = rb_iv_get( self, "@path" );
VALUE path_string = StringValue( path_value );
char *path = strndup( RSTRING_PTR(path_string), RSTRING_LEN(path_string) );

VALUE changes_hash = rb_funcall( self, rb_intern( "attribute_changes" ), 0 );

  store_attribute_changes_to_file( path, changes_hash );

  free( path );
  return ea_refresh_attributes( self );
}

void Init_extended_attributes_ext( void ) {
  cExtendedAttributes = rb_define_class( "ExtendedAttributes", rb_cObject );
  rb_define_method( cExtendedAttributes, "refresh_attributes", ea_refresh_attributes, 0 );
  rb_define_method( cExtendedAttributes, "store", ea_store_attributes, 0 );
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

static void store_attribute_changes_to_file( const char *filepath, VALUE change_hash )
{
command_list_t *command_list = build_command_list( change_hash );
int retval = 0;
int i = 0;

  /* fprintf( stderr, "%d\n", getpid() ); */
  /* kill( getpid(), SIGSTOP ); */

  retval = attr_multi( filepath, command_list->commands, command_list->total_commands, 0 );
  if( retval == -1 ) {
    fprintf( stderr, "Error setting list: %s\n", strerror(errno) );
    exit(-1);
  }

  for( i = 0 ; i < command_list->total_commands; ++i ) {
    attr_multiop_t *command = &(command_list->commands[i]);
    int operation_error_code = command->am_error;

    if( operation_error_code != 0 ) {
      fprintf( stderr, "Error performing operation: %d\n", operation_error_code );
      fprintf( stderr, "Filepath: %s\n", filepath );
      fprintf( stderr, "Operation: %d\n", command->am_opcode );
      fprintf( stderr, "Attribute Name: %s\n", command->am_attrname );
      if( command->am_opcode != ATTR_OP_REMOVE ) {
        fprintf( stderr, "Attribute Value: %s\n", command->am_attrvalue );
      }
    }
  }

  for( i = 0 ; i < command_list->total_commands; ++i ) {
    attr_multiop_t *command = &(command_list->commands[i]);
    free( command->am_attrname );
    if( command->am_opcode != ATTR_OP_REMOVE ) {
      free( command->am_attrvalue );
    }
  }
  free( command_list->commands );
  free( command_list );

  remove_removed_attributes( filepath, change_hash );
}

static void remove_removed_attributes( const char *filepath, VALUE change_hash )
{
VALUE removed_values_hash = rb_hash_aref( change_hash, rb_str_new_cstr( "removed" ) );
int hash_entries = rb_hash_tbl(removed_values_hash)->num_entries;

  rb_hash_foreach( removed_values_hash, remove_command_iterator, (VALUE)filepath );
}

static command_list_t *build_command_list( VALUE change_hash ) 
{
command_list_t *commands = (command_list_t*)malloc( sizeof(command_list_t) );

VALUE changed_values_hash = rb_hash_aref( change_hash, rb_str_new_cstr( "changed" ) );
VALUE added_values_hash   = rb_hash_aref( change_hash, rb_str_new_cstr( "added" ) );
/* I'm sick and tired of looking for workarounds for all those static declared 
 * C functions.
 *
 * This isn't clean but what the fuck */
int changed_values_entries = rb_hash_tbl(changed_values_hash)->num_entries;
int added_values_entries = rb_hash_tbl(added_values_hash)->num_entries;
int entries = changed_values_entries + added_values_entries;


  memset( commands, 0, sizeof( command_list_t ) );
  
  commands->commands = (attr_multiop_t*)malloc( entries * sizeof( attr_multiop_t ) );
  commands->total_commands = entries;
  commands->next_command = 0;

  rb_hash_foreach( changed_values_hash, add_change_command_iterator, (VALUE)commands );
  rb_hash_foreach( added_values_hash,   add_add_command_iterator,    (VALUE)commands );

  return commands;
}

static int add_change_command_iterator( VALUE key, VALUE value, VALUE extra ) 
{
command_list_t *command_list = (command_list_t*)extra;

VALUE key_string_val = StringValue( key );
char *key_string = strndup( RSTRING_PTR(key_string_val), RSTRING_LEN(key_string_val) );
VALUE value_string_val = StringValue( value );
char *value_string = strndup( RSTRING_PTR( value_string_val ), RSTRING_LEN( value_string_val ) );

attr_multiop_t *command = &(command_list->commands[command_list->next_command]);

  command->am_opcode = ATTR_OP_SET;
  command->am_attrname = key_string;
  command->am_attrvalue = value_string;
  command->am_length = strlen( value_string );
  /* command.am_flags = ATTR_REPLACE; */ /* TODO: Think about error handling if this fails! */
  
  command_list->next_command++;
  return ST_CONTINUE;
}

static int remove_command_iterator( VALUE key, VALUE value, VALUE extra ) 
{
const char *filepath = (const char*)extra;

VALUE key_string_val = StringValue( key );
char *key_string = strndup( RSTRING_PTR(key_string_val), RSTRING_LEN(key_string_val) );
int retval = 0;

  retval = attr_remove( filepath, key_string, 0 );
  if( retval == -1 ) {
    fprintf( stderr, "Error removing attribute %s from %s: %d\n", key_string, filepath, errno );
    exit(-1);
    /* TODO real error handling here in line with what attr_multi would do*/
  }

  return ST_CONTINUE;
}

static int add_add_command_iterator( VALUE key, VALUE value, VALUE extra ) 
{
command_list_t *command_list = (command_list_t*)extra;

VALUE key_string_val = StringValue( key );
char *key_string = strndup( RSTRING_PTR(key_string_val), RSTRING_LEN(key_string_val) );
VALUE value_string_val = StringValue( value );
char *value_string = strndup( RSTRING_PTR( value_string_val ), RSTRING_LEN( value_string_val ) );

attr_multiop_t *command = &(command_list->commands[command_list->next_command]);

  command->am_opcode = ATTR_OP_SET;
  command->am_attrname = key_string;
  command->am_attrvalue = value_string;
  command->am_length = strlen( value_string );
  /* command.am_flags = ATTR_CREATE; */ /* TODO: Think about error handling if this fails! */
  
  command_list->next_command++;
  return ST_CONTINUE;
}


