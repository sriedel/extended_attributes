#!/usr/bin/env ruby

require 'benchmark'
require './lib/extended_attributes'

COUNT = 100_000

Benchmark.bm do |x|
  x.report( "Reads" ) do
    COUNT.times { ExtendedAttributes.new( "tmp/attributes" ) }
  end

  x.report( "Writes" ) do
    ea = ExtendedAttributes.new( "tmp/attributes" )
    ea['attr1'] = "foo"
    ea.store

    COUNT.times do
      ea['attr1'] = %w[ a b c d e f g h i j k l m n o p q r s t ].sample(10).join
      ea.store
    end
  end
end
