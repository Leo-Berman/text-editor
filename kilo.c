/***Currently on step 29***/
/* Includes */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

/* Definitions */

// set a macro that is a mask that uses the control key
//
#define CTRL_KEY(k) ((k) & 0x1f)

// create a struct from the termios.h library
// has 3 sets of flags for interfacing with io
//
struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};
struct editorConfig E;

// Error handling
//
void die(const char *s) {
  
  // clear the screen
  //
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  // perror prints an error message
  //
  perror(s);

  // Exit the program
  //
  exit(1);
}

// return to normal mode
//
void disableRawMode() {

  // Variables from unistd.h
  // STDIN_FILENO = 0 (number of stdin)
  // TCSAFLUSH = change attributes when drained and flush input
  // This is essentially resetting the input mode 
  //
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
}

// enter rawmode which disables a lot of the flags
//
void enableRawMode() {

  // gets current parameter for terminals
  // with error handling
  //
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1){
    die("tcsetattr");
  }
    
  // When leaving this program exit raw mode
  //
  atexit(disableRawMode);

  // create a copy of the current struct to manipulate
  //
  struct termios raw = E.orig_termios;

  // Set flags 
  // ECHO = let characters show as you type them
  // ICANON = Canonical mode
  // ISIG = Ctrl-C and Ctrl-Z
  // XIXON = Ctrl-S and Ctrl -Q
  // IEXTEN = Ctrl-V
  // ICRNL = Ctrl-M
  // OPOST = Output processing
  // CS8 = bitsize
  // BRKINT = Break condition to end program
  // ISTRIP = Bit stripping 8th bit -> 0
  // INPCK = Parity checking
  //
  raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | ICRNL | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  // VMIN sets minimum number of bits to be read
  // VTIME sets maximum amount of time to wait before
  // read()
  //
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  // sets parameters to what we declared above
  // with error handling
  //
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

char editorReadKey() {
  
  // nread keeps track of the return value for read
  //
  int nread;
  
  // storage point for holding the character read
  //
  char c;
  
  // Says when nread reads a single byte, return it
  //
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}

void editorProcessKeypress() {

  // read a single byte
  //
  char c = editorReadKey();

  // handle error checking
  //
  switch (c) {

    // if ctrl+q pressed then break
    //
    case CTRL_KEY('q'):
      
      // clear the screen and exit the program
      //
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }

}


void editorDrawRows() {
  
  // number to keep track of which row 
  // the loop is on
  //
  int y;
  
  // loop through the rows
  //
  for (y = 0; y < 24; y++) {
    
    // write a tilda to stdout
    //
    write(STDOUT_FILENO, "~\r\n", 3);

  }
}


void editorRefreshScreen() {
  
  // \x1b is the escape character
  //

  // [2j is erasing all of the display
  //
  write(STDOUT_FILENO, "\x1b[2J", 4);

  // [H moves the cursor to the top left position
  //
  write(STDOUT_FILENO, "\x1b[H", 3);

  // draw rows and move the cursor back to the top 
  // left of the screen
  //
  editorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
  
}

int getWindowSize(int *rows, int *cols) {

  // winsize struct comes from sys/ioctl.h
  // and it holds a lot of the info of the display
  // screen
  //
  struct winsize ws;
  
  // places size of the window into a struct and checks
  // to see if the dimensions are non-zero
  //
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    
    // return a fail
    //
    return -1;

  } else {

    // sets the rows and cols height to what we measured
    //
    *cols = ws.ws_col;
    *rows = ws.ws_row;

    // return a success
    //
    return 0;
  }
}

void initEditor() {

  // if getting window size fails error
  //
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");

}

int main() {

  // enables byte by byte reading without having to press enter
  //
  enableRawMode();
  
  // initialize editor
  //
  initEditor();

  // infinite loop
  //
  while (1) {
    
    // clear the screen
    //
    editorRefreshScreen();

    // process the keypress
    //
    editorProcessKeypress();

  }

  // exit gracefully
  //
  return 0;
}