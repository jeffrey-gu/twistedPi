"""
Same as version 2, except with time interval benchmarks.
"""

from twisted.internet.protocol import Protocol, Factory, ClientFactory
from twisted.protocols.basic import IntNStringReceiver, LineOnlyReceiver
from twisted.internet import task, defer, endpoints
import sys, socket, struct, time

from twisted.internet import reactor

<<<<<<< HEAD
# hostName = 'YuchiLi-PC'
hostName = 'bach.ese.wustl.edu'
# hostName = 'jeffrey-K501UX'
=======
hostName = 'bach.ese.wustl.edu'
>>>>>>> f37b2ffd1aa02d83287394f7f15fd6fbb19292d3

host = None
defaultTwistedServerPort = 53335
defaultUserPort = 5000

prevTime = 0.0
accumTime = 0.0

# options = ['a', 'm', 'o', '1', '2', '3', '4', '5', '6', '7', '8', '9', '10']

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

    # after int prefix and other framing are removed:
    def lineReceived(self, line):
        # print ("line received")
        self.factory.got_msg(line)

    def connectionMade(self):   # calls when connection is made with Twisted server
        print ("connected to twisted server")
        self.factory.clientReady(self)


class SocketClientFactory(ClientFactory):
    """ Created with callbacks for connection and receiving.
        send_msg can be used to send messages when connected.
    """
    protocol = SocketClientProtocol

    def __init__(self):
        # store reference to client
        self.client = None

    def clientConnectionFailed(self, connector, reason):
        print ("connection failed")
        reactor.stop()

    def clientConnectionLost(self, connector, reason):
        print ("connection lost due to: %s"), reason
        outputFile.close()

    def clientReady(self, client):
        self.client = client

    def got_msg(self, msg):
        # print (msg)

        global prevTime, accumTime

        currTime = time.time()
        timeInterval = currTime - prevTime

        if prevTime > 0.0:
            accumTime += timeInterval
        
        outputFile.write(msg)
        
        outputFile.write('\tTime Interval: %f\tRunning Time: %f\n' %(timeInterval, accumTime))

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
