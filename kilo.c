/*** Next step is step 105 ***/

// feature test macros
// this is essentially adding all the 
// features to make this text editor
// more portable
//
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


/* Includes */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>

/* Definitions */

// set a macro that is a mask that uses the control key
//
#define CTRL_KEY(k) ((k) & 0x1f)

// default buffer initializer
//
#define ABUF_INIT {NULL, 0}

// Version type
//
#define KILO_VERSION "Leo's Kilo Text Editor V1"

// tab size
//
#define KILO_TAB_STOP 8

// constants for the arrow keys mapped to
// integers
//
enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

// create a storage object for each row
//
typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
}erow;

// has 3 sets of flags for interfacing with io
// cx and cy are cursor position
// rx is a variable that compensates for tabs
// rowoff is to keep track of what row the user is on in the text file
// coloff is to keep track of what column the user is on in the text file
// screenrows and screencols are the size of the terminal
// create a struct from the termios.h library
// termios = declare the termios to hold terminal info
// numrows is number of rows
// erow is row information in the form of a array of rows
// filename is a character array with the name of the file
// statusmessage is a character array with some info on the file
// statusmsg_time is how long the message has been up
//
struct editorConfig {
  int cx,cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  struct termios orig_termios;
  int numrows;
  erow *row;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
};

// initialize the editor config
//
struct editorConfig E;

// create an append buffer
//
struct abuf {
  char *b;
  int len;
};

/* Functions */

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

void abAppend(struct abuf *ab, const char *s, int len) {

  // add extra length to the buffer
  //
  char *new = realloc(ab->b, ab->len + len);

  // if failed return
  //
  if (new == NULL){
    return;
  }

  // copy the string into the current memory
  // it copies the memory to the last area in
  // the character array in the appendbuffer
  //
  memcpy(&new[ab->len], s, len);

  // set the character array to the new string
  //
  ab->b = new;

  // increase the length by the length of
  // the new appended string
  //
  ab->len += len;
}

void abFree(struct abuf *ab) {

  // free the string from the stack
  //
  free(ab->b);

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
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
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

int editorReadKey() {
  
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

  // if it reads an escape sequence
  //
  if (c == '\x1b') {
    
    // declare a buffer to read into
    //
    char seq[3];
    
    // if it can't read into buffer, then return the plain escape
    // sequence
    //
    if (read(STDIN_FILENO, &seq[0], 1) != 1) {
      return '\x1b';
    }
    if (read(STDIN_FILENO, &seq[1], 1) != 1) {
      return '\x1b';
    }
    // if the first character is indicative 
    //
    if (seq[0] == '[') {

      // /if the byte after is a digit
      //
      if (seq[1] >= '0' && seq[1] <= '9') {

        // if it can't read the digit return the escape character
        //
        if (read(STDIN_FILENO, &seq[2], 1) != 1){
          return '\x1b';
        }

        // if there is a tilda check whether the 
        // previous number fulfills the escape sequence
        //
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      }
      else {

        // check for the correlating arrow key to return
        //
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;

        }
      }
    }

    // check for more escape sequences indicating the home
    // or end key
    //
    else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
    return '\x1b';
  } 
  
  else {
    return c;
  }
}


void editorMoveCursor(int key) {
  
  // set the current row that we are on
  //
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  // depending on the case move the cursor aroudn
  // the if statements prevent the cursor from going
  // off screen
  //
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      }
      else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      }
      else if (row && E.cx == row->size) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows) {
        E.cy++;
      }
      break;
  }
  
  // set the row again since we could be on a different 
  // line
  //
  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  // set the length of the row
  //
  int rowlen = row ? row->size : 0;

  // if it's longer than the row
  // reset to the end of the row
  //
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorProcessKeypress() {

  // read a single byte
  //
  int c = editorReadKey();

  // handle error checking
  //
  switch (c) {

      case '\r':
      /* TODO */
      break;

    // if ctrl+q pressed then break
    //
    case CTRL_KEY('q'):
      
      // clear the screen and exit the program
      //
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;  
    
    case HOME_KEY:
      break;
    case END_KEY:
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      /* TODO */
      break;
    // if page up or page down clicked
    // move the page up or down accordingly
    //
    case PAGE_UP:
    case PAGE_DOWN:

      // if page up then set cy to row offset
      //
      if (c == PAGE_UP) {
          E.cy = E.rowoff;
      }

      // if page down then set cy to the screenrows
      // minus the rowoffset
      //
      else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;

          // if scrolled past the visible screen
          // set it to the bottom
          //
          if (E.cy > E.numrows) {
            E.cy = E.numrows;
          }
      }

      // scroll one visible page length
      //
      int times = E.screenrows; 
      while (times--){
        editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case CTRL_KEY('l'):
    case '\x1b':
      break;

    default:
      editorInsertChar(c);
      break;
  }

}


