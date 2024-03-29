/*** Next step is step 164 ***/

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
#include <fcntl.h>
#include <stdbool.h>

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

// number of times we have to click quit if there are
// pending modifications
//
#define KILO_QUIT_TIMES 3

// flags to enable highlights
//
#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

// Python filename extensions
//
// char *Python_HL_extensions[] = { ".py", NULL };
// char *Python_HL_keywords[] = {
// "def","str","return","if", "while", "elif", "else", "class", "import", "as",
// "from", "open", "insert", 
// "for|", "list|", "set|", "append|", "add|", "range|", "len|", NULL
// };

// C filename extensions
//
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };
char *C_HL_keywords[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case", "#define",
  "sizeof","#include","atexit","memmove","memcpy","abort","abs","goto","abs",
  "acos","asctime","asctime_r",
  "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
  "void|", NULL
};
// number of database entries
//
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

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

// highlighter enumeration
//
enum editorHighlight {
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH
};

// create a storage object for each row
//
typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
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
// dirty is a integer to keep track of if the file has been edited
// editorSyntax is a pointer to the syntax information
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
  int dirty;
  struct editorSyntax *syntax;
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

// struct to hold filetype
// that will hold the syntax
//
struct editorSyntax {
  char *filetype;
  char **filematch;
  char **keywords;
  char *singleline_comment_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
  int flags;
};

// highlight database
//
struct editorSyntax HLDB[] = {
  {
    "c",
    C_HL_extensions,
    C_HL_keywords,
    "//", "/*", "*/",
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
  },
};

/* Prototypes */

// changing terminal mode
//
void enableRawMode();
void disableRawMode();

// error handling
//
void die(const char *s);

// set bottom bar information
//
void editorSetStatusMessage(const char *fmt, ...);
void editorDrawStatusBar(struct abuf *ab);
void editorDrawRows(struct abuf *ab);
void editorDrawMessageBar(struct abuf *ab);

// append buffer
//
void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);

// text actions
//
void editorUpdateRow(erow *row);
void editorInsertRow(int at, char *s, size_t len);
void editorRowInsertChar(erow *row, int at, int c);
void editorInsertChar(int c);
void editorRowAppendString(erow *row, char *s, size_t len);
int editorRowCxToRx(erow *row, int cx); // sets absolute tab spacing
int editorRowRxToCx(erow *row, int rx); // converts back
void editorfreerow(erow *row);
void editorDelRow(int at);
void editorRowDelChar(erow *row, int at);
void editorDelChar();
void editorInsertNewline();
void editorDeleteRight();

// Syntax Actions
//
void editorSelectSyntaxHighlight();
int is_separator(int c);
void editorUpdateSyntax(erow *row);
int editorSyntaxToColor(int hl);

// cursor actions
//
int getCursorPosition(int *rows, int *cols);
void editorMoveCursor(int key);
void editorFind();
void editorMoveCursorBeginRow();
void editorMoveCursorEndRow();
void editorMoveCursorLeftWord();
void editorMoveCursorRightWord();

// editor initalization and file handling
//
void initEditor();
void editorOpen(char* filename);
void editorSave();
char *editorRowsToString(int *buflen);
char *editorPrompt(char *prompt, void (*callback)(char *, int));

// kepypress actions
//
int editorReadKey();
void editorProcessKeypress();


// viewing terminal actions
//
void editorScroll();
int getWindowSize(int *rows, int *cols);
void editorRefreshScreen();

/* Functions */

/* Changing Terminal Mode */

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

/* End Changing Terminal Mode */



