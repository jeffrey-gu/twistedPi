# uses netstring receiver

# !/usr/bin/env python

# Copyright (c) Twisted Matrix Laboratories.
# See LICENSE for details.

from twisted.python import log, usage
from twisted.internet import defer, task
from twisted.internet.protocol import Protocol, ClientFactory, ServerFactory, Factory
from twisted.protocols import basic
import sys, time, struct, bitarray
import ipdb

prevTime = 0.0
streamError = []

if sys.platform == 'win32':
    from twisted.internet import win32eventreactor

    win32eventreactor.install()


class OutputProtocol(object, basic.LineOnlyReceiver):
    def __init__(self):
        self.delimiter = '\n'
        self.count = 0

    def connectionMade(self):
        print ("Got a RPi connection!")
        self.transport.write("hello :-)!\r\n")
        self.factory.client_list.append(self)

    def connectionLost(self, reason):
        print "Lost client %s! Reason: %s\n" % (self, reason)
        self.factory.client_list.remove(self)

    def sendMsg(self, data):
        '''#header 0x7E is new packet
        header = '\x7E'
        self.transport.write('header')
        #0x01 is sensor data
        packetType='\x01'
        self.transport.write(packetType)
        #seven values at 4 bytes each = 28 bytes in total
        packetLength = 28
        self.transport.write(packetLength)
        #for some reason, data is a list of tuples so we need to break it up into a string
        '''
        # for el in data:
        #     self.transport.write(pack('f',el[0])+"\n")

        # self.transport.write("\r\n")
        self.transport.write(str(data) + "\r\n")


class OutputProtocolFactory(ServerFactory):
    protocol = OutputProtocol

    def __init__(self):
        self.client_list = []

    def sendToAll(self, data):
        # check if dict is empty
        if self.client_list:
            for client in self.client_list:
                client.sendMsg(data)


class InputProtocol(basic.NetstringReceiver):
    def __init__(self, outputHandle):
        self.MAX_LENGTH = 33;
        self.dataBuff = []
        self.outputHandle = outputHandle
        print ("Input Protocol built")

    def connectionMade(self):
        print ("Connected to Motive")

    def stringReceived(self, string):
        global prevTime

        currTime = time.clock()
        bytesReceived = sys.getsizeof(string)
        print 'Size of data: %d\n' % (bytesReceived)  # prints out 66 bytes... padding?

        # convert to binary format - this gets condensed down to 33 bytes
        binData = ' '.join('{0:08b}'.format(ord(x), 'b') for x in string)
        outputFile_bin.write(binData)
        outputFile_bin.write('\tReceived %d bytes\tBefore: %f\tNow: %f\n\n' % (bytesReceived, prevTime, currTime))

        try:
            self.dataBuff.append(struct.unpack("!HBBBfffffff", string))
            outputFile.write(str(self.dataBuff))
        except:
            print 'Failed to unpack\n'

        outputFile.write('\tBefore: %f\tNow: %f\n\n' % (prevTime, currTime))
        prevTime = currTime

        # ipdb.set_trace()
        self.outputHandle.sendToAll(self.dataBuff)
        self.dataBuff = []


class InputProtocolFactory(ClientFactory):
    def __init__(self, outputHandle):
        self.outputHandle = outputHandle

    def startedConnecting(self, connector):
        print 'Started to connect.'

    def buildProtocol(self, addr):
        print 'Connected.'
        return InputProtocol(self.outputHandle)

    def clientConnectionLost(self, connector, reason):
        print 'Lost MotiveClient connection.  Reason:', reason
        reactor.stop()
        sys.exit()

    def clientConnectionFailed(self, connector, reason):
        print 'Connection failed. Reason:', reason
        reactor.stop()
        sys.exit()


if __name__ == '__main__':
    from twisted.internet import reactor

    outputFile_bin = open(r'Data Files/motive_results_bin.txt', 'w+')
    outputFile = open(r'Data Files/motive_results.txt', 'w+')

    out = OutputProtocolFactory()
    reactor.listenTCP(53335, out, interface="192.168.95.109")
    reactor.connectTCP("192.168.95.109", 27015, InputProtocolFactory(out))

    reactor.run()

