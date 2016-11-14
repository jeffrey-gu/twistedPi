"""
This is identical to twisted_client.py, except that it writes all received positional data to a file

"""

from twisted.internet.protocol import Protocol, Factory, ClientFactory
from twisted.protocols.basic import IntNStringReceiver, LineOnlyReceiver
from twisted.internet import task, defer, endpoints
import sys, socket, struct

from twisted.internet import reactor

# hostName = 'YuchiLi-PC'
hostName = 'bach.ese.wustl.edu'
# hostName = 'jeffrey-K501UX'

host = None
defaultTwistedServerPort = 53335
defaultUserPort = 5000

# options = ['a', 'm', 'o', '1', '2', '3', '4', '5', '6', '7', '8', '9', '10']

# use win32 reactor if applicable
if sys.platform == 'win32':
    from twisted.internet import win32eventreactor
    win32eventreactor.install()

# find hostname
def findHost():
    # TODO: error case for host not found
    addr = socket.gethostbyname(hostName)
    return addr

######################################################3

class SocketClientProtocol(LineOnlyReceiver):

    # after int prefix and other framing are removed:
    def lineReceived(self, line):
        print ("line received")
        self.factory.got_msg(line)

    def connectionMade(self):   # calls when connection is made with Twisted server
        print ("connected to twisted server")
        self.factory.clientReady(self)


class SocketClientFactory(ClientFactory):
    """ Created with callbacks for connection and receiving.
        send_msg can be used to send messages when connected.
    """
    protocol = SocketClientProtocol

    # def __init__(
    #         self,
    #         connect_success_callback,
    #         connect_fail_callback,
    #         recv_callback):

    def __init__(self):
        # self.connect_success_callback = connect_success_callback
        # self.connect_fail_callback = connect_fail_callback
        # self.recv_callback = recv_callback

        # store reference to client
        self.client = None

    def clientConnectionFailed(self, connector, reason):
        # self.connect_fail_callback(reason)
        print ("connection failed")
        reactor.stop()

    def clientConnectionLost(self, connector, reason):
        print ("connection lost due to: %s"), reason
        outputFile.close()

    def clientReady(self, client):
        self.client = client
        # self.connect_success_callback()

    def got_msg(self, msg):
        # self.recv_callback()
        print (msg)
        outputFile.write(msg)
        outputFile.write('\n')

    #TODO: NOT CALLED --> do we want clients to send messages to server?
    # def send_msg(self, msg):
    #     if self.client:
    #         self.client.sendLine(msg)

###########################################

if __name__ == '__main__':
    print('starting program')
    host = findHost()
    outputFile = open('motive_results.txt', 'w+')
    # host = 128.
    # host = "192.168.95.219"
    # host = "128.252.19.161"
    # host = "bach.ese.wustl.edu"
    if(host is not None):
        print ('Attempting connection to %s:%s') %(host, defaultTwistedServerPort)
        reactor.connectTCP(host, defaultTwistedServerPort, SocketClientFactory())
        # reactor.connectTCP("bach.ese.wustl.edu", defaultTwistedServerPort, SocketClientFactory())
        reactor.run()
    else:
        print ("could not find host")