/* Set Bottom Bar Information */

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
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "(modified)" : "");

  // get how manay characters would be needed and write the message
  // into the character array
  //
  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);

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
      
      // set a pointer to the character array
      //
      char *c = &E.row[filerow].render[E.coloff];

      // set a pointer to the syntax array
      //
      unsigned char *hl = &E.row[filerow].hl[E.coloff]; 

      // keep track of current color
      //
      int current_color = -1;

      // index integer
      //
      int j;

      // iterate through the row
      //
      for (j = 0; j < len; j++) {
        
        // if it is a nonprintable character
        // print out an inverted question mark
        if (iscntrl(c[j])) {

          char sym = '?';
          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &sym, 1);
          abAppend(ab, "\x1b[m", 3);

          // return to normal
          //
          if (current_color != -1) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
            abAppend(ab, buf, clen);
          }

        }
        
        // append normal character
        //
        else if (hl[j] == HL_NORMAL) {
          if (current_color != -1) {
            abAppend(ab, "\x1b[39m", 5);
            current_color = -1;
          }
          abAppend(ab, &c[j], 1);
        }
        
        // otherwise make it the appropriate color
        //
        else {
          int color = editorSyntaxToColor(hl[j]);
          if (color != current_color) {
            current_color = color;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
          }
          abAppend(ab, &c[j], 1);
        }
      } 

      // return to normal
      //
      abAppend(ab, "\x1b[39m", 5);
    }

    // deletes part of the line
    //
    abAppend(ab, "\x1b[K", 3);

    // deal with the last line of the terminal
    //
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawMessageBar(struct abuf *ab) {

  // clear message bar
  //
  abAppend(ab, "\x1b[K", 3);

  // find the length of the status message
  //
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

/* End Set Bottom Bar Information */



/* Error Handling */

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

/* End Error Handling */



/* Append Buffer */

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

/* End Append Buffer */



/* Text Actions */

void editorUpdateRow(erow *row) {

  // integer to keep count for number
  // of tabs
  //
  int tabs = 0;

  // index integer
  //
  int j;

  // iterate through row and count the number
  // of tabs
  //
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      tabs++;
    }
  }
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
      while (idx % KILO_TAB_STOP != 0) {
        row->render[idx++] = ' ';
      }
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

  // update syntax highlighting
  //
  editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
  
  // check to see if it is a valid row
  //
  if (at < 0 || at > E.numrows) {
    return;
  } 

  // allocate memory to the row
  //
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  // move the memory of the row to be inserted
  //
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

  // update the index of each row
  for (int j = at + 1; j <= E.numrows; j++) {
    E.row[j].idx++;
  }

  // have the current row index update
  //
  E.row[at].idx = at;

  // allocate memory to the correct size of rows
  //
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

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
  // and highlight information
  // set default comment info
  //
  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editorUpdateRow(&E.row[at]);
  
  // set the number of rows to 0
  //
  E.numrows++;

  // modification tracking
  //
  E.dirty++;
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


  // modification tracking
  //
  E.dirty++;
}

void editorInsertChar(int c) {

  // if at the end of the file
  //
  if (E.cy == E.numrows) {

    // append a new blank row
    //
    editorInsertRow(E.numrows, "", 0);
  }

  // insert a character into the row
  //
  editorRowInsertChar(&E.row[E.cy], E.cx, c);

  // increase cursor position by 1
  //
  E.cx++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {

  // add memory equivalent to the amount of data needed
  // to be added to the row
  //
  row->chars = realloc(row->chars, row->size + len + 1);

  // copy the string to the end of the row
  //
  memcpy(&row->chars[row->size], s, len);

  // increase the size of the row
  //
  row->size += len;

  // add a null terminating character
  //
  row->chars[row->size] = '\0';

  // update the row
  //
  editorUpdateRow(row);

  // increment the modification counter
  //
  E.dirty++;
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

int editorRowRxToCx(erow *row, int rx) {
  
  // keep track of absolute cursor position
  // and relative cursor position
  //
  int cur_rx = 0;
  int cx;

  // iterate through the row
  //
  for (cx = 0; cx < row->size; cx++) {

    // if tab is found convert from absolute spacing
    // to relative spacing
    //
    if (row->chars[cx] == '\t')
      cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);
    cur_rx++;

    if (cur_rx > rx) {
      return cx;
    }
  }
  return cx;
}

// free up the memory of the row
//
void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editorDelRow(int at) {

  // check to see if valid row to delete
  //
  if (at < 0 || at >= E.numrows) {
    return;
  }

  // Free the memory of the row
  //
  editorFreeRow(&E.row[at]);

  // move the memory of the rows after
  // to fill the new blank space
  //
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));

  // update the index of each row after one is deleted
  //
  for (int j = at; j < E.numrows - 1; j++) {
    E.row[j].idx--;
  }

  // decrement the number of rows
  // and incriment the modification counter
  //
  E.numrows--;
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {

  // Check if valid index size
  //
  if (at < 0 || at >= row->size) {
    return;
  }

  // Perform the delete
  //
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);

  // Decrement Row Size
  //
  row->size--;
  
  // update the row
  //
  editorUpdateRow(row);
  
  // increment modification coutner
  //
  E.dirty++;

}