void editorDrawRows(struct abuf *ab) {
  
  // number to keep track of which row 
  // the loop is on
  //
  int y;
  
  // loop through the rows of the screen
  // and write a tilda at the beginning of each line
  //
  for (y = 0; y < E.screenrows; y++) {
    
    // get user cursor position
    //
    int filerow = y + E.rowoff;

    // check if user is outside of the file 
    //
    if (filerow >= E.numrows) {
      
      // if we are 1/3 the way down the screen
      // and the number of rows is 0
      //
      if (E.numrows == 0 && y == E.screenrows / 3) {
          
        // print out a welcome message
        //
        char welcome[80];

        // return how many characters would have been written
        // if the buffer was the right size
        //
        int welcomelen = snprintf(welcome, sizeof(welcome), "Text editor -- version %s", KILO_VERSION);

        // adjust the size of the buffer and write it
        //
        if (welcomelen > E.screencols) {
          welcomelen = E.screencols;
        }

        // find half way through the screen
        //
        int padding = (E.screencols - welcomelen) / 2;

        // if padding exists
        //
        if (padding) {

          // append a tilda at the start of the line
          // and compensate in the padding
          //
          abAppend(ab, "~", 1);
          padding--;
        
        }

        // print spaces until padding is 0
        //
        while (padding--) {
          abAppend(ab, " ", 1);
        }

        // write the message
        //
        abAppend(ab, welcome, welcomelen);

      } 
      
      else {

        // add a tilda to the beginning of the line
        // 
        abAppend(ab,"~",1);

      }
    }

    else {

      // set length to the row size while 
      // compensating with the offset of the column
      //
      int len = E.row[filerow].rsize - E.coloff;

      // If that number is negative reset it 
      // to the beginning of the column
      //
      if (len < 0){
        len = 0;
      }


      // if the len is greater than a full row
      // set the length to the maximum size
      // of a row
      //
      if (len > E.screencols) {
        len = E.screencols;
      }
          
      // write the row
      //
      abAppend(ab, &E.row[filerow].render[E.coloff], len);
    }

    // deletes part of the line
    //
    abAppend(ab, "\x1b[K", 3);

    // deal with the last line of the terminal
    //
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {

  // switch to inverted colors
  //
  abAppend(ab, "\x1b[7m", 4);
  
  // set a character array to hold the message
  // and a character array for the current line number
  //
  char status[80], rstatus[80];

  // get how many characters would be needed to print the message
  // and load it into the character array
  //
  int len = snprintf(status, sizeof(status), "%.20s - %d lines", E.filename ? E.filename : "[No Name]", E.numrows);
  
  // get how manay characters would be needed and write the message
  // into the character array
  //
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);
  
  // compensate if message is longer than screen length
  //
  if (len > E.screencols) {
    len = E.screencols;
  }

  // write the message
  //
  abAppend(ab, status, len);

  // set the row to be of spaces with a white
  // background and keep track of how long the row is
  //
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    }
    else{
      abAppend(ab, " ", 1);
      len++;
    }
  }

  // switch to original colors
  //
  abAppend(ab, "\x1b[m", 3);

  // print a new line after the status message
  //
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {

  // clear message bar
  //
  abAppend(ab, "\x1b[K", 3);

  // find the length of the status message
  int msglen = strlen(E.statusmsg);

  // if it is longer than the screen
  // compensate for that
  //
  if (msglen > E.screencols) {
    msglen = E.screencols;
  }

  // if it has been less than 5 secs, display
  // the message
  //
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}


