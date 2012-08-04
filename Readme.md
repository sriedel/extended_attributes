# Linux Filesystem Extended Attributes for Ruby

This gem will allow read/write access to extended attributes for those linux
filesystems that support the interface (according to the manpage: ext2, ext3,
ext4, XFS, JFS, Reiser and Btrfs). 

## Usage
Fetching attributes:
```ruby
    ea = ExtendedAttributes.new( path_to_file )
    foo = ea["my_attribute_name"]
    # or
    foo = ea.fetch( "my_attribute_name" )
````

Setting attributes:

```ruby
    ea = ExtendedAttributes.new( path_to_file )
    ea["changed_or_new_attribute"] = "foo"
    ea["existing attribute name"] = nil # this will be removed
    ea.attributes_changed # returns a hash showing what entries changed
    ea.persisted? # => false
    ea.store
    ea.persisted? # => true
````

Resetting to attributes existing on filesystem:

```ruby
    ea = ExtendedAttributes.new( path_to_file )
    ea["changed_or_new_attribute"] = "foo"
    ea.persisted? # => false
    ea.refresh    # accesses the filesystem
    ea.persisted? # => true
````

Resetting to attributes as they were read on the filesystem:

```ruby
    ea = ExtendedAttributes.new( path_to_file )
    ea["changed_or_new_attribute"] = "foo"
    ea.persisted? # => false
    ea.reset      # uses cached values
    ea.persisted? # => true
````

##  Requirements
* libc >= 2.10
* ruby 1.9.3 (not tested under 1.9.2 but I see no reason why it shouldn't work)

##  TODO
* Make following symlinks optional
* Deal with root namespace (?)
* Raise proper exceptions from the C extension
* Add filesystem capability check (?)
