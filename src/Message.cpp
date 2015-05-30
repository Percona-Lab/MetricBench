#include <string>
#include "Message.hpp"

char* Message::getMessageTypeLabel() {
    char *ret;
    ret=(char *) messageTypeLabel[this->op];
    return ret;
}
