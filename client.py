# The "client" is a tool for remote-controlling the robot; it should gradually
# be supplanted by automation on the base (which, for the time being, is
# implemented by the human operator).
#
# This initial version is just a telnet client that knows how to field
# the image packets I was sending back from the ESP8266 version of the
# head. It's obsolete, but still a starting point.

from wxPython.wx import *
from wxPython.lib.layoutf import Layoutf
from telnetlib import Telnet # core module
import base64
import StringIO

#---------------------------------------------------------------------------

class UpdateTimer(wxTimer):
    def __init__(self, target, dur=1000):
        wxTimer.__init__(self)
        self.target = target
        self.Start(dur)

    def Notify(self):
        """Called every timer interval"""
        if self.target:
            self.target.OnUpdate()

#---------------------------------------------------------------------------
class TelnetFrame(wxFrame):
    def __init__(self, parent, ID):
        wxFrame.__init__(self, parent, ID, "Telnet")

        font = wxFont(8, wxMODERN, wxNORMAL, wxNORMAL, faceName="Lucida Console")
        vertical_sizer = wxBoxSizer( wxVERTICAL )

        # Command text box
        commandId = wxNewId()
        self.command = wxTextCtrl(self, commandId, "",
                                  wxPyDefaultPosition, wxPyDefaultSize,
                                  wxTE_PROCESS_ENTER)
        #self.command.SetConstraints(Layoutf('t=t2#1;r=r2#1;b!32;l=l2#1', (self,)))
        self.command.SetFont(font)
        EVT_TEXT_ENTER(self, commandId, self.OnEnter)
        vertical_sizer.Add (self.command, 0, wxEXPAND, 0)

        # Output text box
        self.output = wxTextCtrl(self, -1, "",
                                 wxPyDefaultPosition, wxSize(100,150),
                                 wxTE_MULTILINE|wxTE_READONLY)
        #self.output.SetConstraints(Layoutf('t_2#2;r=r2#1;b=b2#1;l=l2#1', (self,self.command)))
        self.output.SetFont(font)
        vertical_sizer.Add (self.output, 1, wxEXPAND, 0)
        
        self.image = wxStaticBitmap(self, bitmap=wxEmptyBitmap(320, 240))
        self.image.SetConstraints(Layoutf('t_2#2;r=r2#1;b*;l*', (self, self.output)))
        vertical_sizer.Add (self.image, 0, wxALIGN_CENTER)
        
        self.current_image = None
        
        self.SetSizer( vertical_sizer)
        vertical_sizer.SetSizeHints(self)
        vertical_sizer.Fit(self)

        EVT_CLOSE(self, self.OnCloseWindow)

        self.tn = Telnet('192.168.4.1', 333) # connect to host
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
        except EOFError:
            self.tn.close()
            self.timer.Stop()
            self.output.AppendText("Disconnected by remote host")
            
    def image_packet(self, text):
        """We have received a packet for an image. Do the right thing."""
        pieces = text.split("\n")
        for piece in pieces:
            piece = piece.strip()
            if piece == "+img:":
                pass
            elif piece == "done":
                self.output.AppendText("-->image done {0}\r\n? ".format(len(self.current_image)))
                image = wxImageFromStream(StringIO.StringIO(self.current_image), wxBITMAP_TYPE_JPEG);
                self.image.SetBitmap(wxBitmapFromImage(image))
                self.current_image = None
            elif piece == "":
                pass
            elif piece == "?":
                pass
            else:
                piece += "="
                chunk = base64.b64decode(piece);
                self.output.AppendText("---> image packet {0}\n".format(len(chunk)))
                if self.current_image is None:
                    self.current_image = chunk[1:] # Turns out ArduCAM sends a leading zero (or their example code, anyway).
                else:
                    self.current_image += chunk
        
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
    app = wxPySimpleApp()
    frame = TelnetFrame(None, -1)
    frame.Show(true)
    app.MainLoop()
