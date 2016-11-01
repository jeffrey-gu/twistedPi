#!/usr/bin/python

#This implementation does not work with motive (yet)
#It just sends data over and over from a local file

import optparse, os, sys, socket, csv

from struct import *

from twisted.internet import reactor, defer, task
from twisted.internet.protocol import Protocol, Factory
from twisted.protocols.basic import LineOnlyReceiver

defaultTwistedServerPort = 8123
defaultMotivePort = 27015
# hostName = 'YuchiLi-PC'
hostName = 'jeffrey-K501UX'

msgLimit = 7

# win32 support
if sys.platform == 'win32':
    from twisted.internet import win32eventreactor
    win32eventreactor.install()

# def parse_args():
#     usage = """usage: %prog [options] file
#
# This is the Twisted Server
# Run it like this:
#
#   python twisted_server2-0.py <path-to-poetry-file>
# """
#
#     parser = optparse.OptionParser(usage)
#
#     help = "The port to listen on. Default to a random available port."
#     parser.add_option('--port', type='int', help=help)
#
#     help = "The interface to listen on. Default is localhost."
#     parser.add_option('--iface', help=help, default='localhost')
#
#     options, args = parser.parse_args()
#
#     if len(args) != 1:
#         parser.error('Provide exactly one CSV file.')
#
#     file = args[0]
#
#     if not os.path.exists(args[0]):
#         parser.error('No such file: %s' % file)
#
#     return options, file


class MotiveProtocol(LineOnlyReceiver):

    def __init__(self):
        self.delimiter = '\n'
        self.dataBuffer = []
        self.count = 0

    def connectionMade(self):
        print ('Connected to Motive server\n')

    def connectionLost(self, reason):
        print ('Motive connection failed because of: %s'), reason
        self.factory.disconnectedState()
        #TODO: call factory method to do ... wait?

    def lineReceived(self, data):
        try:
            self.dataBuffer.append(unpack("f", data))
            self.count += 1
        except:
            try:
                temp = unpack("f", data)
                if (temp == "x"):
                    self.count = 0
            except:
                print 'Could not read data stream'
                #TODO: data stream read error
                #TODO: log this, or just print to console?

        if(self.count==msgLimit):
            #TODO: fire deferred callbacks in output protocol factory
            self.factory.outputFactory.sendToAll(self.DataBuffer)
            self.dataBuffer = []
            self.count = 0


class MotiveProtocolFactory(Factory):

    protocol = MotiveProtocol

    def __init__(self, outputFactory):  # store reference to output factory
        self.outputFactory = outputFactory

    def disconnectedState(self):
        print ("Shutting down Twisted reactor\n")


class ClientHandler(LineOnlyReceiver):

    # def __init__(self):
    def connectionMade(self):
        print "Got new client!"

        dfd = defer.Deferred()
        dfd.addCallback(self.sendMsg)
        self.factory.clients[self] = dfd

    def connectionLost(self, reason):
        print "Lost a client! Reason: %s\n", reason
        self.factory.clients.pop(self)

        #TODO: pass on reason?

    def sendMsg(self, msg):
        print("sending msg:\n")
        msg = ', '.join(msg)    #make comma-delimited
        self.sendLine(msg)


class ClientHandlerFactory(Factory):
    protocol = ClientHandler

    def __init__(self, payload):
        self.clients = {}
        self.index = 0

    def recycle(self, client):
        for c in self.clients:
            if c == client:

                # print("recycling deferred for %s\n") %(client)
                dfd = defer.Deferred()
                dfd.addCallback(c.sendMsg)
                self.clients[c], dfd = dfd, None    #make it easier on garbage collector

    def sendToAll(self, data):

        self.index +=1
        # check if dict is empty
        if self.clients:
            for client, dfd in self.clients.iteritems():
                if dfd is not None:
                    dfd.callback(data)
                    self.recycle(client)


if __name__ == '__main__':
    # options, file = parse_args()
    # text = open(file).read()
    # payload = open(file)

    clientConnectionFactory = ClientHandlerFactory()
    motiveFactory = MotiveProtocolFactory(clientConnectionFactory)

    piPort = reactor.listenTCP(defaultTwistedServerPort, clientConnectionFactory, interface=socket.gethostbyname(hostName))
    motivePort = reactor.listenTCP(defaultMotivePort, motiveFactory, interface="127.0.0.1")

    # syntax: sendToAll() passes result of the call to LoopingCall
    # instead, you should omit the () to pass sendToAll as an argument
    # l = task.LoopingCall(clientConnectionFactory.sendToAll)
    # l.start(0.1)    # data send rate

    # print 'Serving %s on %s.' % (file, piPort.getHost())

    print 'Serving Motive messages from %s to clients at %s' % (motivePort, piPort)

    reactor.run()