// include files here

#include <string>

#include "../Uri.hpp"

#define BOOST_TEST_MODULE UriTest
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( uri_test )
{
  Uri t = Uri::Parse("tcp://localhost:27017");

  BOOST_CHECK(t.Protocol.compare("tcp") == 0);
  BOOST_CHECK(t.Host.compare("localhost") == 0);
  BOOST_CHECK(t.Port.compare("27017") == 0);

  Uri s = Uri::Parse("unix:///tmp/mongodb-27017.sock");

  BOOST_CHECK(s.Protocol.compare("unix") == 0);
  BOOST_CHECK(s.Path.compare("/tmp/mongodb-27017.sock") == 0);

}

