"""
cleaned up version of twisted_client2
"""

from twisted.internet.protocol import Protocol, Factory, ClientFactory
from twisted.protocols.basic import IntNStringReceiver, LineOnlyReceiver
from twisted.internet import task, defer, endpoints
import sys, socket, struct, time

from twisted.internet import reactor

hostName = 'bach.ese.wustl.edu'

host = None
defaultTwistedServerPort = 53335

prevTime = 0.0
currTime = 0.0
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

class SocketClientProtocol(LineOnlyReceiver):
    # def __init__(self):
        # self.delimiter = '\n'

    # after int prefix and other framing are removed:
    def lineReceived(self, line):
        global currTime
        currTime = time.time()
        self.factory.got_msg(line)

    def connectionMade(self):   # calls when connection is made with Twisted server
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
        self.dataBuff = []

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
        self.dataBuff.append(msg)
        timeInterval = currTime - prevTime

        if prevTime > 0.0:
            accumTime += timeInterval

        for payload in self.dataBuff:
            outputFile.write(payload)
        outputFile.write('\tTime Interval: %f\tRunning Time: %f\n' %(timeInterval, accumTime))

        self.dataBuff = []
        prevTime = currTime

    #TODO: NOT CALLED --> do we want clients to send messages to server?
    # def send_msg(self, msg):
    #     if self.client:
    #         self.client.sendLine(msg)

###########################################

if __name__ == '__main__':
    print('starting program')
    host = findHost()
    outputFile = open('motive_results.txt', 'w+')
    if(host is not None):
        print ('Attempting connection to %s:%s') %(host, defaultTwistedServerPort)
        reactor.connectTCP(host, defaultTwistedServerPort, SocketClientFactory())
        reactor.run()
    else:
        print ("could not find host")
