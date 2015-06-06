#include "../Message.hpp"
#define BOOST_TEST_MODULE MessageTypeLabelTest
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( message_type_label_test )
{
  int i;
  for (i=0; i < sizeof(MessageType); i++) {
    Message m;
    m.op=(MessageType)i;
    char *ret=m.getMessageTypeLabel();
    BOOST_CHECK(strcmp(messageTypeLabel[i],m.getMessageTypeLabel()) == 0);
  }
}