void editorDelChar() {

  // check to see if at the end of the file
  //
  if ((E.cy >= E.numrows) || (E.cx == 0 && E.cy == 0)){
    return;
  }


  // index the row
  //
  erow *row = &E.row[E.cy];

  // Make sure that the cursor is within the row
  //
  if (E.cx > 0) {

    // delete the character to the left
    //
    editorRowDelChar(row, E.cx - 1);

    // decrement the row
    //
    E.cx--;
  }

  else {

    // set cursor position to the end of the previous line
    //
    E.cx = E.row[E.cy - 1].size;

    // Append the row below to the current row
    //
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);

    // Delete the row below
    //
    editorDelRow(E.cy);
    
    // decrement the cursor y position
    //
    E.cy--;
  }

}

void editorInsertNewline() {

  // if at the beginning of a line
  // insert a blank line
  //
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } 
  else {
    
    // index the row
    //
    erow *row = &E.row[E.cy];

    // insert a row below the current one with the characters 
    // after the cursor at the current line
    //
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);

    // index the previous row
    //
    row = &E.row[E.cy];

    // update its size
    // and add a null terminating character
    //
    row->size = E.cx;
    row->chars[row->size] = '\0';

    // update the row
    //
    editorUpdateRow(row);
  }

  // increase the cursor position and set it to the beginning
  // of the next line
  //
  E.cy++;
  E.cx = 0;
}

void editorDeleteRight() {

  // index the row
  //
  erow *row = &E.row[E.cy];

  // check if empty row
  //
  if (row->size == 0) {
    editorDelRow(E.cy);
    return;
  }

  // Check if valid index size
  //
  if (E.cx < 0 || E.cx >= row->size || row->size == 0) {
    return;
  }

  // Perform the delete
  //
  memmove(&row->chars[E.cx], &row->chars[row->size], 1);

  // Decrement Row Size
  //
  row->size-=row->size - E.cx;
  
  // set the cursor position
  //
  E.cx = row->size;

  // update the row
  //
    editorUpdateRow(row);
  
  // increment modification coutner
  //
  E.dirty++;
}

/* End Text Actions */

/* Syntax Actions */

void editorSelectSyntaxHighlight() {

  // reset syntax of row
  //
  E.syntax = NULL;

  // if no filename
  // exit
  //
  if (E.filename == NULL) {
    return;
  }

  // find the extension
  //
  char *ext = strrchr(E.filename, '.');

  // iterate through the highlate database entries
  //
  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {

    // index the highlight database
    //
    struct editorSyntax *s = &HLDB[j];

    // index variable
    //
    unsigned int i = 0;

    // iterate through check for file extensions
    //
    while (s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');

      // if there is an extension check to see if it matches
      // otherwise check if the extension is in the filename
      // 
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) || (!is_ext && strstr(E.filename, s->filematch[i]))) {
        
        // set the correct syntax database
        //
        E.syntax = s;

        // loop through the file rows and update the syntax for
        // each row
        //
        int filerow;
        for (filerow = 0; filerow < E.numrows; filerow++) {
          editorUpdateSyntax(&E.row[filerow]);
        }

        return;
      }

      // increment the variable
      //
      i++;
    }
  }
}

