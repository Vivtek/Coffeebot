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
        self.Bind (wx.EVT_TEXT_ENTER, self.OnEnter)
        vertical_sizer.Add (self.command, 0, wx.EXPAND, 0)

        self.output = wx.TextCtrl(self, -1, "",
                                 wx.DefaultPosition, wx.Size(100,150),
                                 wx.TE_MULTILINE|wx.TE_READONLY)
        self.output.SetFont(font)
        vertical_sizer.Add (self.output, 1, wx.EXPAND, 0)
        
        self.image = BitmapWindow(self, size=wx.Size(320, 240))
        self.bitmap = wx.Bitmap(320, 240)
        self.refresh_bitmap = False
        vertical_sizer.Add (self.image, 0, wx.ALIGN_CENTER)
        
        self.current_image = None
        
        self.SetSizer( vertical_sizer)
        vertical_sizer.SetSizeHints(self)
        vertical_sizer.Fit(self)

        self.Bind(wx.EVT_CLOSE, self.OnCloseWindow)

        self.tn = Telnet('192.168.0.20', 333) # connect to host
        self.timer = UpdateTimer(self, 100)

    def OnUpdate(self):
        """Output what's in the telnet buffer and update the bitmap, if any"""
        if self.refresh_bitmap:
           self.image.SetBitmap(self.bitmap)
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
        self.socket.connect(('192.168.0.20', self.oob_port))
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
              print "image done"
              if image.endswith("+++done\r\n"):
                 # I'm loading into a PIL image first (ref: https://wiki.wxpython.org/WorkingWithImages)
                 # so that I can turn it upside down.
                 try:
                    im = Image.open(StringIO.StringIO(image[:-9])).transpose(Image.ROTATE_180)
                    self.bitmap = wx.Bitmap.FromBuffer (im.size[0], im.size[1], im.tobytes());
                    self.refresh_bitmap = True
                 except:
                    pass
                 print "!"
                 image = ''
                 state = 0
              else:
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
