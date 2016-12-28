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
import sys

import evdev

# --------------------------------------------------------------------------------
# Find controller, if connected
# --------------------------------------------------------------------------------
devices = [evdev.InputDevice(fn) for fn in evdev.list_devices()]
ps3 = None
for device in devices:
   if "PLAYSTATION(R)3" in device.name:
      ps3 = device

if ps3:
   print "found PS3 controller"
   #print ps3.capabilities(verbose=True)
else:
   print "Working sans PS3 controller (commands only)"
   
# ---------------------------------------------------------------------------------
# Find Larry's IP given his probable IPs
# ---------------------------------------------------------------------------------
def ping(host):
    """
    Returns True if host responds to a ping request
    Taken from: http://stackoverflow.com/questions/2953462/pinging-servers-in-python
    (just in case I need this to be portable)
    """
    import subprocess, platform

    # Ping parameters as function of OS
    ping_str = "-n 1" if  platform.system().lower()=="windows" else "-c 1 -W 1 -q"
    args = "ping " + " " + ping_str + " " + host
    need_sh = False if  platform.system().lower()=="windows" else True

    # Ping, discarding output for quiet operation.
    return subprocess.call(args, shell=need_sh, stdout=subprocess.PIPE) == 0

# test call
possible_ips = ["192.168.0.20", "192.168.1.180"]
larry_ip = None
for ip in possible_ips:
   if ping(ip):
      larry_ip = ip
      break

if larry_ip is None:
   print "Is Larry on?"
   sys.exit(1)

print "Larry found at {}".format(larry_ip)
   

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
# Taken from https://wiki.wxpython.org/DoubleBufferedDrawing
#---------------------------------------------------------------------------
USE_BUFFERED_DC = True
class BufferedWindow(wx.Window):

     """

     A Buffered window class.

     To use it, subclass it and define a Draw(DC) method that takes a DC
     to draw to. In that method, put the code needed to draw the picture
     you want. The window will automatically be double buffered, and the
     screen will be automatically updated when a Paint event is received.

     When the drawing needs to change, you app needs to call the
     UpdateDrawing() method. Since the drawing is stored in a bitmap, you
     can also save the drawing to file by calling the
     SaveToFile(self, file_name, file_type) method.

     """
     def __init__(self, *args, **kwargs):
         # make sure the NO_FULL_REPAINT_ON_RESIZE style flag is set.
         kwargs['style'] = kwargs.setdefault('style', wx.NO_FULL_REPAINT_ON_RESIZE) | wx.NO_FULL_REPAINT_ON_RESIZE
         wx.Window.__init__(self, *args, **kwargs)
 
         self.Bind (wx.EVT_PAINT, self.OnPaint)
         self.Bind (wx.EVT_SIZE, self.OnSize)
 
         # OnSize called to make sure the buffer is initialized.
         # This might result in OnSize getting called twice on some
         # platforms at initialization, but little harm done.
         self.OnSize(None)
         self.paint_count = 0
 
     def Draw(self, dc):
         ## just here as a place holder.
         ## This method should be over-ridden when subclassed
         pass
 
     def OnPaint(self, event):
         # All that is needed here is to draw the buffer to screen
         if USE_BUFFERED_DC:
             dc = wx.BufferedPaintDC(self, self._Buffer)
         else:
             dc = wx.PaintDC(self)
             dc.DrawBitmap(self._Buffer, 0, 0)
 
     def OnSize(self,event):
         # The Buffer init is done here, to make sure the buffer is always
         # the same size as the Window
         #Size  = self.GetClientSizeTuple()
         Size  = self.ClientSize
 
         # Make new offscreen bitmap: this bitmap will always have the
         # current drawing in it, so it can be used to save the image to
         # a file, or whatever.
         self._Buffer = wx.Bitmap(*Size)
         self.UpdateDrawing()
 
     def SaveToFile(self, FileName, FileType=wx.BITMAP_TYPE_PNG):
         ## This will save the contents of the buffer
         ## to the specified file. See the wxWindows docs for 
         ## wx.Bitmap::SaveFile for the details
         self._Buffer.SaveFile(FileName, FileType)
 
     def UpdateDrawing(self):
         """
         This would get called if the drawing needed to change, for whatever reason.
 
         The idea here is that the drawing is based on some data generated
         elsewhere in the system. If that data changes, the drawing needs to
         be updated.
 
         This code re-draws the buffer, then calls Update, which forces a paint event.
         """
         dc = wx.MemoryDC()
         dc.SelectObject(self._Buffer)
         self.Draw(dc)
         del dc # need to get rid of the MemoryDC before Update() is called.
         self.Refresh()
         self.Update()
         
class BitmapWindow(BufferedWindow):
     def __init__(self, *args, **kwargs):
         ## Any data the Draw() function needs must be initialized before
         ## calling BufferedWindow.__init__, as it will call the Draw
         ## function.
         self.bitmap = wx.Bitmap(kwargs['size'])
         BufferedWindow.__init__(self, *args, **kwargs)
         
     def SetBitmap(self, bitmap):
         self.bitmap = bitmap
         self.UpdateDrawing()
 
     def Draw(self, dc):
         dc.SetBackground( wx.Brush("White") )
         dc.Clear() # make sure you clear the bitmap!

         dc.DrawBitmap(self.bitmap, 0, 0)
         