int is_separator(int c) {

  // check if it is a separator 
  //
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row) {

  // allocate memory for the row highlights
  //
  row->hl = realloc(row->hl, row->rsize);

  // set the whole row to normal
  //
  memset(row->hl, HL_NORMAL, row->rsize);

  // if there is no syntax in the row
  // return
  //
  if (E.syntax == NULL) {
   return;
  }

  // load in keywords
  //
  char **keywords = E.syntax->keywords;

  // keep track of single line comment start
  //
  char *scs = E.syntax->singleline_comment_start;

  // multiline comments start and end
  //
  char *mcs = E.syntax->multiline_comment_start;
  char *mce = E.syntax->multiline_comment_end;

  // length of single line comment
  //
  int scs_len = scs ? strlen(scs) : 0;
  
  // length of multiline comments
  //
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;

  // keep track of separator characters
  //
  int prev_sep = 1;
  
  // keep track of if we are in a string
  //
  int in_string = 0;
  
  // keep track of if we are in a ml comment
  //
  int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

  // index variable
  //
  int i = 0;

  // iterate through the whole row
  //
  while (i < row->rsize) {

    // load in the specific character
    //
    char c = row->render[i];
    
    // previous character
    //
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    // if this is a single line comment
    //
    if (scs_len && !in_string && !in_comment) {

      // if thi is not the beginning of the row
      //
      if (!strncmp(&row->render[i], scs, scs_len)) {
        memset(&row->hl[i], HL_COMMENT, row->rsize - i);
        break;
      }
    }

    // if all proper flags are one and we aren't in a string
    //
    if (mcs_len && mce_len && !in_string) {

      // if we are within a comment
      //
      if (in_comment) {

        // set the character to a multiline comment
        row->hl[i] = HL_MLCOMMENT;

        // check if at end of multiline comment
        //
        if (!strncmp(&row->render[i], mce, mce_len)) {

          // if not highlight the whole comment 
          //
          memset(&row->hl[i], HL_MLCOMMENT, mce_len);
          i += mce_len;
          in_comment = 0;
          prev_sep = 1;
          continue;
        } 
        
        else {
          i++;
          continue;
        }
      }

      // check if we are at the beginning of a comment
      // 
      else if (!strncmp(&row->render[i], mcs, mcs_len)) {

        // if so set the comment to the appropriate color
        //
        memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
        i += mcs_len;
        in_comment = 1;
        continue;
      }
    }

    // if the syntax flag is on highlihgt strings
    //
    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {

      // if currently in a string
      //
      if (in_string) {

        // highlight the character
        //
        row->hl[i] = HL_STRING;

        // if character is the closing quote
        // note exit string
        //
        if (c == in_string) {
          in_string = 0;
        }

        // increment
        //
        i++;

        // note that this is a separation character
        prev_sep = 1;
        continue;
      } 

      // if is single or double quote
      //
      else {
        if (c == '"' || c == '\'') {

          // set the end quote
          //
          in_string = c;

          // highlihgt as part of string
          //
          row->hl[i] = HL_STRING;

          // incremenet
          //
          i++;
          continue;
        }
      }
    }
    
    // if the syntax flag is on and highlighting numbers flag is on
    //
    if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {

      // if it is a number/. and the previous HL is a number, separator, period
      //
      if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER)) {

        // make the current character a number
        //
        row->hl[i] = HL_NUMBER;

        // incriment i
        //
        i++;

        // reset separator
        //
        prev_sep = 0;
        continue;
      }
    }


    // if prevous word was a separator
    //
    if (prev_sep) {

      // index variable
      //
      int j;

      // iterate through keywords
      //
      for (j = 0; keywords[j]; j++) {

        // find the length of the keyword
        //
        int klen = strlen(keywords[j]);

        // check if extraneous character
        //
        int kw2 = keywords[j][klen - 1] == '|';

        // if there is an extraneous char
        // decrement length of keyword
        //
        if (kw2) {
          klen--;
        }

        // check if the keyword is at the current position and that it is followed
        // by a separator character
        //
        if (!strncmp(&row->render[i], keywords[j], klen) && is_separator(row->render[i + klen])) {

          // set the memory of the highlight appropriately
          //
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          
          // increment by the keyword length
          //
          i += klen;
          break;
        }
      }

      // check to see if it reached end of array
      //
      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue;
      }
    }

    // check if the chracter is a separator
    //
    prev_sep = is_separator(c);
    
    // incriment
    //
    i++;
  }

  // highlighting of next line won't change if
  // not in a comment
  //
  int changed = (row->hl_open_comment != in_comment);
  
  // set open comment to current state of comment
  //
  row->hl_open_comment = in_comment;

  // if it is changed and within the file
  // then update syntax of the row after
  if (changed && row->idx + 1 < E.numrows) {
    editorUpdateSyntax(&E.row[row->idx + 1]);
  }
}
int editorSyntaxToColor(int hl) {

  // switch case with the numbers correlating
  // to the appropriate colors
  //
  switch (hl) {
    case HL_COMMENT:
    case HL_MLCOMMENT: return 36;
    case HL_KEYWORD1: return 33;
    case HL_KEYWORD2: return 32;
    case HL_STRING: return 35;
    case HL_NUMBER: return 31;
    case HL_MATCH: return 34;
    default: return 37;
  }
}

