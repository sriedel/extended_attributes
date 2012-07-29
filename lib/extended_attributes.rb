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

    @attributes_hash[key_s] = value_s
  end
  alias_method :set, :[]=

  def attribute_changes
    changes = {}

    attr_ary = @attributes_hash.to_a
    original_ary = @original_attributes_hash.to_a

    attr_keys = @attributes_hash.keys
    original_keys = @original_attributes_hash.keys

    unchanged_keys = ( attr_ary & original_ary ).map(&:first)
    deleted_keys = original_keys - attr_keys
    changed_keys = attr_keys - unchanged_keys # includes added keys

    deleted_keys.each_with_object( changes ) { |key, changes| changes[key] = nil }
    changed_keys.each_with_object( changes ) { |key, changes| changes[key] = @attributes_hash[key] }

    changes
  end

  def reset
    @is_persisted = true
    @attributes_hash = @original_attributes_hash.dup
  end

  def persisted?
    @is_persisted
  end
end
