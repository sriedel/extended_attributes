require 'spec_helper'
require 'fileutils'

describe ExtendedAttributes do
  ATTR_TEMPLATE_FILE = File.join( "tmp", "attr_template" )
  EMPTY_TEMPLATE_FILE = File.join( "tmp", "empty_template" )

  def fs_attrs_to_hash( filepath )
    getfattr_output = %x{getfattr -d #{filepath}}

    getfattr_output.lines.grep( /^user./ ).each_with_object( {} ) do |line, attrs|
      line.chomp!
      name, value = line.split( '=', 2 )
      name.sub!( /^user\./, '' ) # strip user/root namespace
      value.gsub!( /(?:^"|"$)/, '' ) # strip leading/trailing "
      attrs[name] = value
    end
  end

  def hash_to_fs_attrs( filepath, hash )
    File.open( ATTR_TEMPLATE_FILE, "w" ) do |fh|
      fh.puts "# file: #{filepath}"
      hash.each_pair do |attr_name, attr_value|
        fh.puts %W{user.#{attr_name}="#{attr_value}"}
      end
      fh.puts
    end

    `setfattr --restore=#{ATTR_TEMPLATE_FILE}`
  end

  before( :each ) do
    attrfile = File.join( "tmp", "attributes" )
    emptyfile = File.join( "tmp", "noattributes" )

    FileUtils.mkdir( "tmp" ) unless File.exists?( "tmp" )

    FileUtils.rm( attrfile ) if File.exists?( attrfile )
    FileUtils.rm( emptyfile ) if File.exists?( emptyfile )

    FileUtils.touch( attrfile )
    FileUtils.touch( emptyfile )

    hash_to_fs_attrs( attrfile, { "attr1" => "i stay the same",
                                  "attr2" => "i change",
                                  "attr3" => "i get removed" } )
  end

  let( :file_with_attributes ) { File.join( "tmp", "attributes" ) }
  let( :file_without_attributes ) { File.join( "tmp", "noattributes" ) }


  describe "the initializer" do
    subject { ExtendedAttributes.new( file_without_attributes ) }

    it "should exist" do
      subject.should be_an( ExtendedAttributes )
    end

    it "should have a filename attribute" do
      subject.path.should == file_without_attributes
    end

    it "should have an attributes attribute" do
      subject.attributes.should be_a( Hash )
      subject.attributes.size.should == 0
    end

    it "should have an original attributes attribute" do
      subject.original_attributes.should be_a( Hash )
      subject.original_attributes.size.should == 0
    end
  end

  describe "reading attributes" do
    context "if the file does not have any attributes" do
      subject { ExtendedAttributes.new( file_without_attributes ) }

      it "should have an empty attributes hash" do
        subject["attribute"].should == nil
      end
    end

    context "if the file has attributes" do
      subject { ExtendedAttributes.new( file_with_attributes ) }

      it "should have them assigned to the attributes hash" do
        subject["attr1"].should == "i stay the same"
        subject["attr2"].should == "i change"
      end
    end
  end

  describe "refresh attributes" do
    it "should raise an exception if the file does not exist" do
      pending "Figure out how to raise Errno::ENOENT from C"
      ea = ExtendedAttributes.new( "tmp/i_do_not_exist" )
      lambda { ea["foo"] }.should raise_error( Errno::ENOENT )
    end

    context "if the file has no attributes" do
      subject { ExtendedAttributes.new( file_without_attributes ) }

      it "should reset the attributes hash to an empty hash" do
        subject.refresh_attributes
        subject.attributes.should be_a( Hash )
        subject.attributes.size.should == 0

        subject.original_attributes.should eq( subject.attributes )
      end
    end

    context "if the file has attributes" do
      subject { ExtendedAttributes.new( file_with_attributes ) }
      it "should reset the attributes hash to reflect the set attributes" do
        subject.refresh_attributes
        subject.attributes.should be_a( Hash )
        subject.attributes.size.should == 3

        subject.original_attributes.should eq( subject.attributes )
      end
    end
  end

  describe "#[]" do
    subject { ExtendedAttributes.new( file_with_attributes ) }

    it "should return the attribute" do
      subject["attr1"].should == subject.attributes["attr1"]
    end

    it "should convert keys to strings" do
      subject[:attr1].should == subject.attributes["attr1"]
    end
  end

  describe "#[]=" do
    subject { ExtendedAttributes.new( file_with_attributes ) }

    it "should set the attribute in the attributes hash" do
      subject["attr1"] = "quux"
      subject.attributes["attr1"].should == "quux"
    end

    it "should not modify the original attributes hash" do
      expect { subject["attr1"] = "quux" }.to_not change { subject.original_attributes }
    end

    it "should delete the attribute if an empty string is assigned" do
      subject["attr3"] = ""
      subject.attributes.should_not have_key( "attr3" )
    end

    it "should delete the attribute if nil is assigned" do
      subject["attr3"] = nil
      subject.attributes.should_not have_key( "attr3" )
    end

    it "should convert keys and values to strings if possible" do
      subject[:attr4] = :foo
      subject["attr4"].should == "foo"

      subject["attr4"] = 234
      subject["attr4"].should == "234"
    end
  end

  describe "#reset" do
    subject { ExtendedAttributes.new( file_with_attributes ) }

    it "should reset the attributes hash to the original attributes" do
      subject["attr1"] = "quux"
      subject["attr3"] = "asdfasdf"
      subject.reset
      subject.attributes.should eq( subject.original_attributes ) 
    end
  end

  describe "#persisted?" do
    subject { ExtendedAttributes.new( file_with_attributes ) }

    it "should be true directly after reading" do
      subject.should be_persisted
    end

    it "should be false after modifying the attributes" do
      subject["attr1"] = "quux"
      subject.should_not be_persisted
    end

    it "should be true after reset" do
      subject["attr1"] = "quux"
      subject.reset
      subject.should be_persisted
    end

    it "should be true after writing"
    it "should be true if changing all attributes back to the original value"
  end

  describe "#attribute_changes" do
    subject { ExtendedAttributes.new( file_with_attributes ) }

    it "should return an empty hash if nothing changed" do
      subject.attribute_changes.should eq( "changed" => {},
                                           "removed" => {},
                                           "added"   => {} )
    end

    context "when something changed" do
      before( :each ) do
        subject["attr2"] = "some other value"
        subject["attr3"] = nil
        subject["attr4"] = "a new value"
      end

      it "should return a hash reflecting the changes" do
        subject.attribute_changes.should eq( "changed" => { "attr2" => "some other value" },
                                             "removed" => { "attr3" => nil },
                                             "added"   => { "attr4" => "a new value" } )
      end
    end
  end

  describe "#store" do
    subject { ExtendedAttributes.new( file_with_attributes ) }

    before( :each ) do
      subject["attr2"] = "some other value"
      subject["attr3"] = nil
      subject["attr4"] = "a new value"
    end

    it "should change the changed attributes" do
      subject.store

      present_attributes = fs_attrs_to_hash( file_with_attributes )
      present_attributes["attr2"].should == "some other value"
    end

    it "should delete the deleted attributes" do
      subject.store

      present_attributes = fs_attrs_to_hash( file_with_attributes )
      present_attributes["attr3"].should be_nil
    end

    it "should add the added attributes" do
      subject.store

      present_attributes = fs_attrs_to_hash( file_with_attributes )
      present_attributes["attr4"].should == "a new value"
    end

    it "should re-read the attributes present in the file"
  end
end