/* End Syntax Actions */

/* Cursor Actions */

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
      if (E.cy < E.numrows-1) {
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

void editorFindCallback(char *query, int key) {
  
  static char *match=NULL;

  // direction to search
  //
  int direction = 1;

  // current line
  //
  int check_line = E.cy;
  int i = check_line;
  erow *row = &E.row[i];
  
  

  if(match) {
    editorUpdateRow(row);
  }

  //if enter or escape return
  //
  if (key == '\x1b') {
    // reset
    //
    direction = 1;
    return;
  }
  // set search forwards
  //
  else if (key == ARROW_DOWN) {
    direction = 1;
  } 
  // set search backwards
  //
  else if (key == ARROW_UP) {
    direction = -1;
  }
  else {
    goto label;
  }

  
  
  if(match!=NULL && query && row->render) {
    char *match2 = strstr(match+1,query);
    if(match2 != NULL) {
      E.cy = i;
      E.cx = match2 - row->render;
      E.cx =  editorRowRxToCx(row, E.cx);
      match = match2;
      check_line = i;
      goto label;
    }
    i+=direction;
  }

  while(1) {
    if(i >= E.numrows) {
      i = 0;
    }
    if(i == -1) {
      i = E.numrows - 1;
    }

    row = &E.row[i];
    if(query && row->render) {
      match = strstr(row->render,query);
    }
    if(match) {
      E.cy = i;
      E.cx = match - row->render;
      E.cx =  editorRowRxToCx(row, E.cx);
      check_line = i;
      goto label;
    }
    i+=direction;
    

  }
  label:
  if(match!=NULL) {
    memset(&row->hl[E.cx],HL_MATCH,strlen(query));
  }
}

void editorFind() {

  // save current cursor position
  //
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  // prompt the user
  //
  char *query = editorPrompt("Search: %s (ESC to cancel) (UP/DOWN to search)", editorFindCallback);

  // manage memory
  //
  if (query) {
    free(query);
  }

  // // if query is null then restore cursor position
  // //
  // else {
  //   E.cx = saved_cx;
  //   E.cy = saved_cy;
  //   E.coloff = saved_coloff;
  //   E.rowoff = saved_rowoff;
  // }

}

void editorMoveCursorBeginRow() {
  E.cx = 0;
}

void editorMoveCursorEndRow() {
  erow *row = &E.row[E.cy];
  E.cx = row->size;
}

void editorMoveCursorLeftWord() {

  erow *row = &E.row[E.cy];
  char* curr = row->chars;

  if (E.cx == 0) {
    if (E.cy == 0) {
      return;
    }
    else {
      E.cy--;
      row = &E.row[E.cy];
      E.cx = row->size;
      return;
    }
  }
  if (curr[E.cx] == ' ' || curr[E.cx] == '\t') {
    if (curr[E.cx-1] != ' ' && curr[E.cx-1] != '\t'){
      E.cx--;
    }
    else {
      while (curr[E.cx] == ' ' || curr[E.cx] == '\t') {
        if (E.cx == 0){
          return;
        }
        E.cx--;
      }
      E.cx++;
      return;
    }
  }
  
  if (curr[E.cx] != ' ' && curr[E.cx] != '\t') {
    if (curr[E.cx-1] == ' ' || curr[E.cx-1] == '\t'){
        while (curr[E.cx] == ' ' || curr[E.cx] == '\t') {
          if (E.cx == 0){
            return;
          }
          E.cx--;
        }
        if (E.cx == 0){
            return;
        }
        E.cx--;
        return;
      }
    else{
      while (curr[E.cx] != ' ' && curr[E.cx] != '\t') {
        if (E.cx == 0) {
          return;
        }
        E.cx--;
      }
      E.cx++;
      return;
    }
  }
}

void editorMoveCursorRightWord() {
  erow *row = &E.row[E.cy];
  char* curr = row->chars;

  if (E.cx == row->size) {
    if (E.cy == E.numrows) {
      return;
    }
    else {
      E.cy++;
      row = &E.row[E.cy];
      E.cx = 0;
      return;
    }
  }

  if (curr[E.cx] == ' ' || curr[E.cx] == '\t') {
    if (curr[E.cx+1] != ' ' && curr[E.cx+1] != '\t'){
      E.cx++;
    }
    else {
      while (curr[E.cx] == ' ' || curr[E.cx] == '\t') {
        if (E.cx == row->size){
          return;
        }
        E.cx++;
      }
      return;
    }
  }

  if (curr[E.cx] != ' ' && curr[E.cx] != '\t') {
    if (curr[E.cx+1] == ' ' || curr[E.cx+1] == '\t'){
        while (curr[E.cx] == ' ' || curr[E.cx] == '\t') {
          if (E.cx == row->size){
            return;
          }
          E.cx++;
        }
        return;
      }
    else{
      while (curr[E.cx] != ' ' && curr[E.cx] != '\t') {
        if (E.cx == row->size) {
          return;
        }
        E.cx++;
      }
      return;
    }
  }
}

/* End Cursor Actions*/



/* Editor Initialization and File Handline */

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
  
  // set the file's default to unedited
  //
  E.dirty = 0;

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


  // set the default filetype to none
  //
  E.syntax = NULL;


}