int editorRowCxToRx(erow *row, int cx) {

  // calculate position variable
  //
  int rx = 0;
  
  // index variable
  //
  int j;

  // loop through all characters
  //
  for (j = 0; j < cx; j++) {

    // if the character is a tab, set it to the
    // appropriate amount of spaces
    // and increment rx accordingly
    //
    if (row->chars[j] == '\t')
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    rx++;
  }

  // return the row variable
  //
  return rx;
}

void editorScroll() {

  // if the cursor y position is < than the cursor position
  // set the proper rx position
  //
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  // if the cursor is above the visible window
  // change the top of the window
  //
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }

  // if the cursor is below the visible window
  // change the bottom of the window
  //
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }

  // if cursor goes to the left of the visible window
  // move the left of the window
  //
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }

  // if cursor goes to the right of the visible window
  // move the right of the windwo
  //
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

int getCursorPosition(int *rows, int *cols) {

  // variables to iterate through and read
  // the response to the cursor position
  //
  char buf[32];

  // subtract the row offset from the screen
  //
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, E.cx + 1);

  unsigned int i = 0;

  // request cursor position
  //
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4){
    return -1;
  }

  // move crusor to beginning of line and print new line
  //
  printf("\r\n");
  
  // read the cursor position byte by byte
  //
  while (i < sizeof(buf) - 1) {

    // if it fails to read break
    // if it finds 'R' break
    //
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }

  // set the last character to a null terminating character
  //
  buf[i] = '\0';

  // checks to see if it's an escape sequence
  //
  if (buf[0] != '\x1b' || buf[1] != '['){

    // return fail
    //
    return -1;
  }

  // read the two numbers that indicate cursor position
  //
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2){

    // return fail
    //
    return -1;
  }

  // return success
  //
  return 0;
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
  if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    
    // 999C moves cursor right and 999B moves cursord down
    // each by 999 steps
    // write returns number of bytes written so if it is less than 12
    // it will return a faiure
    //
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){
      return -1;
    }
    return getCursorPosition(rows, cols);
    // Reads the character where the cursor is at
    //
    editorReadKey();

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

void editorRefreshScreen() {
  
  /*Freestyling*/

  // if (getWindowSize(&E.screenrows, &E.screencols) == -1){
  //   die("getWindowSize");
  // }

  // E.screenrows-=2;

  /*Freestyling*/

  // scroll to keep the cursor within the
  // visible window
  //
  editorScroll();

  // declare buffer
  //
  struct abuf ab = ABUF_INIT;

  // \x1b is the escape character
  //

  // hide the cursor
  //
  abAppend(&ab, "\x1b[?25l", 6);

  // [2j is erasing all of the display
  //
  // abAppend(&ab, "\x1b[2J", 4);

  // [H moves the cursor to the top left position
  //
  abAppend(&ab, "\x1b[H", 3);

  // draw rows and move the cursor back to the top 
  // left of the screen
  //
  editorDrawRows(&ab);
  
  // draw the status bar
  //
  editorDrawStatusBar(&ab);

  // draw the message b ar
  //
  editorDrawMessageBar(&ab);

  // initialize buffer length
  //
  char buf[32];

  // write the cursor position escape sequence
  //
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));
  
  // unhide the cursor
  //
  abAppend(&ab, "\x1b[?25h", 6);

  // write out the append buffer stats and free
  // the appended buffer
  //
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);

}

// ... makes a variadic function
// means it can take any number of arguments
//
void editorSetStatusMessage(const char *fmt, ...) {

  // intialize the variable this will be
  // like a %d but take the string
  // we give it
  //
  va_list ap;

  // sets the variable list to the characters
  // in the message
  //
  va_start(ap, fmt);

  // set the status message to the help message in a formatted
  // way where we print he status 
  //
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);

  // cleans up memory associated
  // 
  va_end(ap);

  // get the time
  //
  E.statusmsg_time = time(NULL);
}





void initEditor() {
  
  // set the cursor to the start point
  //
  E.cx = 0;
  E.cy = 0;

  // set the tab variable
  //
  E.rx = 0;

  // set the row offset to the top of the screen
  //
  E.rowoff = 0;

  // set the column offset to the beginning
  // of the row
  //
  E.coloff = 0;
  
  // set the number of rows to zero
  //
  E.numrows = 0;

  // set the array of rows to a null
  //
  E.row = NULL;

  // sett the filename character array to a null
  //
  E.filename = NULL;

  // set the status message a null terminating character
  // and set the time to zero
  //
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;

  // if getting window size fails error
  //
  if (getWindowSize(&E.screenrows, &E.screencols) == -1){
    die("getWindowSize");
  }

  // deal with the last line so no text
  // is displayed on the bottom bar
  // to compensate for the help bar
  //
  E.screenrows -= 2;

}

