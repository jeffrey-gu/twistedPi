#!/usr/bin/env python

# Copyright (c) Twisted Matrix Laboratories.
# See LICENSE for details.

from twisted.python import log, usage
from twisted.internet import defer, task
from twisted.internet.protocol import Protocol, ClientFactory, Factory
from twisted.protocols import basic
from struct import *
import sys
import pdb

if sys.platform == 'win32':
    from twisted.internet import win32eventreactor
    win32eventreactor.install()

class InputLogger:
    def writeLog(self, data):
      if(type(data) is list):
        print(" ".join(["%f"%el for el in data]))
      else:
        log.msg(data)

# class ArdLogger:
#     def writeLog(self, data):
#       if(type(data) is list):
#         print("".join(["%s"%el for el in data]))
#       else:
#         log.msg(data)

class InputProtocol(object,basic.LineOnlyReceiver, InputLogger):
    
    def __init__(self,outputHandle):
        self.delimiter = '\n'
        self.count = 0
        self.dataBuff = []
        self.outputHandle=outputHandle

    def lineReceived(self,data):
        try:
            #pdb.set_trace()
            self.dataBuff.append(unpack("f",data))
            self.count=self.count+1
        except:
            try:
                temp = unpack("f",data)
                if(temp=="x"):
                    self.count=0
            except:
                super(InputProtocol,self).writeLog("Error reading data stream")
        if(self.count==7):
            #super(InputProtocol,self).writeLog(self.dataBuff)
            print (self.dataBuff)
            if self.outputHandle.client_list is not None:
                self.outputHandle.sendToAll(self.dataBuff)
            self.dataBuff=[]
            self.count=0

class OutputProtocol(object,basic.LineOnlyReceiver, InputLogger):
        
    def __init__(self):
        self.delimiter = '\n'
        self.count = 0
        self.dataBuff = []

    def connectionMade(self):
        self.transport.write("hello!\r\n")

    # def lineReceived(self,data):
    #     super(InputProtocol,self).writeLog(self.dataBuff)
    #     self.dataBuff=[]
    #     self.count=0

    def sendMsg(self,data):
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
        for el in data:
            self.transport.write(pack('f',el[0])+"\n")

class OutputProtocolFactory(Factory):
    
    protocol = OutputProtocol

    def __init__(self):
        self.client_list = []
    
    def recycle(self, client):
        for c in self.client_list:
            if c == client:
                # print("recycling deferred for %s\n") %(client)
                dfd = defer.Deferred()
                dfd.addCallback(c.sendMsg)
                self.client_list[c], dfd = dfd, None    #make it easier on garbage collector

    def sendToAll(self, data):
        # check if dict is empty
        if self.client_list:
            for client, dfd in self.client_list.iteritems():
                if dfd is not None:
                    dfd.callback(data)
                    self.recycle(client)

class InputProtocolFactory(ClientFactory):
    def __init__(self,outputHandle):
        self.client_list = []
        self.outputHandle=outputHandle

    def startedConnecting(self, connector):
        print 'Started to connect.'

    def buildProtocol(self, addr):
        print 'Connected.'
        return InputProtocol(self.outputHandle)

    def clientConnectionLost(self, connector, reason):
        print 'Lost connection.  Reason:', reason

    def clientConnectionFailed(self, connector, reason):
        print 'Connection failed. Reason:', reason
        if not self.client_list:
            reactor.stop()

if __name__ == '__main__':
    from twisted.internet import reactor
    # from twisted.internet.serialport import SerialPort

    
    logFile = sys.stdout
    log.startLogging(logFile)
    portOutput = 'COM3'
    baudrateOut = 115200

    # out = OutputProtocol()
    out = OutputProtocolFactory()

    reactor.listenTCP(53335, out, interface="192.168.95.109")
    reactor.connectTCP("localhost",27015, InputProtocolFactory(out))

    # sOut = SerialPort(out,portOutput, reactor, baudrate=baudrateOut)

    #sArduino = SerialPort(ArdProtocol(),portArd, reactor, baudrate=baudrateArd)
    #sHost = SerialPort(InputProtocol(),portHost, reactor, baudrate=baudrateHost)

    reactor.run()

