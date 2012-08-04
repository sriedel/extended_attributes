require_relative '../ext/extended_attributes_ext'

class ExtendedAttributes
  attr_reader :path

=begin rdoc
  Instantiates an EA object and reads the file system attributes.
  Can throw exceptions (q.v. attr_list(3) and attr_get(3)) due to filesystem
  access.

  +path+ is a filesystem object, i.e. a file or directory. Symlinks are 
  currently automatically followed. At the moment it is not possible to
  access or modify the attributes attached to a symlink.
=end
  def initialize( path )
    @attributes_hash = {}
    @original_attributes_hash = {}
    @path = path
    @is_persisted = true

    refresh_attributes
  end

=begin rdoc
  Returns the current attributes hash. Note that the contents of this hash
  reflects what the user may have modified and has not yet been flushed to
  disk. Thus, the 'real' attributes attached to the filesystem object may
  be different.
=end
  def attributes 
    @attributes_hash
  end

=begin rdoc
  Returns a hash of the last known attributes of the filesystem object
  as they were on the filesystem. These may be out of date if something
  has modified the attributes on disk since the last refresh.
=end
  def original_attributes
    @original_attributes_hash
  end

=begin rdoc
  Returns the value of the attribute +key+ in the current attributes hash. 
  This may be a user-modified value that has not been flushed to disk yet.
=end
  def []( key ) 
    @attributes_hash[key.to_s]
  end
  alias_method :fetch, :[]

=begin rdoc
  Sets the attribute +key+ to +value+ to the current attributes. +key+ and 
  +value+ are converted to strings before saving.

  Assigining an empty string as +value+ will mark this attribute for deletion.

  Note, changes to this hash will not be flushed automatically to disk; you 
  will need to explicitly call #store.

  Will raise an ArgumentError exception if the values length is too large
  according to libattr. Note that this check does not neccessarily catch 
  all errors. E.g. for ext2/3/4 the maximum value length is one block size 
  (max 4kb), for JFS this is 64k for values and 255 bytes for names
  and XFS has no defined limits. Btrfs is known.
=end
  def []=( key, value )
    @is_persisted = false

    key_s = key.to_s
    value_s = value.to_s
    return @attributes_hash.delete( key_s ) if value_s == ""

    raise ArgumentError if value_s.length > MAX_VALUE_LENGTH

    @attributes_hash[key_s] = value_s
  end
  alias_method :set, :[]=

=begin rdoc
  Computes and returns a hash containing the differences between the 
  current attributes hash and the last known attributes as they are (were) on
  the filesystem. This hash is used to determine which operations to perform
  when flushing the changes to the filesystem.

  The returned hash has the following structure:
  { "changed" => { "attribute name 1" => "changed attribute value 1",
                    ... },
    "removed" => { "attribute name n" => nil,
                   ... },
    "added"   => { "attribute name x" => "attribute value x",
                   ... }
  }
=end
  def attribute_changes
    changes = { "changed" => {},
                "removed" => {},
                "added"   => {} }

    #TODO: Clean attr_keys and original_keys of unchanged entries earlier
    attr_keys = @attributes_hash.keys
    original_keys = @original_attributes_hash.keys

    deleted_keys = original_keys - attr_keys
    added_keys   = attr_keys - original_keys
    changed_keys = attr_keys - unchanged_keys - added_keys

    deleted_keys.each_with_object( changes ) { |key, changes| changes["removed"][key] = nil }
    changed_keys.each_with_object( changes ) { |key, changes| changes["changed"][key] = @attributes_hash[key] }
    added_keys.each_with_object( changes )   { |key, changes| changes["added"][key]   = @attributes_hash[key] }
    
    changes
  end

=begin rdoc
  Resets the current attribute hash to the last known filesystem state.
  Does not re-read the current attribute state from disk.
=end
  def reset
    @is_persisted = true
    @attributes_hash = @original_attributes_hash.dup
  end

=begin rdoc
  Returns a boolean indicating if the current attribute state reflects what
  was last read from disk.
=end
  def persisted?
    @is_persisted
  end

  private
  def unchanged_keys
    attr_ary = @attributes_hash.to_a
    original_ary = @original_attributes_hash.to_a

    ( attr_ary & original_ary ).map(&:first)
  end
end
