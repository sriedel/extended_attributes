= Linux Filesystem Extended Attributes for Ruby

This gem will allow read/write access to extended attributes for those linux
filesystems that support the interface (according to the manpage: ext2, ext3,
ext4, XFS, JFS, Reiser and Btrfs). 

== Usage
Currently, only the getter accessors are implemented:

    ea = ExtendedAttributes.new( path_to_file )
    foo = ea["my_attribute_name"]
    # or
    foo = ea.fetch( "my_attribute_name" )

== Requirements
* libc >= 2.10
* ruby 1.9.3 (not tested under 1.9.2 but I see no reason why it shouldn't work)

== TODO
* Make following symlinks optional
* Add setters
* Use attr_multi to reduce accesses to the filesystem layer
* Raise proper exceptions from the C extension
* Add filesystem capability check (?)
