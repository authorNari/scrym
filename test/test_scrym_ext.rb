if defined?(Scrym::Mutator.test)
  class TestScmExt < Test::Unit::TestCase
    def test_ext
      puts "--- C extension test start ---"
      Scrym::Mutator.test
      puts "--- C extension test end   ---"
    end
  end
end
