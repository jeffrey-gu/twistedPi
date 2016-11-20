#!/usr/bin/python

#This implementation does not work with motive (yet)
#It just sends data over and over from a local file

from twisted.internet import defer, task
from twisted.python import log, usage
from twisted.internet.protocol import Protocol, ClientFactory, Factory
from twisted.protocols.basic import LineOnlyReceiver
from struct import *
import sys, pdb


# win32 support
if sys.platform == 'win32':
    from twisted.internet import win32eventreactor
    win32eventreactor.install()

# defaultTwistedServerPort = 5009
defaultTwistedServerPort = 53335
defaultMotivePort = 27015
hostName = 'YuchiLi-PC'
# hostName = 'jeffrey-K501UX'

msgLimit = 7

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
        # data = unicode(data, errors='ignore')
        
        try:
            # print (data)
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
            self.factory.outputFactory.sendToAll(self.dataBuffer)
            self.dataBuffer = []
            self.count = 0


class MotiveProtocolFactory(ClientFactory):

    protocol = MotiveProtocol

    def __init__(self, outputFactory):  # store reference to output factory
        self.outputFactory = outputFactory

    def startedConnecting(self, connector):
        print ("Started to connect.\n")

    def clientConnectionLost(self):
        print ("Lost connection to Motive. Standby...")

    def disconnectedState(self):
        print ("Shutting down Twisted reactor\n")
        reactor.stop()


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

    def __init__(self):
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

    from twisted.internet import reactor

    clientConnectionFactory = ClientHandlerFactory()
    # motiveFactory = MotiveProtocolFactory(clientConnectionFactory)

    # piPort = reactor.listenTCP(defaultTwistedServerPort, clientConnectionFactory, interface=socket.gethostbyname(hostName))
    # piPort = reactor.listenTCP(defaultTwistedServerPort, clientConnectionFactory, interface="128.252.19.161")
    piPort = reactor.listenTCP(defaultTwistedServerPort, clientConnectionFactory, interface="192.168.95.109")
    motivePort = reactor.connectTCP("localhost", defaultMotivePort, MotiveProtocolFactory(clientConnectionFactory))

    # syntax: sendToAll() passes result of the call to LoopingCall
    # instead, you should omit the () to pass sendToAll as an argument
    # l = task.LoopingCall(clientConnectionFactory.sendToAll)
    # l.start(0.1)    # data send rate

    print 'Serving Motive messages from %s to clients at %s' % (motivePort, piPort)

    reactor.run()