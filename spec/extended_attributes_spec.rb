require 'spec_helper'
require 'fileutils'

describe ExtendedAttributes do
  ATTR_TEMPLATE_FILE = File.join( "tmp", "attr_template" )
  EMPTY_TEMPLATE_FILE = File.join( "tmp", "empty_template" )

  before( :all ) do
    testfile = File.join( "tmp", "testfile" )

    FileUtils.mkdir( "tmp" ) unless File.exists?( "tmp" )

    File.open( testfile, "w" ) do |fh|
      "I am a file to test extended attribute access!" 
    end

    File.open( ATTR_TEMPLATE_FILE, "w" ) do |fh|
      fh.puts <<-EOF
# file: #{testfile}
user.attr1="foo"
user.attr2="bar"

      EOF
    end

    File.open( EMPTY_TEMPLATE_FILE, "w" ) do |fh|
      fh.puts <<-EOF
# file: #{testfile}

      EOF
    end
  end

  describe "the initializer" do
    let( :filename ) { "tmp/testfile" }
    subject { ExtendedAttributes.new( filename ) }

    it "should exist" do
      subject.should be_an( ExtendedAttributes )
    end

    it "should have a filename attribute" do
      subject.path.should == filename
    end
  end

  describe "reading attributes" do
    it "should raise an exception if the file does not exist"

    context "if the file does not have any attributes" do
      before( :each ) do
        `setfattr --restore=#{EMPTY_TEMPLATE_FILE}`
      end
      it "should have an empty attributes hash"
    end

    context "if the file has attributes" do
      before( :each ) do
        `setfattr --restore=#{ATTR_TEMPLATE_FILE}`
      end
      it "should have them assigned to the attributes hash"
    end

  end
end