void editorOpen(char* filename) {

  // free the current memory of the filename
  // and copy the one trying to be opened
  //
  free(E.filename);
  E.filename = strdup(filename);

  editorSelectSyntaxHighlight();

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
  editorInsertRow(E.numrows, line, linelen-1);

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
    editorInsertRow(E.numrows,line, linelen);

  }
  // free up the character array holding the line
  // and close the file
  //
  free(line);
  fclose(fp);

  // reset the modification counter
  //
  E.dirty = 0;
  
}

void editorSave() {

  // if there is no filename break
  // prompt the user for a file name
  //
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
  }

  // integer for keeping track of the length of the file
  //
  int len;

  // get the pointer to the beginning of the string
  //
  char *buf = editorRowsToString(&len);

  // O_RDWR = open for reading and writing
  // O_CREAT = create a file if it doesn't exist 
  // 0644 is the standard permissions to save files
  //
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {

    // sets file size
    //
    if (ftruncate(fd, len) != -1) {

      // writes to the file
      //
      if (write(fd, buf, len) == len) {  
  
        // closes the file
        //
        close(fd);

        // frees the memory storing the string
        //
        free(buf);
        
        // reset modification counter
        // and set saved messaged
        //
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);

        return;
      }
      editorSelectSyntaxHighlight();
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

char *editorRowsToString (int *buflen) {

  // total length of the string
  //
  int totlen = 0;

  // index integer
  //
  int j;
  for (j = 0; j < E.numrows; j++)

    // set the total length to the size of the row + 
    // the new line character
    //
    totlen += E.row[j].size + 1;

  // set the length to include the new line character
  //
  *buflen = totlen;

  // allocate memory equal to the length of the desired string
  //
  char *buf = malloc(totlen);

  // declare pointer to the return string
  //
  char *p = buf;

  // iterate through each row
  //
  for (j = 0; j < E.numrows; j++) {

    // copy the row into the poitner
    //
    memcpy(p, E.row[j].chars, E.row[j].size);

    // set the pointer to the end of the string
    //
    p += E.row[j].size;

    // add a new line character to the end of the string and iterate past
    //
    *p = '\n';
    p++;
  }

  // return the pointer to the beginning of the string
  //
  return buf;
}

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {

  // set size of the buffer
  //
  size_t bufsize = 128;
  
  // allocate appropriat memory
  //
  char *buf = malloc(bufsize);
  
  // set the length of the buffer
  //
  size_t buflen = 0;
  
  // set the beginning of the buffer to be a null
  // terminating character
  //
  buf[0] = '\0';

  // loop until proper key is read
  //
  while (1) {

    // set message at bottom bar
    //
    editorSetStatusMessage(prompt, buf);

    // update viewing screen
    //
    editorRefreshScreen();
    
    // read key presses
    //
    int c = editorReadKey();

    // allow user to backspace
    //
    if (c == BACKSPACE) {
      if (buflen != 0) {
         buf[--buflen] = '\0';
      }
    }

    // if escape is pressed
    // cancel the prompt
    //
    else if (c == '\x1b') {
      editorSetStatusMessage("");


      if (callback) {
        callback(buf, c);
      }
      free(buf);
      return NULL;
    }

    // if character press is enter
    //
    else if (c == '\r') {

      // confirm their input is not empty
      // and return
      //
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback) {
          callback(buf, c);
        }
        return buf;
      }
    } 
    // otherwise if they're not trying to cancel
    // or select another special character
    //
    else if (!iscntrl(c) && c < 128) {

      // if the bufflen is full
      // increase the size by double
      //
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }

      // add the character to the buf
      // and also add a null terminating character
      //
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
    if (callback) {
      callback(buf, c);
    }
  }
}

