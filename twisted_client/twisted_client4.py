"""
now implements netstring receiver when connecting to twisted_server
"""

from twisted.internet.protocol import Protocol, Factory, ClientFactory
from twisted.protocols.basic import NetstringReceiver, LineOnlyReceiver
from twisted.internet import task, defer, endpoints
import sys, socket, struct, time

from twisted.internet import reactor

hostName = 'bach.ese.wustl.edu'

host = None
defaultTwistedServerPort = 53335

currTime = 0.0
prevTime = 0.0
accumTime = 0.0

# use win32 reactor if applicable
if sys.platform == 'win32':
    from twisted.internet import win32eventreactor

    win32eventreactor.install()


# find hostname
def findHost():
    addr = socket.gethostbyname(hostName)
    return addr


######################################################3

class SocketClientProtocol(NetstringReceiver):
    def __init__(self):
        self.MAX_LENGTH = 33    # seven 4-byte values

    # after int prefix and other framing are removed:
    def stringReceived(self, string):
        global currTime
        currTime = time.time()
        try:
            #format: mmm ss mm hh x y z qx qy qw qz
            self.factory.dataBuffer.append(struct.unpack("!HBBBfffffff", string))
            self.factory.got_msg()
        except:
            print 'Failed to unpack\n'

    def connectionMade(self):  # calls when connection is made with Twisted server
        self.transport.setTcpNoDelay(True)
        print ("connected to twisted server")
        self.factory.clientReady(self)


class SocketClientFactory(ClientFactory):
    """ Created with callbacks for connection and receiving.
        send_msg can be used to send messages when connected.
    """
    protocol = SocketClientProtocol

    def __init__(self):
        self.client = None
        self.dataBuffer = []

    def clientConnectionFailed(self, connector, reason):
        print ("connection failed because of %s"), reason
        outputFile.close()
        reactor.stop()

    def clientConnectionLost(self, connector, reason):
        print ("connection lost due to: %s"), reason
        outputFile.close()
        reactor.stop()

    def clientReady(self, client):
        self.client = client

    def got_msg(self, msg):
        global prevTime, currTime, accumTime
        timeInterval = currTime - prevTime

        if prevTime > 0.0:
            accumTime += timeInterval

        for payload in self.dataBuffer:
            outputFile.write('%s\t' % str(payload))
        outputFile.write('\tTime Interval: %f\tRunning Time: %f\n' % (timeInterval, accumTime))

        prevTime = currTime

###########################################

if __name__ == '__main__':
    print('starting program')
    host = findHost()
    outputFile = open('motive_results.txt', 'w+')
    if (host is not None):
        print ('Attempting connection to %s:%s') % (host, defaultTwistedServerPort)
        reactor.connectTCP(host, defaultTwistedServerPort, SocketClientFactory())
        reactor.run()
    else:
        print ("could not find host")
