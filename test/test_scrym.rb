class TestScm < Test::Unit::TestCase
  include Scrym

  def test_mark
    str = "mark"
    Mutator.mark(str)
    assert_raise ArgumentError do
      Mutator.mark(1)
    end
    Mutator.collect
  end
end