/* End Editor Initialization and File Handline */



/* Keypress Actions */

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

void editorProcessKeypress() {

  // number of times it takes to quit without
  // saving
  //
  static int quit_times = KILO_QUIT_TIMES;

  // read a single byte
  //
  int c = editorReadKey();
  int work;

  // printf("%d ",c);
  // handle error checking
  //
  switch (c) {

    case '\r':
      editorInsertNewline();  
      break;

    // if ctrl+q pressed then break
    //
    case CTRL_KEY('q'):
      
      // check to see if the file is modified and if quit times is zero
      //
       if (E.dirty && quit_times > 0) {

        // warn the user about unsaved changes
        //
        editorSetStatusMessage("WARNING!!! File has unsaved changes. Press Ctrl-Q %d more times to quit.", quit_times);

        // track the amount of times the quit button has been clicked
        //
        quit_times--;
        return;
      }

      // clear the screen and exit the program
      //
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;  
    
    case CTRL_KEY('a'):
      editorMoveCursorBeginRow();
      break;

    case CTRL_KEY('e'):
      editorMoveCursorEndRow();
      break;

    case CTRL_KEY('k'):
      editorDeleteRight();
      break;

    case HOME_KEY:
      break;
    case END_KEY:
      break;

    case BACKSPACE:
    case DEL_KEY:
      if (c == DEL_KEY) {
        editorMoveCursor(ARROW_RIGHT);
      }
      editorDelChar();
      break;

    case CTRL_KEY('s'):
      editorSave();
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
      work = editorReadKey();

      if (work == 53) {
        work = editorReadKey();

        if (work == 68) {
          editorMoveCursorLeftWord();
        }
        else if (work == 67) {
          editorMoveCursorRightWord();
        }
      }
      break;

    case CTRL_KEY('f'):
      editorFind();
      break;

    default:
      editorInsertChar(c);
      break;
  }

// reset quit times number
//
quit_times = KILO_QUIT_TIMES;
}



/* End Keypress Actions */ 



/* Viewing Terminal Actions */

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

/* End Terminal Viewing Actions*/



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
  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-f = find");

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
