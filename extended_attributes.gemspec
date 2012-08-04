Gem::Specification.new do |s|
  s.name        = 'extended_attributes'
  s.version     = '0.3'
  s.summary     = "Extended Attributes for Linux Filesystems"
  s.description =<<EOF
Allows access to the extended attributes that some linux filesystems provide,
e.g. ext2/3/4, xfs and btrfs. This code uses a C extension that calls libattr.
EOF
  s.authors     = ["Sven Riedel"]
  s.email       = 'sr@gimp.org'
  s.files       = %w[ ext/extconf.rb 
                      ext/extended_attributes.c
                      lib/extended_attributes.rb
                      spec/spec_helper.rb
                      spec/extended_attributes_spec.rb ]
  s.homepage    = 'https://github.com/sriedel/extended_attributes'
  s.platform = Gem::Platform::CURRENT
  s.require_path = 'lib'
  s.extensions << 'ext/extconf.rb'
  s.license = 'GPL-2'
  s.required_ruby_version = ">= 1.9.2"
  s.requirements << "libc >= 2.10"
  s.requirements << "libattr1 and header files"
end