# -------------------------------------------------------------------------
# The controller model keeps track of the state of our motion parameters.
# We keep it synched between the client and Larry.
# -------------------------------------------------------------------------
class ControllerModel():
   def __init__(self):
      self.vforward = 0;
      self.vbackward = 0;
      self.vfspeed = 0;
      self.vbspeed = 0;
      
      self.socket = False
      self.vars = {}
      
   def set_socket(self, socket):
      self.socket = socket
      
   def setvar(self, key, value):
      if not key in self.vars or self.vars[key] != value:
         self.vars[key] = value
         if self.socket:
            self.socket.send ("{}={}\n".format(key, value))

   def stop(self):
       self.vbackward = 0
       self.vforward = 0
       self.vfspeed = 0
       self.vbspeed = 0
       self.setvar('FS', 0)
       self.setvar('BS', 0)
       #print "stop"

   def forward(self, evalue):
      if evalue == 1:
         if self.vbackward:
            self.stop()
         else:
            self.vforward = 1
            #print "go forward"
      else:
         self.stop()

   def fspeed(self, evalue):
      if self.vforward:
         speed = 1 + evalue / 10
         if speed != self.vfspeed:
            self.vfspeed = speed
            self.setvar('FS', speed)
 
   def backward(self, evalue):
      if evalue == 1:
         if self.vforward:
            self.stop()
         else:
            self.vbackward = 1
            #print "go backward"
      else:
         self.stop()

   def bspeed(self, evalue):
      if self.vbackward:
         speed = 1 + evalue / 10
         if speed != self.vbspeed:
            self.vbspeed = speed
            self.setvar('BS', speed)
            
   def leftright(self, evalue):
      if evalue < 118 or evalue > 140:
         pspeed = (evalue - 128) / 10
         if pspeed < 0: pspeed += 1
         self.setvar('PS', pspeed)
      else:
         self.setvar('PS', 0)

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
        self.command.SetFont(font)
        self.command.Bind (wx.EVT_TEXT_ENTER, self.OnEnter)
        vertical_sizer.Add (self.command, 0, wx.EXPAND, 0)

        self.output = wx.TextCtrl(self, -1, "",
                                 wx.DefaultPosition, wx.Size(800,300),
                                 wx.TE_MULTILINE|wx.TE_READONLY)
        self.output.SetFont(font)
        vertical_sizer.Add (self.output, 1, wx.EXPAND, 0)
        
        self.image = BitmapWindow(self, size=wx.Size(320, 240))
        self.bitmap = wx.Bitmap(320, 240)
        self.refresh_bitmap = False
        self.image_counter = 0
        vertical_sizer.Add (self.image, 0, wx.ALIGN_CENTER)
        
        self.current_image = None
        
        self.SetSizer( vertical_sizer)
        vertical_sizer.SetSizeHints(self)
        vertical_sizer.Fit(self)

        self.Bind(wx.EVT_CLOSE, self.OnCloseWindow)

        self.tn = Telnet(larry_ip, 333) # connect to host
        self.timer = UpdateTimer(self, 100)
        
        if ps3:
           self.controller_thread = threading.Thread(target=self.controller_thread_handler)
           self.controller_thread.start()
        else:
           self.cmodel = False


    def OnUpdate(self):
        """Output what's in the telnet buffer and update the bitmap, if any"""
        if self.refresh_bitmap:
           self.image.SetBitmap(self.bitmap)
           self.image.SaveToFile ("{}.jpg".format(self.image_counter))
           self.image_counter += 1
           self.refresh_bitmap = False
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
        self.socket.connect((larry_ip, self.oob_port))
        if self.cmodel:
           self.cmodel.set_socket(self.socket)
        state = 0
        image = ''
        while True:
           data = self.socket.recv(1024)
           if not data: break
           
           # OK, we have some data on the OOB channel.
           if state == 1:
              image += data
              if image.endswith("+++done\r\n"):
                 state = 2
              
           if state == 0:
              if (data.startswith('+')):
                 image = data[1:]
                 if image.endswith("+++done\r\n"):
                    state = 2
                 else:
                    state = 1
              elif data.startswith('='):
                 # Value coming in
                 pass
                 
           if state == 2:
              if image.endswith("+++done\r\n"):
                 # I'm loading into a PIL image first (ref: https://wiki.wxpython.org/WorkingWithImages)
                 # so that I can turn it upside down.
                 try:
                    im = Image.open(StringIO.StringIO(image[:-9])).transpose(Image.ROTATE_180)
                    self.bitmap = wx.Bitmap.FromBuffer (im.size[0], im.size[1], im.tobytes());
                    self.refresh_bitmap = True
                 except:
                    pass
                 image = ''
                 state = 0
              else:
                 state = 1
                 
    def controller_thread_handler(self):
        """Thread to watch the controller, if one is connected, and do the Right Thing"""
        self.controller_stop = False
        self.cmodel = ControllerModel()
        for event in ps3.read_loop():
            if event.code == 297:
               self.cmodel.forward(event.value)
            if event.code == 49:
               self.cmodel.fspeed(event.value)
            if event.code == 296:
               self.cmodel.backward(event.value)
            if event.code == 48:
               self.cmodel.bspeed(event.value)
               
            if event.code == 0 and event.type == 3:
               self.cmodel.leftright(event.value)

            if self.controller_stop: break
            
    def OnEnter(self, e):
        """The user has entered a command.  Send it!"""
        input = e.GetString()
        
        self.output.AppendText(input + "\r\n")
        self.tn.write(str(input + "\r\n"))
        self.command.SetSelection(0, self.command.GetLastPosition()) 
        
    def OnCloseWindow(self, event):
        """We're closing, so clean up."""
        self.timer.Stop()
        self.controller_stop = 1
        self.Destroy()

#---------------------------------------------------------------------------

#if __name__ == '__main__':
import sys
app = wx.App()
frame = TelnetFrame(None, -1)
frame.Show(True)
app.MainLoop()
