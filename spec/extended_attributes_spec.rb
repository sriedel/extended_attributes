require 'spec_helper'
require 'fileutils'

describe ExtendedAttributes do
  ATTR_TEMPLATE_FILE = File.join( "tmp", "attr_template" )
  EMPTY_TEMPLATE_FILE = File.join( "tmp", "empty_template" )

  before( :all ) do
    attrfile = File.join( "tmp", "attributes" )
    emptyfile = File.join( "tmp", "noattributes" )

    FileUtils.mkdir( "tmp" ) unless File.exists?( "tmp" )

    FileUtils.touch( attrfile )
    FileUtils.touch( emptyfile )

    File.open( ATTR_TEMPLATE_FILE, "w" ) do |fh|
      fh.puts <<-EOF
# file: #{attrfile}
user.attr1="i stay the same"
user.attr2="i change"
user.attr3="i get removed"

      EOF
    end
    `setfattr --restore=#{ATTR_TEMPLATE_FILE}`
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

    it "should convert keys to strings"
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

    it "should delete the attribute if nil is assigned"

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
      subject.attribute_changes.should eq( {} )
    end

    context "when something changed" do
      before( :each ) do
        pending
        subject["attr2"] = "some other value"
        subject["attr3"] = nil
        subject["attr4"] = "a new value"
      end

      it "should return a hash reflecting the changes" do
        subject.attribute_changes.should eq( { "attr2" => "some other value",
                                               "attr3" => nil,
                                               "attr4" => "a new value" } )
      end
    end
    
  end
end
