# The "client" is a tool for remote-controlling the robot; it should gradually
# be supplanted by automation on the base (which, for the time being, is
# implemented by the human operator).
#
# This initial version is just a telnet client that knows how to field
# the image packets I was sending back from the ESP8266 version of the
# head. It's obsolete, but still a starting point.

#from wx import *
import wx
#from wx.lib.layoutf import Layoutf
from telnetlib import Telnet # core module
import base64
import StringIO
import socket
import threading
from PIL import Image


#---------------------------------------------------------------------------

class UpdateTimer(wx.Timer):
    def __init__(self, target, dur=1000):
        wx.Timer.__init__(self)
        self.target = target
        self.Start(dur)

    def Notify(self):
        """Called every timer interval"""
        if self.target:
            self.target.OnUpdate()

#---------------------------------------------------------------------------
class TelnetFrame(wx.Frame):
    def __init__(self, parent, ID):
        wx.Frame.__init__(self, parent, ID, "Telnet")

        font = wx.Font(8, wx.MODERN, wx.NORMAL, wx.NORMAL, faceName="Lucida Console")
        vertical_sizer = wx.BoxSizer( wx.VERTICAL )

        # Command text box
        commandId = wx.NewId()
        self.command = wx.TextCtrl(self, commandId, "",
                                  wx.DefaultPosition, wx.DefaultSize,
                                  wx.TE_PROCESS_ENTER)
        #self.command.SetConstraints(Layoutf('t=t2#1;r=r2#1;b!32;l=l2#1', (self,)))
        self.command.SetFont(font)
        self.Bind (wx.EVT_TEXT_ENTER, self.OnEnter)
        vertical_sizer.Add (self.command, 0, wx.EXPAND, 0)

        # Output text box
        self.output = wx.TextCtrl(self, -1, "",
                                 wx.DefaultPosition, wx.Size(100,150),
                                 wx.TE_MULTILINE|wx.TE_READONLY)
        #self.output.SetConstraints(Layoutf('t_2#2;r=r2#1;b=b2#1;l=l2#1', (self,self.command)))
        self.output.SetFont(font)
        vertical_sizer.Add (self.output, 1, wx.EXPAND, 0)
        
        self.image = wx.StaticBitmap(self)
        self.image.SetBitmap(wx.Bitmap(320, 240))
        #self.image.SetConstraints(Layoutf('t_2#2;r=r2#1;b*;l*', (self, self.output)))
        vertical_sizer.Add (self.image, 0, wx.ALIGN_CENTER)
        
        self.current_image = None
        
        self.SetSizer( vertical_sizer)
        vertical_sizer.SetSizeHints(self)
        vertical_sizer.Fit(self)

        self.Bind(wx.EVT_CLOSE, self.OnCloseWindow)

        self.tn = Telnet('192.168.0.20', 333) # connect to host
        self.timer = UpdateTimer(self, 100)

    def OnUpdate(self):
        """Output what's in the telnet buffer"""
        try:
            newtext = self.tn.read_very_eager()
            if newtext:
                if newtext.startswith('+img:'):
                    self.image_packet(newtext);
                    #self.output.AppendText("image packet {0}\n".format(len(newtext)))
                else:
                    lines = newtext.split('\n') # This keeps it from printing out extra blank lines.
                    for line in lines:
                        self.output.AppendText(line)
                        if line.startswith('105 '):
                            port = ''.join(i for i in line[5:] if i.isdigit())
                            self.oob_port = int(port)
                            self.output.AppendText("--> port {}\n".format(port))
                            self.thread = threading.Thread(target=self.oob_thread)
                            self.thread.start()
        except EOFError:
            self.tn.close()
            self.timer.Stop()
            self.output.AppendText("Disconnected by remote host")
            
    def oob_thread(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect(('192.168.0.20', self.oob_port))
        state = 0
        image = ''
        while True:
           data = self.socket.recv(1024)
           if not data: break
           
           # OK, we have some data on the OOB channel.
           if state == 1:
              image += data
              print "state 1"
              if image.endswith("+++done\r\n"):
                 print "done"
                 state = 2
              
           if state == 0:
              if (data.startswith('+')):
                 image = data[1:]
                 print "starting image"
                 if image.endswith("+++done\r\n"):
                    print "done in first"
                    state = 2
                 else:
                    print "more to come"
                    state = 1
              elif data.startswith('='):
                 # Value coming in
                 pass
                 
           if state == 2:
              print "image done"
              if image.endswith("+++done\r\n"):
                 # I'm loading into a PIL image first (ref: https://wiki.wxpython.org/WorkingWithImages)
                 # so that I can turn it upside down.
                 try:
                    im = Image.open(StringIO.StringIO(image[:-9])).transpose(Image.ROTATE_180)
                    print im.size
                    bitmap = wx.Bitmap.FromBuffer (im.size[0], im.size[1], im.tobytes());
                    print "made bitmap"
                    self.image.SetBitmap (bitmap)
                 except:
                    pass
                 print "!"
                 image = ''
                 state = 0
              else:
                 print "oops"
                 state = 1
            
    def OnEnter(self, e):
        """The user has entered a command.  Send it!"""
        input = e.GetString()
        
        self.output.AppendText(input + "\r\n")
        self.tn.write(str(input + "\r\n"))
        self.command.SetSelection(0, self.command.GetLastPosition()) 
        
    def OnCloseWindow(self, event):
        """We're closing, so clean up."""
        self.timer.Stop()
        self.Destroy()

#---------------------------------------------------------------------------

if __name__ == '__main__':
    import sys
    app = wx.App()
    frame = TelnetFrame(None, -1)
    frame.Show(True)
    app.MainLoop()