void editorUpdateRow(erow *row) {

  // integer to keep count for number
  // of tabs
  //
  int tabs = 0;

  // index integer
  //
  int j;

  // iterate through rows and count the number
  // of tabs
  //
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;

  // release the memory from the previous render
  //
  free(row->render);
  
  // allocate memory the size of the row render
  // plus the number of tabs
  //
  row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1);

  // external integer with different scope
  //
  int idx = 0;

  // iterate through the row
  //
  for (j = 0; j < row->size; j++) {

    // If it is a tab
    //
    if (row->chars[j] == '\t') {

      // add spaces up to 8
      //
      row->render[idx++] = ' ';
      while (idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';

    } 
    else {

      // increase the render size
      // and set it equal to the character
      //
      row->render[idx++] = row->chars[j];
    }
  }

  // set the null terminating character
  // at the end of the row
  // and change the size of the render
  //
  row->render[idx] = '\0';
  row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) {
  
  // allocate memory to the correct size of rows
  //
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  // find the last row
  //
  int at = E.numrows;

  // set the size of the row to the length of the string
  //
  E.row[at].size = len;

  // allocate memory while compensating for the null
  // terminating characer
  //
  E.row[at].chars = malloc(len + 1);

  // copy the string into the row
  //
  memcpy(E.row[at].chars, s, len);

  // set the last character to a null terminating character
  E.row[at].chars[len] = '\0';

  // set render information
  //
  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);
  
  // set the number of rows to 0
  //
  E.numrows++;
}

// takes a file as an input parameter
//
void editorOpen(char* filename) {

  // free the current memory of the filename
  // and copy the one trying to be opened
  //
  free(E.filename);
  E.filename = strdup(filename);

  // open the file for reading
  //
  FILE *fp = fopen(filename, "r");
  
  // if file failed to open then
  // die
  //
  if (!fp) {
    die("fopen");
  }
  
  // set a NULL ptr to a character array
  //
  char *line = NULL;
  
  // initialize the line capacity
  // and the length of the line
  //
  size_t linecap = 0;
  ssize_t linelen;

  // readin the line length and the line capacity
  // from the file
  //
  linelen = getline(&line, &linecap, fp);

  /** This is my contribution **/

  // it fixes an error with not printing the first line
  //
  editorAppendRow(line, linelen-1);

  /* End my contribution */

  // if the line isn't empty
  //
  while ((linelen = getline(&line, &linecap, fp)) != -1) {

    // iterate through all the characters in the line
    //
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
      linelen--;
    }

    // write the line
    //
    editorAppendRow(line, linelen);

  }
  // free up the character array holding the line
  // and close the file
  //
  free(line);
  fclose(fp);
  
}

void editorRowInsertChar(erow *row, int at, int c) {

  // checks for line ending and beginnings
  // and compensates by putting the cursor at
  // the end of the row
  //
  if (at < 0 || at > row->size) {
    at = row->size;
  }

  // allocate room for the inserted character
  //
  row->chars = realloc(row->chars, row->size + 2);

  // copy the string of all the characters before with an extra gap
  //
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);

  // increase the size of the row
  //
  row->size++;

  // set the spare gap to a character
  //
  row->chars[at] = c;

  // update the row
  //
  editorUpdateRow(row);
}

void editorInsertChar(int c) {

  // if at the end of the file
  //
  if (E.cy == E.numrows) {

    // append a new blank row
    //
    editorAppendRow("", 0);
  }

  // insert a character into the row
  //
  editorRowInsertChar(&E.row[E.cy], E.cx, c);

  // increase cursor position by 1
  //
  E.cx++;
}


int main(int argc, char *argv[]) {

  // enables byte by byte reading without having to press enter
  //
  enableRawMode();
  
  // initialize editor
  //
  initEditor();

  // open the editor with the appropriate file
  //
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  // set initial status message
  //
  editorSetStatusMessage("HELP: Ctrl-Q = quit");

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