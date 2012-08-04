require_relative '../ext/extended_attributes_ext'

class ExtendedAttributes
  attr_reader :path

  def initialize( path )
    @attributes_hash = {}
    @original_attributes_hash = {}
    @path = path
    @is_persisted = true

    refresh_attributes
  end

  def attributes 
    @attributes_hash
  end

  def original_attributes
    @original_attributes_hash
  end

  def []( key ) 
    @attributes_hash[key.to_s]
  end
  alias_method :fetch, :[]

  def []=( key, value )
    @is_persisted = false

    key_s = key.to_s
    value_s = value.to_s
    return @attributes_hash.delete( key_s ) if value_s == ""

    raise ArgumentError if value_s.length > MAX_VALUE_LENGTH

    @attributes_hash[key_s] = value_s
  end
  alias_method :set, :[]=

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

  def reset
    @is_persisted = true
    @attributes_hash = @original_attributes_hash.dup
  end

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
