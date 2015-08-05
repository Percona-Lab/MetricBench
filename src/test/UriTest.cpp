// include files here

#include <string>

#include "../Uri.hpp"

#define BOOST_TEST_MODULE UriTest
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( uri_test )
{
  Uri t = Uri::Parse(L"tcp://localhost:27017");

  BOOST_CHECK(t.Protocol.compare(L"tcp") == 0);
  BOOST_CHECK(t.Host.compare(L"localhost") == 0);
  BOOST_CHECK(t.Port.compare(L"27017") == 0);

  Uri s = Uri::Parse(L"unix:///tmp/mongodb-27017.sock");

  BOOST_CHECK(s.Protocol.compare(L"unix") == 0);
  BOOST_CHECK(s.Path.compare(L"/tmp/mongodb-27017.sock") == 0);

}

