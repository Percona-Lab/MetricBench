// include files here

#define BOOST_TEST_MODULE NameOfTest
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( name_of_test )
{
  bool val=true;

  BOOST_CHECK(val);
}

