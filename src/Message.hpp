#pragma once

enum MessageType
{
	Insert=0, 
	Delete, 
	InsertMetric, 
	DeleteDevice, 
	Select_K1, 
	Select_K2, 
	Select_K3, 
	messageEND
};

// MessageType labels for reporting
const char * const messageTypeLabel[messageEND] = {
	"Insert", 
	"Delete", 
	"InsertMetric", 
	"DeleteDevice", 
	"Select_K1", 
	"Select_K2",
	"Select_K3"
};

class Message {

public:

    MessageType op = Insert;
    unsigned int ts = 0;
    unsigned int device_id =0;
    unsigned int table_id =0;
    Message(MessageType o,
	unsigned int t,
	unsigned int d,
	unsigned int tab) :op(o),ts(t),device_id(d),table_id(tab) {}
    Message() {}
    char* getMessageTypeLabel();
};

/* class to pass messages for statistics */
class StatMessage {
public:
    MessageType op = Insert;
    unsigned int time_us = 0;
    unsigned int cnt = 0;
    StatMessage(MessageType o, unsigned int t, unsigned int c) :op(o),time_us(t),cnt(c) {}
    StatMessage() {}
    char * getMessageTypeLabel();
};
