/*
  maeFat Rev 2.1
  First Version Feb. 5, 2011

  Copyright (C) (2011) Ken Young orrery.moko@gmail.com

  This program is free software; you can redistribute it and/or 
  modify it under the terms of the GNU General Public License 
  as published by the Free Software Foundation; either 
  version 2 of the License, or (at your option) any later 
  version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  ------------------------------------------------------------------

  This program is a tool for personal weight reduction/management.
  It is similar to the Hacker Diet tools, although it is not a clone
  of that package.   It lacks the features for tracking exercize
  levels which are present in the Hacker Diet tools, but adds other
  features such as the ability to predict when a weightloss goal
  will be reached.   It is optimized to make daily entry of weight
  measurements as quick as possible, and requires no network
  access.

 */

#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <locale.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <pango/pangocairo.h>
#include <gtk/gtk.h>
#include <dbus/dbus.h>
#include <hildon-1/hildon/hildon.h>
#include <hildon-fm-2/hildon/hildon-file-chooser-dialog.h>
#include <libosso.h>
#include "maeFatColors.h"

#define FALSE       (0)
#define ERROR_EXIT (-1)
#define OK_EXIT     (0)

#define DATA_CELL   (0)
#define RIGHT_ARROW (1)
#define LEFT_ARROW  (2)

#define M_HALF_PI (M_PI * 0.5)

#define KG_PER_LB (0.45359237)
#define M_PER_IN (2.54e-2)
#define CALORIES_PER_POUND (3500.0) /* Need to propagate this */

#define RENDER_CENTER_X (80)
#define RENDER_CENTER_Y (400)
#define INT_TOKEN     0
#define DOUBLE_TOKEN  1
#define FLOAT_TOKEN   2
#define STRING_TOKEN  3
#define BORDER_FACTOR_X (0.015)
#define BORDER_FACTOR_Y (0.015)
#define LEFT_BORDER   (33)
#define RIGHT_BORDER  (40)
#define TOP_BORDER    (21)
#define BOTTOM_BORDER (46)

#define DO_NOT_FIT_DATA (0)
#define FIT_WINDOW      (1)
#define FIT_FROM_DATE   (2)

#define ONE_WEEK         (7)
#define ONE_FORTNIGHT   (14)
#define ONE_MONTH       (30)
#define ONE_QUARTER     (91)
#define ONE_SIX_MONTHS (183)
#define ONE_YEAR       (365)

unsigned char normalYear[12] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
unsigned char leapYear[2] = {6, 2};
char *homeDir, *userDir, *fileName, *settingsFileName;
char *backupDir = "/media/mmc1/.maeFat";
char *backupFile = "/media/mmc1/.maeFat/data";
char *defaultComment = "Type Comment Here (optional)";
char *monthName[12] = {"January",   "February", "March",    "April",
		       "May",       "June",     "July",     "August",
		       "September", "October",  "November", "December"};
char *dayName[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
char *maeFatVersion = "2.0";
char scratchString[200];

int debugMessagesOn = FALSE;
#define dprintf if (debugMessagesOn) printf
int nDataPoints = 0;
int goodColor = OR_GREEN;
int badColor  = OR_RED;
int displayHeight, displayWidth, accepted, deleted;
int minSelectorWeight = 50;
int maxSelectorWeight = 300;
int mainBoxInTrendStackable = FALSE;

double monthLengths[12] = {31.0, 28.0, 31.0, 30.0, 31.0, 30.0, 31.0, 31.0, 30.0, 31.0, 30.0, 31.0};
double maxJD = -1.0;
double minJD = 1.0e30;
double maxWeight = -1.0;
double minWeight = 1.0e30;
double plotMaxX, plotMinX, plotMaxY, plotMinY;

typedef struct logEntry {
  double time;           /* The time at which the log entry was made */
  float weight;          /* The reported weight                      */
  char *comment;         /* User-entered comment text                */
  struct logEntry *next; /* Pointer to next entry                    */
  struct logEntry *last; /* Pointer to last entry                    */
} logEntry;

logEntry *logRoot = NULL;
logEntry *lastEntry = NULL;
logEntry *editPtr;

/*
  logCell structures hold information about the weight entries shown
  on the "Log" page.   They are used to implement the edit callback
  capability.
 */
typedef struct logCell {
  GdkPoint        box[4]; /* Coordinates of the little box drawn around this entry */
  logEntry       *ptr;    /* This points to the log entry displayed, which may be edited */
  int             data;   /* Used to distinguish cell types */
  struct logCell *next;
} logCell;
logCell *logCellRoot = NULL;
logCell *lastLogCell = NULL;
int logDisplayed = FALSE;
int logPageOffset = 0;

/* Values which are stored in the settings file */
double myHeight          =            72.0;
double myTarget          =           150.0;
int weightkg             =           FALSE;
int heightcm             =           FALSE;
int monthFirst           =               0;
int nonjudgementalColors =           FALSE;
int hackerDietMode       =           FALSE;
int showComments         =           FALSE;
int showTarget           =           FALSE;
int plotInterval         =        ONE_YEAR;
int aveInterval          =        ONE_WEEK;
int fitInterval          =              -1;
int fitType              = DO_NOT_FIT_DATA;
int losingWeightIsGood   =            TRUE;
int startWithDataEntry   =           FALSE;
/* int interpolateFirstPair =           FALSE; */
int plotTrendLine        =            TRUE;
int plotDataPoints       =            TRUE;
int plotWindow           =            TRUE;
int fitDay               =              -1;
int fitMonth             =              -1;
int fitYear              =              -1;
int plotStartDay         =              -1;
int plotStartMonth       =              -1;
int plotStartYear        =              -1;
int plotEndDay           =              -1;
int plotEndMonth         =              -1;
int plotEndYear          =              -1;
int plotFromFirst        =           FALSE;
int plotToLast           =           FALSE;
int haveGoalDate         =           FALSE;
int targetDay            =              -1;
int targetMonth          =              -1;
int targetYear           =              -1;
/* End of settings file variables */

GdkGC *gC[N_COLORS];
GdkPixmap *pixmap = NULL;
GdkPixmap *cairoPixmap = NULL;

/* Cairo and Pango related globals */

#define N_PANGO_FONTS (4)
#define SMALL_PANGO_FONT       (0)
#define MEDIUM_PANGO_FONT      (1)
#define BIG_PANGO_FONT         (2)
#define SMALL_MONO_PANGO_FONT  (3)
#define BIG_PANGO_FONT_NAME         "Sans Bold 32"
#define MEDIUM_PANGO_FONT_NAME      "Sans Normal 18"
#define SMALL_PANGO_FONT_NAME       "Sans Normal 14"
#define SMALL_MONO_PANGO_FONT_NAME  "Monospace Bold 16"

cairo_t *cairoContext = NULL;

GtkWidget *window, *mainBox, *drawingArea, *weightButton, *dataEntryAccept, *dataEditDelete,
  *dataEntryButton, *logButton, *trendButton, *aboutYouButton, *monthAccept, *iEButton,
  *helpButton, *aboutButton, *commentText, *logSelectorButton, *iEFileButton, *fitFromButton,
  *selector, *dataEntryStackable, *aboutYouStackable, *logStackable, *iESeparator,
  *dateButton, *timeButton, *trendStackable, *dateEditButton, *timeEditButton, *fitDateButton,
  *dataEditStackable, *heightSpin, *weightUnitLabel, *heightUnitLabel, *fitWindowButton,
  *iSOButton, *poundsButton, *kgButton, *inButton, *cmButton, *dayButton, *sWDEButton,
  *monthButton, *nonjudgementalButton, *hackerDietButton, *showCommentsButton, *plotTrendLineButton,
  *plotFortnightButton, *plotMonthButton, *plotQuarterButton, *plot6MonthButton, *plotDataPointsButton,
  *plotYearButton, *plotHistoryButton, *targetSpinLabel, *targetSpin, *showTargetButton,
  *fitNothingButton, *fitMonthButton, *fitQuarterButton, *fit6MonthButton, *iEStackable,
  *fitYearButton, *fitHistoryButton, *losingWeightIsGoodButton, *gainingWeightIsGoodButton,
  *goalLabel, *iECommaButton, *iEWhitespaceButton, *iEFieldLabel[4], *iEWeightButton[4],
  *iEDayButton[4], *iETimeButton[4], *iECommentButton[4], *iESkipButton[4], *iENoneButton[4],
  *iEFileLabel, *iEImportButton, *iEExportButton, *iEHackerDietFormatButton, *iECustomFormatButton,
  *plotSettingsButton, *fitSettingsButton, *plotSettingsStackable, *fitSettingsStackable,
  *setGoalDateButton, *goalDateButton, *plotWindowButton, *plotFromButton, *plotStartButton,
  *plotEndButton, *plotFromFirstButton, *plotToLastButton, *averageIntervalLabel, *aveWeekButton,
  *aveFortnightButton, *aveMonthButton, *aveQuarterButton, *aveYearButton, *aveHistoryButton
  /* , *interpolateFirstPairButton */;

/* Variables used for setting the format of import/export files */
#define IE_FIELD_NOT_USED   (0)
#define IE_FIELD_IS_DATE    (1)
#define IE_FIELD_IS_TIME    (2)
#define IE_FIELD_IS_WEIGHT  (3)
#define IE_FIELD_IS_COMMENT (4)
#define IE_FIELD_IS_SKIPPED (5)
#define IE_DEFAULT_PATH "/home/user/"
#define IE_DEFAULT_FILE "weightLog"
#define IE_DEFAULT_HD_FILE "hackdiet_db.csv"
int iECommasAreSeparators = TRUE;
int iEFieldUse[4] = {IE_FIELD_IS_DATE, IE_FIELD_IS_TIME, IE_FIELD_IS_WEIGHT, IE_FIELD_IS_COMMENT};
char iEFileName[256];
int iEHackerDietFormat = TRUE;

/*   E N D   O F   V A R I A B L E   D E C L A R A T I O N S   */

void dataEntryButtonClicked(GtkButton *button, gpointer userData);

/*
  C R E A T E  P A N G O  C A I R O  W O R K S P A C E

  Create the structures needed to be able to yse Pango and Cairo to
render antialiased fonts
*/
void createPangoCairoWorkspace(void)
{
  cairoContext = gdk_cairo_create(cairoPixmap);
  if (cairoContext == NULL) {
    fprintf(stderr, "cairo_create returned a NULL pointer\n");
    exit(ERROR_EXIT);
  }
  cairo_translate(cairoContext, RENDER_CENTER_X, RENDER_CENTER_Y);
} /* End of  C R E A T E  P A N G O  C A I R O  W O R K S P A C E */

/*
  R E N D E R  P A N G O  T E X T

  The following function renders a text string using Pango and Cairo, on the
  cairo screen, and copies it to a gtk drawable.   The height and width
  of the area occupied by the text is returned in passed variables.
*/
void renderPangoText(char *theText, unsigned short color, int font,
		     int *width, int *height, GdkDrawable *dest,
		     int x, int y, float angle, int center, int topClip,
		     int background)
{
  static int firstCall = TRUE;
  static PangoFontMap *fontmap = NULL;
  static char *fontName[N_PANGO_FONTS] = {SMALL_PANGO_FONT_NAME, MEDIUM_PANGO_FONT_NAME,
					  BIG_PANGO_FONT_NAME, SMALL_MONO_PANGO_FONT_NAME};
  static PangoContext *pangoContext[N_PANGO_FONTS];
  static cairo_font_options_t *options = NULL;
  static int xC, yC, wC, hC = 0;
  static float currentAngle = 0.0;

  float deltaAngle, cA, sA, fW, fH;
  int pWidth, pHeight, iFont;
  PangoFontDescription *fontDescription;
  PangoLayout *layout;

  if (angle < 0.0)
    angle += M_PI * 2.0;
  else if (angle > M_PI * 2.0)
    angle -= M_PI * 2.0;
  if ((angle > M_HALF_PI) && (angle < M_HALF_PI + M_PI))
    angle -= M_PI;
  if (angle < 0.0)
    angle += M_PI * 2.0;
  if (firstCall) {
    /* Initialize fonts, etc. */
    createPangoCairoWorkspace();
    fontmap = pango_cairo_font_map_get_default();
    if (fontmap == NULL) {
      fprintf(stderr, "pango_cairo_font_map_get_default() returned a NULL pointer\n");
      exit(ERROR_EXIT);
    }
    options = cairo_font_options_create();
    if (options == NULL) {
      fprintf(stderr, "cairo_font_options_create() returned a NULL pointer\n");
      exit(ERROR_EXIT);
    }
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_FULL);
    cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_ON);
    cairo_font_options_set_subpixel_order(options, CAIRO_SUBPIXEL_ORDER_BGR);
    for (iFont = 0; iFont < N_PANGO_FONTS; iFont++) {
      fontDescription = pango_font_description_new();
      if (fontDescription == NULL) {
	fprintf(stderr, "pango_font_description_new() returned a NULL pointer, font = %d\n", iFont);
	exit(ERROR_EXIT);
      }
      pango_font_description_set_family(fontDescription, (const char*) fontName[iFont]);
      pangoContext[iFont] = pango_cairo_font_map_create_context(PANGO_CAIRO_FONT_MAP(fontmap));
      if (pangoContext[iFont] == NULL) {
	fprintf(stderr, "pango_cairo_font_map_create_context(iFont = %d) returned a NULL pointer\n", iFont);
	exit(ERROR_EXIT);
      }
      pango_context_set_font_description(pangoContext[iFont], fontDescription);
      pango_font_description_free(fontDescription);
      pango_cairo_context_set_font_options(pangoContext[iFont], options);
    }
    firstCall = FALSE;
  } else
    if (font == MEDIUM_PANGO_FONT)
      gdk_draw_rectangle(cairoPixmap, gC[background], TRUE, RENDER_CENTER_X+xC, RENDER_CENTER_Y+yC,
			 wC, hC+7);
    else if (font == BIG_PANGO_FONT)
      gdk_draw_rectangle(cairoPixmap, gC[background], TRUE, RENDER_CENTER_X+xC, RENDER_CENTER_Y+yC,
			 wC, hC+30);
    else
      gdk_draw_rectangle(cairoPixmap, gC[background], TRUE, RENDER_CENTER_X+xC, RENDER_CENTER_Y+yC,
			 wC, hC);
  layout = pango_layout_new(pangoContext[font]);
  if (layout == NULL) {
    fprintf(stderr, "pango_layout_new() returned a NULL pointer\n");
      exit(ERROR_EXIT);
  }
  pango_layout_set_text(layout, theText, -1);
  fontDescription = pango_font_description_from_string(fontName[font]);
  pango_layout_set_font_description (layout, fontDescription);
  pango_font_description_free(fontDescription);
  cairo_set_source_rgb(cairoContext,
		       ((double)orreryColorRGB[color][0])/DOUBLE_MAX16,
		       ((double)orreryColorRGB[color][1])/DOUBLE_MAX16,
			((double)orreryColorRGB[color][2])/DOUBLE_MAX16);
  deltaAngle = angle - currentAngle;
  cairo_rotate(cairoContext, deltaAngle);
  currentAngle = angle;
  pango_cairo_update_layout(cairoContext, layout);
  pango_layout_get_size(layout, &pWidth, &pHeight);
  fW = (float)pWidth/(float)PANGO_SCALE;
  fH = (float)pHeight/(float)PANGO_SCALE;
  *width = (int)(fW+0.5);
  *height = (int)(fH+0.5);
  cairo_move_to(cairoContext, 0.0, 0.0);
  pango_cairo_show_layout(cairoContext, layout);
  g_object_unref(layout);
  cA = cosf(angle); sA = sinf(angle);
  wC = (int)((fW*fabs(cA) + fH*fabs(sA)) + 0.5);
  hC = (int)((fW*fabs(sA) + fH*fabs(cA)) + 0.5);
  if (angle < M_HALF_PI) {
    xC = (int)((-fH*sA) + 0.5);
    yC = 0;
  } else {
    xC = 0;
    yC = (int)((fW*sA) + 0.5);
  }
  if (dest != NULL) {
    if (center) {
      int xM, yM, ys, yd, h;

      xM = (int)((fW*fabs(cA) + fH*fabs(sA)) + 0.5);
      yM = (int)((fW*fabs(sA) + fH*fabs(cA)) + 0.5);
      yd = y-(yM>>1);
      if (yd < topClip) {
	int delta;

	delta = topClip - yd;
	h = hC - delta;
	ys = RENDER_CENTER_Y + yC + delta;
	yd = topClip;
      } else {
	ys = RENDER_CENTER_Y+yC;
	h = hC;
      }
      gdk_draw_drawable(dest, gC[OR_BLUE], cairoPixmap, RENDER_CENTER_X+xC, ys,
			x-(xM>>1), yd, wC, h);
    } else
      gdk_draw_drawable(dest, gC[OR_BLUE], cairoPixmap, RENDER_CENTER_X+xC, RENDER_CENTER_Y+yC,
			x, y-((*height)>>1), wC, hC);
  }
} /* End of  R E N D E R  P A N G O  T E X T */

/*
  C A L C U L A T E  J U L I A N  D A T E

  calculateJulianDate converts the Gregorian Calendar date (dd/mm/yyy)
  to the Julian Date, which it returns.
 */
int calculateJulianDate(int yyyy, int mm, int dd)
{
  int a, y, m;

  a = (14 - mm)/12;
  y = yyyy + 4800 - a;
  m = mm + 12*a - 3;
  return(dd + (153*m + 2)/5 + 365*y +y/4 - y/100 + y/400 -32045);
} /* End of  C A L C U L A T E  J U L I A N  D A T E */

/*
  T J D  T O  D A T E

  Convert the Julian day into Calendar Date as per
  "Astronomical Algorithms" (Meeus)
*/
void tJDToDate(double tJD, int *year, int *month, int *day)
{
  int Z, alpha, A, B, C, D, E;
  double F;

  Z = (int)(tJD+0.5);
  F = tJD + 0.5 - (double)Z;
  if (Z >= 2299161) {
    alpha = (int)(((double)Z - 1867216.25)/36524.25);
    A = Z + 1 + alpha - alpha/4;
  } else
    A = Z;
  B = A + 1524;
  C = (int)(((double)B - 122.1) / 365.25);
  D = (int)(365.25 * (double)C);
  E = (int)(((double)(B - D))/30.6001);
  *day = (int)((double)B - (double)D - (double)((int)(30.6001*(double)E)) + F);
  if (E < 14)
    *month = E - 1;
  else
    *month = E - 13;
  if (*month > 2)
    *year = C - 4716;
  else
    *year = C - 4715;
} /* End of  T J D  T O  D A T E */

/*
  C A L C U L A T E  G O A L  D A T E

  Attempt to calculate the date on which the goal weight will be reached.
  Return TRUE for success, FALSE for failure.  A linear least squares fit is
  done over the interval defined by fitInterval.
 */
int calculateGoalDate(double *jD, int *year, int *month, int *day,
		      int *hour, int *minute, double *slope, double *intercept,
		      int *nFitPoints)
{
  if (((fitType == FIT_WINDOW) && (fitInterval > 0))
      || ((fitType == FIT_FROM_DATE) && (fitYear > 0))) {
    int nPoints = 0;
    double interval;
    double Sx, Sy, Sxx, Sxy;
    logEntry *ptr;

    Sx = Sy = Sxx = Sxy = 0.0;
    if (fitType == FIT_WINDOW)
      interval = (double)fitInterval;
    else {
      if (logRoot == NULL)
	return(FALSE);
      else
	interval = lastEntry->time - (double)calculateJulianDate(fitYear, fitMonth, fitDay);
    }
    ptr = logRoot;
    while (ptr != NULL) {
      if ((lastEntry->time - ptr->time) <= interval) {
	Sx  += ptr->time;
	Sy  += ptr->weight;
	Sxx += ptr->time * ptr->time;
	Sxy += ptr->time * ptr->weight;
	nPoints++;
      }
      ptr = ptr->next;
    }
    if (nPoints < 2)
      /* We need at least two points to fit a line */
      return(FALSE);
    else {
      double d, n;

      n = (double)nPoints;
      d = n*Sxx - Sx*Sx;
      if (d == 0.0)
	return(FALSE);
      else {
	int yyyy, mmmm, dd, hh, mm;
	double a, b, goalJD;

	b = (n*Sxy - Sx*Sy)/d;
	a = Sy/n - b*Sx/n;
	if (b == 0.0) {
	  goalJD = -1.0;
	  yyyy   = -1;
	  mmmm = dd = hh = mm = 0;
	} else {
	  double wholeJD, fracJD;

	  goalJD = (myTarget - a)/b;
	  wholeJD = (double)((int)goalJD);
	  fracJD = goalJD - wholeJD;
	  hh = (int)(fracJD*24.0);
	  mm = (int)((fracJD - ((double)hh)/24.0)*1440.0 + 0.5);
	  if (mm >= 60)
	    mm = 59;
	  tJDToDate(goalJD, &yyyy, &mmmm, &dd);
	}
	if (jD != NULL)
	  *jD = goalJD;
	if (year != NULL)
	  *year = yyyy;
	if (month != NULL)
	  *month = mmmm;
	if (day != NULL)
	  *day = dd;
	if (hour != NULL)
	  *hour = hh;
	if (minute != NULL)
	  *minute = mm;
	if (slope != NULL)
	  *slope = b;
	if (intercept != NULL)
	  *intercept = a;
	if (nFitPoints != NULL)
	  *nFitPoints = nPoints;
	return(TRUE);
      }
    }
  } else
    return(FALSE);
} /* End of  C A L C U L A T E  G O A L  D A T E */

/*
  E N Q U E U E  E N T R Y

  enqueueEntry adds a new log entry to the end of the linked
  list of log entries.   It keeps them in time order, with logRoot
  pointing to the earliest.
*/
void enqueueEntry(double jD, float weight, char *comment)
{
  logEntry *newEntry;

  newEntry = (logEntry *)malloc(sizeof(logEntry));
  if (newEntry == NULL) {
    perror("malloc of newEntry");
    exit(ERROR_EXIT);
  }
  newEntry->time = jD;
  newEntry->weight = weight;
  newEntry->comment = comment;
  newEntry->next = newEntry->last = NULL;
  if (logRoot == NULL)
    /* This is the first entry, just make the root point to it. */
    logRoot = newEntry;
  else {
    logEntry *ptr;

    if (logRoot->time > jD) {
      /* The new entry is the very oldest, insert it at the list's begining. */
      newEntry->next = logRoot;
      logRoot->last = newEntry;
      logRoot = newEntry;
    } else {
      /* Find out where the new entry belongs in the time-ordered list. */
      ptr = logRoot;
      while ((ptr != NULL) && (ptr->time < jD))
	ptr = ptr->next;
      if (ptr == NULL) {
	/* The new entry is the newest, put it at the end. */
	lastEntry->next = newEntry;
	newEntry->last = lastEntry;
      } else {
	/* Insert the new entry inside the list. */
	newEntry->next = ptr;
	newEntry->last = ptr->last;
	(ptr->last)->next = newEntry;
	ptr->last = newEntry;
      }
    }
  }
  /* Make sure lastEntry points to the last entry in the data queue */
  lastEntry = logRoot;
  while (lastEntry->next != NULL)
    lastEntry = lastEntry->next;
  nDataPoints++;
} /* End of  E N Q U E U E  E N T R Y */

/*
  C A L C U L A T E  W E I G H T E D  A V E R A G E 1

  This function calculates the weighted average of the available
  available weight from the first entry until the one pointed
  to by the end parameter passed to it.   If a weight was
  entered more than 100 days ago, it is ignored (to reduce the
  coumputation load for large data sets).
 */
float calculateWeightedAverage1(logEntry *end)
{
  double startTime;
  float ave, weightSum, weight;
  float p = 0.9; /* The weight of each entry is reduced by p per day */
  logEntry *ptr;

  ptr = end;
  startTime = end->time;
  ave = weightSum = 0.0;
  while (ptr != NULL) {
    if ((startTime - ptr->time) < 100.0) {
      weight = powf(p, (float)(startTime - ptr->time));
      ave += weight * ptr->weight;
      weightSum += weight;
    }
    ptr = ptr->last;
  }
  ave /= weightSum;
  return(ave);
} /* End of  C A L C U L A T E  W E I G H T E D  A V E R A G E 1 */

/*
  H A C K E R  D I E T  A V E R A G E

  This routine calculates the weighted average using the "Hacker Diet"
  algorithm.
 */
float hackerDietAverage(logEntry *end)
{
  float ave;
  float p = 0.9;
  logEntry *ptr;

  ave = logRoot->weight;
  ptr = logRoot;
  while (ptr != end) {
    if (ptr->next != NULL)
      ave = ave + (1.0-p)*(ptr->next->weight - ave);
    ptr = ptr->next;
  }
  return(ave);
} /* End of  H A C K E R  D I E T  A V E R A G E */

/*
  C A L C U L A T E  W E I G H T E D  A V E R A G E

  Select which function to use when calculating the weighted
  average of the weight data values.
 */
float calculateWeightedAverage(logEntry *end)
{
  if (hackerDietMode)
    return(hackerDietAverage(end));
  else
    return(calculateWeightedAverage1(end));
} /* End of  C A L C U L A T E  W E I G H T E D  A V E R A G E */

/*
  G E T  L I N E

  Read a line of text from the config file, and strip comments (flagged by #).
*/
int getLine(int fD, int stripComments, char *buffer, int *eOF)
{
  char inChar = (char)0;
  int count = 0;
  int sawComment = FALSE;
  int foundSomething = FALSE;

  buffer[0] = (char)0;
  while ((!(*eOF)) && (inChar != '\n') && (count < 132)) {
    int nChar;

    nChar = read(fD, &inChar, 1);
    if (nChar > 0) {
      foundSomething = TRUE;
      if ((inChar == '#') && stripComments)
        sawComment = TRUE;
      if (!sawComment)
        buffer[count++] = inChar;
    } else {
      *eOF = TRUE;
    }
  }
  if (foundSomething) {
    if (count > 0)
      buffer[count-1] = (char)0;
    return(TRUE);
  } else
    return(FALSE);
} /* End of  G E T  L I N E */

/*
  C O M M A S  T O  P E R I O D S

  Kludge: sscanf does not seem to honor the regionalization settings, in particular
  whether a period or comma is used as the decimal separator for printed floats.
  So I'll just switch commas to periods before passing the string to sscanf.
*/
void commasToPeriods(char *string)
{
  int i;

  if (strlen(string) > 0) {
    for (i = 0; i < strlen(string); i++)
      if (string[i] == '#')
	break;
      else if (string[i] == ',')
	string[i] = '.';
  }
} /* End of C O M M A S  T O  P E R I O D S */

/*
  T O K E N  C H E C K

  Scan "line" for "token".   If found, read the value into
  "value" as an integer, double or float, depending on "type".

  Return TRUE IFF the token is seen.
*/
int tokenCheck(char *line, char *token, int type, void *value)
{
  if (strstr(line, token)) {
    int nRead;

    switch (type) {
    case INT_TOKEN:
      nRead = sscanf(&((char *)strstr(line, token))[strlen(token)+1], "%d", (int *)value);
      if (nRead != 1) {
	fprintf(stderr, "Unable to parse config file line \"%s\"\n", line);
	return(FALSE);
      }
      break;
    case DOUBLE_TOKEN:
      commasToPeriods(&((char *)strstr(line, token))[strlen(token)+1]);
      nRead = sscanf(&((char *)strstr(line, token))[strlen(token)+1], "%lf", (double *)value);
      if (nRead != 1) {
	fprintf(stderr, "Unable to parse config file line \"%s\"\n", line);
	return(FALSE);
      }
      break;
    case FLOAT_TOKEN:
      commasToPeriods(&((char *)strstr(line, token))[strlen(token)+1]);
      nRead = sscanf(&((char *)strstr(line, token))[strlen(token)+1], "%f", (float *)value);
      if (nRead != 1) {
	fprintf(stderr, "Unable to parse config file line \"%s\"\n", line);
	return(FALSE);
      }
      break;
    case STRING_TOKEN:
      nRead = sscanf(&((char *)strstr(line, token))[strlen(token)+1], "%s", (char *)value);
      if (nRead != 1) {
	fprintf(stderr, "Unable to parse config file line \"%s\"\n", line);
	return(FALSE);
      }
      break;
    default:
      fprintf(stderr, "Unrecognized type (%d) passed to tokenCheck\n", type);
    }
    return(TRUE);
  } else
    return(FALSE);
} /* End of  T O K E N  C H E C K */

/*
  R E A D  S E T T I N G S

  Read the settings file and set the variables stored in it.
*/
void readSettings(char *fileName)
{
  int eOF = FALSE;
  int lineNumber = 0;
  int settingsFD;
  char inLine[100];

  settingsFD = open(fileName, O_RDONLY);
  if (settingsFD < 0) {
    /* Not having a settings file is nonfatal - the default values will do. */
    perror("settings");
    return;
  }
  while (!eOF) {
    lineNumber++;
    if (getLine(settingsFD, TRUE, &inLine[0], &eOF))
      if (strlen(inLine) > 0) {
	tokenCheck(inLine, "MY_HEIGHT",          DOUBLE_TOKEN, &myHeight);
	tokenCheck(inLine, "MY_TARGET",          DOUBLE_TOKEN, &myTarget);
	tokenCheck(inLine, "WEIGHT_KG",             INT_TOKEN, &weightkg);
	tokenCheck(inLine, "HEIGHT_CM",             INT_TOKEN, &heightcm);
	tokenCheck(inLine, "MONTH_FIRST",           INT_TOKEN, &monthFirst);
	tokenCheck(inLine, "NONJUDGEMENTAL_COLORS", INT_TOKEN, &nonjudgementalColors);
	tokenCheck(inLine, "HACKER_DIET_MODE",      INT_TOKEN, &hackerDietMode);
	tokenCheck(inLine, "SHOW_COMMENTS",         INT_TOKEN, &showComments);
	tokenCheck(inLine, "SHOW_TARGET",           INT_TOKEN, &showTarget);
	tokenCheck(inLine, "PLOT_INTERVAL",         INT_TOKEN, &plotInterval);
	tokenCheck(inLine, "AVE_INTERVAL",          INT_TOKEN, &aveInterval);
	tokenCheck(inLine, "FIT_INTERVAL",          INT_TOKEN, &fitInterval);
	tokenCheck(inLine, "FIT_TYPE",              INT_TOKEN, &fitType);
	tokenCheck(inLine, "LOSING_WEIGHT_IS_GOOD", INT_TOKEN, &losingWeightIsGood);
	tokenCheck(inLine, "START_WITH_DATA_ENTRY", INT_TOKEN, &startWithDataEntry);
	/* tokenCheck(inLine, "INTERPOLATE_FIRST_PAIR",INT_TOKEN, &interpolateFirstPair); */
	tokenCheck(inLine, "PLOT_TREND_LINE",       INT_TOKEN, &plotTrendLine);
	tokenCheck(inLine, "PLOT_DATA_POINTS",      INT_TOKEN, &plotDataPoints);
	tokenCheck(inLine, "PLOT_WINDOW",           INT_TOKEN, &plotWindow);
	tokenCheck(inLine, "FIT_DAY",               INT_TOKEN, &fitDay);
	tokenCheck(inLine, "FIT_MONTH",             INT_TOKEN, &fitMonth);
	tokenCheck(inLine, "FIT_YEAR",              INT_TOKEN, &fitYear);
	tokenCheck(inLine, "PLOT_START_DAY",        INT_TOKEN, &plotStartDay);
	tokenCheck(inLine, "PLOT_START_MONTH",      INT_TOKEN, &plotStartMonth);
	tokenCheck(inLine, "PLOT_START_YEAR",       INT_TOKEN, &plotStartYear);
	tokenCheck(inLine, "PLOT_END_DAY",          INT_TOKEN, &plotEndDay);
	tokenCheck(inLine, "PLOT_END_MONTH",        INT_TOKEN, &plotEndMonth);
	tokenCheck(inLine, "PLOT_END_YEAR",         INT_TOKEN, &plotEndYear);
	tokenCheck(inLine, "PLOT_FROM_FIRST",       INT_TOKEN, &plotFromFirst);
	tokenCheck(inLine, "PLOT_TO_LAST",          INT_TOKEN, &plotToLast);
	tokenCheck(inLine, "HAVE_GOAL_DATE",        INT_TOKEN, &haveGoalDate);
	tokenCheck(inLine, "TARGET_DAY",            INT_TOKEN, &targetDay);
	tokenCheck(inLine, "TARGET_MONTH",          INT_TOKEN, &targetMonth);
	tokenCheck(inLine, "TARGET_YEAR",           INT_TOKEN, &targetYear);
      }
  }
  if (nonjudgementalColors)
    goodColor = badColor = OR_BLUE;
  else {
    if (losingWeightIsGood) {
      goodColor = OR_GREEN;
      badColor  = OR_RED;
    } else {
      goodColor = OR_RED;
      badColor  = OR_GREEN;
    }
  }
  close(settingsFD);
} /* End of  R E A D  S E T T I N G S */

/*
  C A L C  M A X I M A

  Calculate the maximum and minimum weight and time (JD) values for the
  entire data set from startTJD though endTJD.  The plot
  maxima and minima are also computed.
 */
void calcMaxima(double startTJD, double endTJD)
{
  if (nDataPoints == 1) {
    double aveJD, aveWeight;

    maxJD     = minJD     = logRoot->time;
    maxWeight = minWeight = logRoot->weight;
    aveJD = (maxJD+minJD)*0.5;
    maxJD = aveJD + 5.0;
    minJD = aveJD - 5.0;
    aveWeight = (maxWeight+minWeight)*0.5;
    maxWeight = aveWeight + 5.0;
    minWeight = aveWeight - 5.0;
  } else {
    logEntry *ptr;

    maxJD = -1.0; minJD = 1.0e30; maxWeight = -1.0; minWeight = 1.0e30;
    ptr = logRoot;
    while (ptr != NULL) {
      if ((ptr->time >= startTJD) && (ptr->time <= endTJD)) {
	if (maxJD < ptr->time)
	  maxJD = ptr->time;
	if (minJD > ptr->time)
	  minJD = ptr->time;
	if (maxWeight < ptr->weight)
	  maxWeight = ptr->weight;
	if (minWeight > ptr->weight)
	  minWeight = ptr->weight;
	if (maxWeight < calculateWeightedAverage(ptr))
	  maxWeight = calculateWeightedAverage(ptr);
	if (minWeight > calculateWeightedAverage(ptr))
	  minWeight = calculateWeightedAverage(ptr);
      }
      ptr = ptr->next;
    }
  }
  if (endTJD > maxJD)
    maxJD = endTJD;
  if ((maxJD - minJD) < 1.0) {
    maxJD += 1.0;
    minJD -= 1.0;
  }
  /*
    Make sure the plot Y axis covers at least 2 weight unit increments,
    so that at least 2 Y axis label values will be plotted.
  */
  if ((maxWeight - minWeight) < 2.0) {
    maxWeight += 1.0;
    minWeight -= 1.0;
  }
  plotMaxX = maxJD + BORDER_FACTOR_X*(maxJD-minJD);
  plotMinX = minJD - BORDER_FACTOR_X*(maxJD-minJD);
  if (showTarget) {
    if (maxWeight < myTarget)
      maxWeight = myTarget;
    else if (minWeight > myTarget)
      minWeight = myTarget;
  }
  plotMaxY = maxWeight + BORDER_FACTOR_Y*(maxWeight-minWeight);
  plotMinY = minWeight - BORDER_FACTOR_Y*(maxWeight-minWeight);
  dprintf("maxWeight = %f minWeight = %f plotMaxY = %f plotMinY = %f\n",
	 maxWeight, minWeight, plotMaxY, plotMinY);
} /* End of  C A L C  M A X I M A */

/*
  R E A D  D A T A

  Read in the date, time and weight data from the data file.
*/
void readData(char *fileName)
{
  int eOF = FALSE;
  int dataFD;
  char inLine[1000];

  nDataPoints = 0;
  dprintf("Trying to open \"%s\"\n", fileName);
  dataFD = open(fileName, O_RDONLY);
  if (dataFD >= 0) {
    while (!eOF) {
      int nRead;
      float newWeight;
      char dateString[100], timeString[100];

      getLine(dataFD, FALSE, &inLine[0], &eOF);
      commasToPeriods(inLine);
      nRead = sscanf(inLine, "%s %s %f", dateString, timeString, &newWeight);
      if (nRead == 3) {
	double dayFraction, fJD;
	int hh, mm, dd, MM, yyyy, jD;
	char *commentPtr;
	char *comment = NULL;

	sscanf(timeString, "%d:%d", &hh, &mm);
	dayFraction = ((double)hh + (double)mm/60.0)/24.0;
	sscanf(dateString,"%d/%d/%d", &dd, &MM, &yyyy);
	jD = calculateJulianDate(yyyy, MM, dd);
	fJD = (double)jD + dayFraction;
	commentPtr = strstr(inLine, "# ");
	if (commentPtr) {
	  commentPtr += 2;
	  dprintf("Comment: \"%s\"\n", commentPtr);
	  comment = malloc(strlen(commentPtr)+1);
	  if (comment)
	    sprintf(comment, "%s", commentPtr);
	}
	enqueueEntry(fJD, newWeight, comment);
      }
    }
    calcMaxima(0.0, lastEntry->time);
    close(dataFD);
  } else
    perror("Data file could not be opened");
} /* End of  R E A D  D A T A */

/*
  S C R E E N  C O O R D I N A T E S

  This function is called with plot coordinates in physical units
  (time and weight) and it returns x, y coordinates in pixels on
  the weight vs time plot.
 */
void screenCoordinates(double xScale, double yScale,
		       double x, double y, int *ix, int *iy)
{
  *ix = (x - plotMinX)*xScale + LEFT_BORDER;
  *iy = displayHeight - ((y - plotMinY)*yScale + BOTTOM_BORDER) - 2;
} /* End of  S C R E E N  C O O R D I N A T E S */

/*
  B M I

  This function is passed a weight, and returns a Body Mass Index
  value.
 */
double bMI(double weight)
{
  double height, tWeight;

  if (weightkg)
    tWeight = weight/KG_PER_LB;
  else
    tWeight = weight;
  if (heightcm)
    height = myHeight/2.54;
  else
    height = myHeight;
  return (tWeight*KG_PER_LB/(height*height*M_PER_IN*M_PER_IN));
} /* End of  B M I */

/*
  W E I G H T

  This function is called with a BMI value, and it returns the
  corresponding weight.
 */
double weight(double bMI)
{
  double height;

  if (heightcm)
    height = myHeight/2.54;
  else
    height = myHeight;
  if (weightkg)
    return (bMI*height*height*M_PER_IN*M_PER_IN);
  else
    return (bMI*height*height*M_PER_IN*M_PER_IN/KG_PER_LB);
} /* End of  W E I G H T */

/*
  C A L C  S T A T S

  This function is passed a number of days, and it calculates the
  weight loss statistics from a period that far in the past, until
  the current time.
 */
int calcStats(int days, 
	      float *startWeight, float *endWeight,
	      float *maxWeight, float *minWeight, float *aveWeight)
{
  int doStart, doEnd, doMax, doMin, doAve;
  int nSamples = 0;
  double earliestTJD, dDays;
  logEntry *ptr;

  if (logRoot == NULL)
    return(FALSE);
  if (days < 0)
    dDays = lastEntry->time - logRoot->time;
  else
    dDays = (double)days;
  if ((lastEntry->time - logRoot->time) < dDays)
    return(FALSE);
  if (startWeight == NULL) doStart = FALSE; else doStart = TRUE;
  if (endWeight   == NULL) doEnd   = FALSE; else doEnd   = TRUE;
  if (maxWeight   == NULL) doMax   = FALSE; else doMax   = TRUE;
  if (minWeight   == NULL) doMin   = FALSE; else doMin   = TRUE;
  if (aveWeight   == NULL) doAve   = FALSE; else doAve   = TRUE;
  if (doMax)
    *maxWeight = -1.0e30;
  if (doMin)
    *minWeight = 1.0e30;
  if (doAve)
    *aveWeight = 0.0;
  earliestTJD = lastEntry->time - dDays;
  ptr = lastEntry;
  while (ptr->time > earliestTJD)
    ptr = ptr->last;
  if (doStart)
    *startWeight = calculateWeightedAverage(ptr);
  if (doEnd)
    *endWeight = calculateWeightedAverage(lastEntry);
  while (ptr != NULL) {
    float trendWeight;

    trendWeight = calculateWeightedAverage(ptr);
    if (doMax)
      if (*maxWeight < trendWeight)
	*maxWeight = trendWeight;
    if (doMin)
      if (*minWeight > trendWeight)
	*minWeight = trendWeight;
    if (doAve) {
      *aveWeight += trendWeight;
      nSamples++;
    }
    ptr = ptr->next;
  }
  if (doAve && (nSamples > 0))
    *aveWeight /= (double)nSamples;
  if (doStart)
    dprintf("Returning startWeight = %f\n", *startWeight);
  if (doEnd)
    dprintf("Returning endWeight = %f\n", *endWeight);
  if (doMax)
    dprintf("Returning maxWeight = %f\n", *maxWeight);
  if (doMin)
    dprintf("Returning minWeight = %f\n", *minWeight);
  if (doAve)
    dprintf("Returning aveWeight = %f\n", *aveWeight);
  return(TRUE);
} /* End of  C A L C  S T A T S */

/*
  M A K E  M O N T H  S T R I N G

  Put the year or abreviated month name into the scratch string
  for printing on the plot.   Return the x coordinate of the
  first day of the month (screen coordinate system).
*/
int makeMonthString(double xScale, double yScale, double jD)
{
  int i, year, month, day, x, y;

  tJDToDate(jD, &year, &month, &day);
  if (month == 1)
    /* For January, print the year, rather than the month name */
    sprintf(scratchString, "%d", year);
  else {
    /* Abbreviate the month name to 3 characters */
    for (i = 0; i < 3; i++)
      scratchString[i] = monthName[month-1][i];
    scratchString[3] = (char)0;
  }
  i = calculateJulianDate(year, month, 1);
  screenCoordinates(xScale, yScale, (double)i, minWeight, &x, &y);
  return(x);
} /* End of  M A K E  M O N T H  S T R I N G */

/*
  R E D R A W  S C R E E N

  Redraw the weight vs time plot.   This is the default display.
 */
void redrawScreen(void)
{
  static int firstCall = TRUE;
  int nPoints = 0;
  int inc, onlyOneMonthPlotted;
  int gotGoal = FALSE;
  int nCurvePoints = 0;
  int minPlottableY, maxPlottableY, minPlottableX, maxPlottableX;
  int i, x, y, tWidth, tHeight, weightStart, weightEnd, bMIStart, bMIEnd;
  int sYear, sMonth, sDay, eYear, eMonth, eDay, plotCenter;
  int sDayS, sMonthS, sYearS;
  float weightNow, weightLastWeek;
  double plotTimeInterval = 0.0;
  double plotWeightInterval, xScale, yScale, goalDate;
  double labelDate, lastLabelDate;
  logEntry *ptr;
  logEntry *firstPlottable = NULL;
  logEntry *lastPlottable = NULL;
  GdkPoint *dataPoints, *linePoints, *lineTops, box[4];

  /*
    Here comes a kludge.   A user sent me a settings file which causes the program
    to crash.   The problem was that plotToLast was set to FALSE, but the day, month
    and year were set to -1, so the program crashed when trying to plot over some
    crazy time interval.   I am unable to figure out how the program got into that
    state, so here I'm checking for that problem (for both start and stop times)
    and setting the plotToLast flag TRUE if the date it has is clearly bad.   At
    least the app won't crash then.
   */
  if ((!plotToLast) && ((plotEndDay < 0) || (plotEndMonth < 0) || (plotEndYear < 0)))
    plotToLast = TRUE;
  if ((!plotFromFirst) && ((plotStartDay < 0) || (plotStartMonth < 0) || (plotStartYear < 0)))
    plotFromFirst = TRUE;
  /* End of kludge */

  /* Clear the entire display */
  gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
		     TRUE, 0, 0,
		     drawingArea->allocation.width,
		     drawingArea->allocation.height);

  if (logRoot == NULL) {
    renderPangoText("No data has been entered yet", OR_WHITE, BIG_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, displayWidth/2, 50, 0.0, TRUE, 0, OR_BLACK);
    renderPangoText("select the Data Entry menu option", OR_WHITE, BIG_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, displayWidth/2, 100, 0.0, TRUE, 0, OR_BLACK);
    renderPangoText("to enter initial entries.", OR_WHITE, BIG_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, displayWidth/2, 150, 0.0, TRUE, 0, OR_BLACK);
    renderPangoText("Also, use the About You menu", OR_WHITE, BIG_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, displayWidth/2, 200, 0.0, TRUE, 0, OR_BLACK);
    renderPangoText("to set your height and units.", OR_WHITE, BIG_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, displayWidth/2, 250, 0.0, TRUE, 0, OR_BLACK);
  } else {
    int nFitPoints;
    double slope, intercept;
    double earliestTJDToPlot = 0.0;
    double latestTJDToPlot = 0.0;

    if (plotWindow)
      earliestTJDToPlot = lastEntry->time - plotInterval;
    else if (plotFromFirst)
      earliestTJDToPlot = logRoot->time;
    else
      earliestTJDToPlot = (double)calculateJulianDate(plotStartYear, plotStartMonth, plotStartDay);
    if (plotWindow || plotToLast)
      latestTJDToPlot   = lastEntry->time;
    else
      latestTJDToPlot   = (double)calculateJulianDate(plotEndYear,   plotEndMonth,   plotEndDay);
    if (earliestTJDToPlot < 0.0)
      earliestTJDToPlot = logRoot->time;
    if (latestTJDToPlot > lastEntry->time)
      latestTJDToPlot = lastEntry->time;

    /* If the last date to plot is before the first date, swap the dates */
    if (latestTJDToPlot < earliestTJDToPlot) {
      double temp;

      temp = earliestTJDToPlot;
      earliestTJDToPlot = latestTJDToPlot;
      latestTJDToPlot = temp;
      hildon_banner_show_information(drawingArea, "ignored", "Plot start date > end date.   Will swap dates.");
    }
    if ((fitType != DO_NOT_FIT_DATA) && (!((fitType == FIT_WINDOW) && (fitInterval < 2))) && showTarget &&
	(gotGoal = calculateGoalDate(&goalDate, NULL, NULL, NULL, NULL, NULL, &slope, &intercept, &nFitPoints))) {
      if (goalDate > latestTJDToPlot)
	calcMaxima(earliestTJDToPlot, goalDate);
      else
	calcMaxima(earliestTJDToPlot, latestTJDToPlot);
    } else
      calcMaxima(earliestTJDToPlot, latestTJDToPlot);
    plotWeightInterval = maxWeight - minWeight;
    dataPoints = (GdkPoint *)malloc(nDataPoints * sizeof(GdkPoint));
    if (dataPoints == NULL) {
      perror("malloc of dataPoints");
      exit(ERROR_EXIT);
    }
    linePoints = (GdkPoint *)malloc(nDataPoints * sizeof(GdkPoint));
    if (linePoints == NULL) {
      perror("malloc of linePoints");
      exit(ERROR_EXIT);
    }
    lineTops = (GdkPoint *)malloc(nDataPoints * sizeof(GdkPoint));
    if (lineTops == NULL) {
      perror("malloc of lineTops");
      exit(ERROR_EXIT);
    }
    xScale = ((double)displayWidth - LEFT_BORDER - RIGHT_BORDER - 1)/(plotMaxX - plotMinX);
    yScale = ((double)displayHeight - TOP_BORDER - BOTTOM_BORDER - 4)/(plotMaxY - plotMinY);

    /* Draw the empty plot box */
    screenCoordinates(xScale, yScale, plotMinX, plotMinY, &box[0].x, &box[0].y);
    minPlottableX = box[0].x;
    minPlottableY = box[0].y;
    screenCoordinates(xScale, yScale, plotMinX, plotMaxY, &box[1].x, &box[1].y);
    maxPlottableY = box[1].y;
    screenCoordinates(xScale, yScale, plotMaxX, plotMaxY, &box[2].x, &box[2].y);
    maxPlottableX = box[2].x;
    screenCoordinates(xScale, yScale, plotMaxX, plotMinY, &box[3].x, &box[3].y);
    gdk_draw_polygon(pixmap, gC[OR_DARK_GREY], TRUE, box, 4);
    gdk_draw_polygon(pixmap, gC[OR_WHITE], FALSE, box, 4);
    /* plotCenter will be used to position the comments, if they are plotted */
    plotCenter = (box[0].y + box[1].y + box[2].y + box[3].y)/4;

    /* Draw the plot title */
    tJDToDate((double)((int)logRoot->time), &sYear, &sMonth, &sDay);
    if (logRoot->next == NULL) {
      eDay = sDay; eMonth = sMonth; eYear = sYear;
      firstPlottable = lastPlottable = logRoot;
      onlyOneMonthPlotted = TRUE;
    } else {
      /* Find the first and last data points which should be plotted. */
      ptr = lastEntry;
      while (ptr != NULL) {
	if (ptr->time >= earliestTJDToPlot)
	  firstPlottable = ptr;
	ptr = ptr->last;
      }
      ptr = logRoot;
      while (ptr != NULL) {
	if (ptr->time <= latestTJDToPlot)
	  lastPlottable = ptr;
	ptr = ptr->next;
      }

      /* Generate the starting and stopping dates for the plot. */
      tJDToDate((double)((int)firstPlottable->time), &sYear, &sMonth, &sDay);
      if (gotGoal)
	tJDToDate(goalDate, &eYear, &eMonth, &eDay);
      else
	tJDToDate((double)((int)lastPlottable->time), &eYear, &eMonth, &eDay);
      if ((sMonth == eMonth) && (sYear == eYear))
	onlyOneMonthPlotted = TRUE;
      else
	onlyOneMonthPlotted = FALSE;

    }
    sDayS = sDay; sMonthS = sMonth; sYearS = sYear;
    if (showTarget) {
      int x0, y0, x1, y1;

      if (gotGoal) {
	double leftY, fitStartTime;

	/* Draw a line showing begining of fit interval */
	if (fitType == FIT_WINDOW)
	  fitStartTime = latestTJDToPlot - (double)fitInterval;
	else
	  fitStartTime = (double)calculateJulianDate(fitYear, fitMonth, fitDay);
	screenCoordinates(xScale, yScale, fitStartTime, plotMinY, &x0, &y0);
	screenCoordinates(xScale, yScale, fitStartTime, plotMaxY, &x1, &y1);
	if (x0 > minPlottableX)
	  gdk_draw_line(pixmap, gC[OR_GREY], x0, y0, x1, y1);

	/* Draw a line showing the least squares fit line */
	leftY = intercept + slope*plotMinX;
	if (leftY > plotMaxY) {
	  double leftX;

	  leftX = (plotMaxY - intercept)/slope;
	  screenCoordinates(xScale, yScale, leftX, plotMaxY, &x0, &y0);
	} else if (leftY < plotMinY) {
	  double leftX;

	  leftX = (plotMinY - intercept)/slope;
	  screenCoordinates(xScale, yScale, leftX, plotMinY, &x0, &y0);
	} else
	  screenCoordinates(xScale, yScale, plotMinX, leftY, &x0, &y0);
	screenCoordinates(xScale, yScale, goalDate, myTarget, &x1, &y1);
	gdk_draw_line(pixmap, gC[OR_LIGHT_BLUE], x0, y0, x1, y1);
	if (weightkg)
	  sprintf(scratchString, "slope = %6.2f kg/week", slope * ((float)ONE_WEEK));
	else
	  sprintf(scratchString, "slope = %6.2f lbs/week", slope * ((float)ONE_WEEK));
	if (slope < 0.0) {
	  renderPangoText(scratchString, OR_LIGHT_BLUE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth*2/3, displayHeight*1/12,
			  0.0, FALSE, 0, OR_DARK_GREY);
	} else {
	  renderPangoText(scratchString, OR_LIGHT_BLUE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth*1/8, displayHeight*1/12,
			  0.0, FALSE, 0, OR_DARK_GREY);
	}
	sprintf(scratchString, "%d points used for fit", nFitPoints);
	if (slope < 0.0) {
	  renderPangoText(scratchString, OR_LIGHT_BLUE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth*2/3, displayHeight*1/7,
			  0.0, FALSE, 0, OR_DARK_GREY);
	} else {
	  renderPangoText(scratchString, OR_LIGHT_BLUE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth*1/8, displayHeight*1/7,
			  0.0, FALSE, 0, OR_DARK_GREY);
	}
      }
      /* Draw a thick green line at the target weight */
      screenCoordinates(xScale, yScale, plotMinX, myTarget, &x0, &y0);
      screenCoordinates(xScale, yScale, plotMaxX, myTarget, &x1, &y1);
      box[0].x = x0;       box[0].y = y0-1;
      box[1].x = x1;       box[1].y = box[0].y;
      box[2].x = box[1].x; box[2].y = y0+1;
      box[3].x = box[0].x; box[3].y = box[2].y;
      gdk_draw_polygon(pixmap, gC[OR_GREEN], TRUE, box, 4);
    }

    /* Y axis labels */
    ptr = logRoot;
    weightStart = (int)(minWeight + 0.5);
    bMIStart    = 10*(int)(bMI(minWeight) - 0.5);
    weightEnd   = (int)(maxWeight + 0.5);
    bMIEnd      = 10*(int)(bMI(maxWeight) + 0.5);
    if (weightStart < weightEnd)
      for (i = weightStart; i <= weightEnd; i += 1) {
	int color, stub;
	
	sprintf(scratchString, "%3d", i);
	screenCoordinates(xScale, yScale, logRoot->time, (double)i, &x, &y);
	if ((y <= minPlottableY) && (y >= maxPlottableY)) {
	  if (i % 10 == 0) {
	    color = OR_WHITE;
	    stub = 10;
	    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 0, y, 0.0, FALSE, 0, OR_BLACK);
	    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 0, y, 0.0, FALSE, 0, OR_BLACK);
	  } else if (i % 5 == 0) {
	    stub = 8;
	    if (plotWeightInterval < 20.0) {
	      renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			      &tWidth, &tHeight, pixmap, 0, y, 0.0, FALSE, 0, OR_BLACK);
	      renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			      &tWidth, &tHeight, pixmap, 0, y, 0.0, FALSE, 0, OR_BLACK);
	    }
	  } else {
	    stub = 5;
	    if (plotWeightInterval < 10.0) {
	      renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			      &tWidth, &tHeight, pixmap, 0, y, 0.0, FALSE, 0, OR_BLACK);
	      renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			      &tWidth, &tHeight, pixmap, 0, y, 0.0, FALSE, 0, OR_BLACK);
	    }
	  }
	  gdk_draw_line(pixmap, gC[OR_WHITE], LEFT_BORDER, y, LEFT_BORDER+stub, y);
	}
      }
    if (bMIStart != bMIEnd) {
      int bMIInc;

      if ((maxWeight - minWeight) >= 100.0)
	bMIInc = 20;
      else if ((maxWeight - minWeight) >= 50.0)
	bMIInc = 10;
      else if ((maxWeight - minWeight) >= 10.0)
	bMIInc = 5;
      else
	bMIInc = 1;
      for (i = bMIStart-20; i <= bMIEnd+20; i += bMIInc) {
	double tWeight;
	int color, stub;

	tWeight = weight(0.1 * (double)i);
	screenCoordinates(xScale, yScale, logRoot->time, tWeight, &x, &y);
	if ((y > TOP_BORDER) && (y < displayHeight-BOTTOM_BORDER)) {
	  sprintf(scratchString, "%4.1f", 0.1*(double)i);
	  if (i % 10 == 0) {
	    if (nonjudgementalColors)
	      color = OR_WHITE;
	    else if ((i == 300) || (i == 160))
	      color = OR_ORANGE;
	    else if ((i == 250) || (i == 200))
	      color = OR_YELLOW_GREEN;
	    else if (i > 300)
	      color = OR_RED;
	    else if (i > 250)
	      color = OR_YELLOW;
	    else if (i < 160)
	      color = OR_RED;
	    else if (i < 200)
	      color = OR_YELLOW;
	    else
	      color = OR_GREEN;
	    stub = 10;
	  } else {
	    if (nonjudgementalColors)
	      color = OR_BLUE;
	    else if (i >= 300)
	      color = OR_RED;
	    else if (i >= 250)
	      color = OR_YELLOW;
	    else if (i <= 160)
	      color = OR_RED;
	    else if (i <= 200)
	      color = OR_YELLOW;
	    else
	      color = OR_GREEN;
	    stub = 5;
	  }
	  x = displayWidth-RIGHT_BORDER;
	  renderPangoText(scratchString, color, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, x+2, y, 0.0, FALSE, 0, OR_BLACK);
	  gdk_draw_line(pixmap, gC[OR_WHITE], x, y, x-stub, y);
	}
      }
    }

    /* X axis labelling */
    if (onlyOneMonthPlotted) {
      /*
	All the data to be plotted falls within one month, so we should
	show the full month name, and the days within the plot range
      */
      int day, x, y, lDay, lMonth, lYear;
      double jD;

      tJDToDate((double)((int)firstPlottable->time), &lYear, &lMonth, &lDay);
      for (day = 1; day <= monthLengths[sMonth-1]; day++) {
	jD = (double)calculateJulianDate(lYear, lMonth, day);
	screenCoordinates(xScale, yScale, jD, plotMinY, &x, &y);
	if ((x > minPlottableX) && (x < maxPlottableX)) {
	  gdk_draw_line(pixmap, gC[OR_WHITE], x, y, x, y-4);
	  sprintf(scratchString, "%d", day);
	  renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, x, y+14, 0.0, TRUE, 0, OR_BLACK);
	}
      }
    } else {
      /* More than one month covered by the plot */
      x = makeMonthString(xScale, yScale, firstPlottable->time);
      if (gotGoal && (goalDate >= maxJD))
	plotTimeInterval = goalDate - minJD;
      else
	plotTimeInterval = maxJD - minJD;
      tJDToDate(firstPlottable->time, &sYear, &sMonth, &sDay);
      if (x < RIGHT_BORDER)
	x = RIGHT_BORDER;
      if (((plotTimeInterval < 5.0) || (sDay < 26)) && ((plotMaxX - plotMinX) < 180.0))
	renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			&tWidth, &tHeight, pixmap, x, displayHeight-35, 0.0, TRUE, 0, OR_BLACK);
      if (gotGoal && (goalDate >= maxJD))
	lastLabelDate = goalDate;
      else
	lastLabelDate = lastPlottable->time;
      for (labelDate = earliestTJDToPlot; labelDate <= lastLabelDate; labelDate += 1.0) {
	int plotIt = FALSE;

	tJDToDate(labelDate, &eYear, &eMonth, &eDay);
	if (sMonth != eMonth) {
	  sMonth = eMonth;
	  x = makeMonthString(xScale, yScale, labelDate);
	  if (plotTimeInterval < 500.0)
	    plotIt = TRUE;
	  else if ((plotTimeInterval >= 500.0) && (plotTimeInterval < 1000.0) && ((sMonth-1) % 2) == 0)
	    plotIt = TRUE;
	  else if ((plotTimeInterval >= 1000.0) && (plotTimeInterval < 1500.0) && ((sMonth-1) % 3) == 0)
	    plotIt = TRUE;
	  else if ((plotTimeInterval >= 1500.0) && (plotTimeInterval < 2000.0) && ((sMonth-1) % 4) == 0)
	    plotIt = TRUE;
	  else if ((plotTimeInterval >= 2000.0) && (plotTimeInterval < 2500.0) && ((sMonth-1) % 6) == 0)
	    plotIt = TRUE;
	  else if ((plotTimeInterval >= 2500.0) && (sMonth == 1)) {
	    if ((plotTimeInterval < 7000.0)
		|| ((plotTimeInterval < 10000.0) && (eYear % 2 == 0))
		|| ((plotTimeInterval < 20000.0) && (eYear % 4 == 0))
		|| (eYear % 10 == 0))
	    plotIt = TRUE;
	  } else
	    plotIt = FALSE;
	  if (plotIt && (x >= RIGHT_BORDER)) {
	    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, x, displayHeight-35, 0.0, TRUE, 0, OR_BLACK);
	    if (eMonth == 1) {
	      /* Draw a little triangle to mark the start of a year */
	      box[0].x = x-5; box[0].y = displayHeight - BOTTOM_BORDER - 2;
	      box[1].x = x+5; box[1].y = box[0].y;
	      box[2].x = x;   box[2].y = box[1].y - 10;
	      gdk_draw_polygon(pixmap, gC[OR_WHITE], TRUE, box, 3);
	    } else {
	      /* Just draw a tickmark for a month */
	      y = displayHeight - BOTTOM_BORDER - 2;
	      gdk_draw_line(pixmap, gC[OR_WHITE], x, y, x, y-10);
	    }
	  }
	}
      }
    } /* end of multimonth plot labelling */

    /* Put together the summary line for the bottom of the plot. */
    if (calcStats(aveInterval, &weightLastWeek, &weightNow, NULL, NULL, NULL)) {
      float divisor;
      char intervalLabel[40];

      if (aveInterval < 0)
	divisor = lastEntry->time - logRoot->time;
      else
	divisor = (float)aveInterval;
      if (divisor == 0.0)
	divisor = 1.0;
      switch (aveInterval) {
      case ONE_WEEK:
	strcpy(intervalLabel, "Last week");
	break;
      case ONE_FORTNIGHT:
	strcpy(intervalLabel, "Last fortnight");
	break;
      case ONE_MONTH:
	strcpy(intervalLabel, "Last month");
	break;
      case ONE_QUARTER:
	strcpy(intervalLabel, "Last quarter");
	break;
      case ONE_SIX_MONTHS:
	strcpy(intervalLabel, "Last six month");
	break;
      case ONE_YEAR:
	strcpy(intervalLabel, "Last year");
	break;
      default:
	strcpy(intervalLabel, "Entire history");
      }
      if (weightNow < weightLastWeek) {
	if (weightkg)
	  sprintf(scratchString,
		  "Weight %5.1f kg,  BMI %3.1f,  %s's weight loss %4.2f kg,  Daily deficit %1.0f calories",
		  weightNow, bMI(weightNow), intervalLabel, weightLastWeek - weightNow,
		  (weightLastWeek - weightNow)*(CALORIES_PER_POUND/divisor)/KG_PER_LB);
	else
	  sprintf(scratchString,
		  "Weight %5.1f lbs,  BMI %3.1f,  %s's weight loss %4.2f lbs,  Daily deficit %1.0f calories",
		  weightNow, bMI(weightNow), intervalLabel, weightLastWeek - weightNow,
		  (weightLastWeek - weightNow)*(CALORIES_PER_POUND/divisor));
      } else {
	if (weightkg)
	  sprintf(scratchString,
		  "Weight %5.1f kg,  BMI %3.1f,  %s's weight gain %4.2f kg,  Daily excess %1.0f calories",
		  weightNow, bMI(weightNow), intervalLabel, -(weightLastWeek - weightNow),
		  -(weightLastWeek - weightNow)*(CALORIES_PER_POUND/divisor)/KG_PER_LB);
	else
	  sprintf(scratchString,
		  "Weight %5.1f lbs,  BMI %3.1f,  %s's weight gain %4.2f lbs,  Daily excess %1.0f calories",
		  weightNow, bMI(weightNow), intervalLabel, -(weightLastWeek - weightNow),
		  -(weightLastWeek - weightNow)*(CALORIES_PER_POUND/divisor));
      }
      renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth/2, displayHeight-13, 0.0, TRUE, 0, OR_BLACK);
      renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth/2, displayHeight-13, 0.0, TRUE, 0, OR_BLACK);
    }

    ptr = logRoot;
    while (ptr != NULL) {
      if ((ptr->time >= earliestTJDToPlot) && (ptr->time <= latestTJDToPlot)) {
	screenCoordinates(xScale, yScale, ptr->time, ptr->weight,
			  &dataPoints[nPoints].x, &dataPoints[nPoints].y);
	screenCoordinates(xScale, yScale, ptr->time, calculateWeightedAverage(ptr),
			  &linePoints[nCurvePoints].x, &linePoints[nCurvePoints].y);
	if ((linePoints[nCurvePoints].y > maxPlottableY)
	    && (linePoints[nCurvePoints].y < minPlottableY)) {
	  lineTops[nPoints].y = linePoints[nCurvePoints].y;
	  nCurvePoints++;
	} else if (linePoints[nCurvePoints].y < maxPlottableY)
	  lineTops[nPoints].y = maxPlottableY;
	else
	  lineTops[nPoints].y = minPlottableY;
	nPoints++;
      }
      ptr = ptr->next;
    }
    if (onlyOneMonthPlotted)
      sprintf(scratchString, "Weight and Trend Data for %s %d   (%d days, %d entries)",
	      monthName[sMonthS-1], sYearS, (int)(0.5 + latestTJDToPlot - earliestTJDToPlot), nPoints);
    else {
      tJDToDate((double)((int)latestTJDToPlot), &eYear, &eMonth, &eDay);
      if (monthFirst == 2) {
	sprintf(scratchString, "Weight and Trend Data for %d-%02d-%02d through %d-%02d-%02d   (%d days, %d entries)",
		sYearS, sMonthS, sDayS, eYear, eMonth, eDay,
		(int)(0.5 + latestTJDToPlot - earliestTJDToPlot), nPoints);
      } else {
	if (monthFirst == 1) {
	  int temp;
	  
	  temp = sDayS;
	  sDayS = sMonthS;
	  sMonthS = temp;
	  temp = eDay;
	  eDay = eMonth;
	  eMonth = temp;
	}
	sprintf(scratchString, "Weight and Trend Data for %d/%d/%d through %d/%d/%d   (%d days, %d entries)",
		sDayS, sMonthS, sYearS, eDay, eMonth, eYear,
		(int)(0.5 + latestTJDToPlot - earliestTJDToPlot), nPoints);
      }
    }
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, displayWidth/2, 10, 0.0, TRUE, 0, OR_BLACK);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, displayWidth/2, 10, 0.0, TRUE, 0, OR_BLACK);
    if (showComments) {
      int x, y;
      int firstComment = TRUE;

      ptr = logRoot;
      while (ptr != NULL) {
	if ((ptr->time >= earliestTJDToPlot) && (ptr->time <= latestTJDToPlot)) {
	  if (ptr->comment) {
	    screenCoordinates(xScale, yScale, ptr->time, calculateWeightedAverage(ptr), &x, &y);
	    renderPangoText(ptr->comment, OR_PINK, SMALL_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, x, plotCenter,
			    -M_PI*0.5, TRUE, 0, OR_DARK_GREY);
	    if (firstComment) {
	      renderPangoText(ptr->comment, OR_PINK, SMALL_PANGO_FONT,
			      &tWidth, &tHeight, pixmap, x, plotCenter,
			      -M_PI*0.5, TRUE, 0, OR_DARK_GREY);
	      firstComment = FALSE;
	    }
	  }
	}
	ptr = ptr->next;
      }
    }
    if (plotTrendLine)
      gdk_draw_lines(pixmap, gC[OR_YELLOW], linePoints, nCurvePoints);
    if (plotDataPoints) {
      if (plotTimeInterval < ((float)ONE_FORTNIGHT))
	inc = 5;
      else if (plotTimeInterval < 100.0)
	inc = 4;
      else if (plotTimeInterval < 200.0)
	inc = 3;
      else if (plotTimeInterval < 400.0)
	inc = 2;
      else
	inc = 1;
      for (i = 0; i < nPoints; i++) {
	box[0].x = dataPoints[i].x;
	box[0].y = dataPoints[i].y + inc;
	box[1].x = dataPoints[i].x + inc;
	box[1].y = dataPoints[i].y;
	box[2].x = dataPoints[i].x;
	box[2].y = dataPoints[i].y - inc;
	box[3].x = dataPoints[i].x - inc;
	box[3].y = dataPoints[i].y;
	gdk_draw_polygon(pixmap, gC[OR_WHITE], TRUE, box, 4);
	if ((plotTimeInterval < 200.0) && !gotGoal){
	  if (box[0].y < lineTops[i].y - 2)
	    gdk_draw_line(pixmap, gC[badColor], box[0].x, box[0].y, box[0].x, lineTops[i].y-1);
	  if (box[2].y > lineTops[i].y + 2)
	    gdk_draw_line(pixmap, gC[goodColor], box[0].x, box[2].y, box[0].x, lineTops[i].y+1);
	}
      }
    }
    free(dataPoints); free(linePoints); free(lineTops);
  } /* End of logRoot != NULL */
  gdk_draw_drawable(drawingArea->window,
		    drawingArea->style->fg_gc[GTK_WIDGET_STATE (drawingArea)],
		    pixmap,
		    0,0,0,0,
		    displayWidth, displayHeight);
  if (firstCall) {
    if (startWithDataEntry)
      dataEntryButtonClicked(NULL, NULL);
    firstCall = FALSE;
  }
} /* End of  R E D R A W  S C R E E N */

/*
  E X P O S E  E V E N T

  This function is called when a portion of the drawing area
  is exposed.  The plot is put together through calls which
  write on pixmap, which is copied to the drawing area for
  display.
*/
static gboolean exposeEvent(GtkWidget *widget, GdkEventExpose *event)
{
  dprintf("In exposeEvnet()\n");
  gdk_draw_drawable (widget->window,
                     widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                     pixmap,
                     event->area.x, event->area.y,
                     event->area.x, event->area.y,
                     event->area.width, event->area.height);
  return(FALSE);
} /* End of  E X P O S E  E V E N T */

/*
  C O N F I G U R E  E V E N T

  This function creates and initializes a backing pixmap
  of the appropriate size for the drawing area.
*/
static int configureEvent(GtkWidget *widget, GdkEventConfigure *event)
{
  displayHeight = widget->allocation.height;
  displayWidth  = widget->allocation.width;
  if (!cairoPixmap)
    cairoPixmap = gdk_pixmap_new(widget->window, 1100, 1100, -1);
  if (pixmap)
    g_object_unref(pixmap);
  pixmap = gdk_pixmap_new(widget->window,
			  widget->allocation.width,
			  widget->allocation.height, -1);
  dprintf("In configureEvent, w: %d, h: %d\n",
	  displayWidth, displayHeight);
  redrawScreen();
  return TRUE;
} /* End of  C O N F I G U R E  E V E N T */

/*
  M A K E  G R A P H I C  C O N T E X T S

  Make all the Graphic Contexts the program will need.
*/
static void makeGraphicContexts(GtkWidget *widget)
{
  int stat, i;
  GdkGCValues gCValues;
  GdkColor gColor;
  GdkGCValuesMask gCValuesMask;

  gCValuesMask = GDK_GC_FOREGROUND;

  for (i = 0; i < N_COLORS; i++) {
    gColor.red   = orreryColorRGB[i][0];
    gColor.green = orreryColorRGB[i][1];
    gColor.blue  = orreryColorRGB[i][2];
    stat = gdk_colormap_alloc_color(widget->style->colormap,
				    &gColor,
				    FALSE,
				    TRUE);
    if (stat != TRUE) {
      fprintf(stderr, "Error allocating color %d\n", i);
      exit(ERROR_EXIT);
    }
    gCValues.foreground = gColor;
    gC[i] = gtk_gc_get(widget->style->depth,
		       widget->style->colormap,
		       &gCValues, gCValuesMask
		       );
    if (gC[i] == NULL) {
      fprintf(stderr, "gtk_gc_get failed for color %d\n", i);
      exit(ERROR_EXIT);
    }
  }
} /* End of  M A K E  G R A P H I C  C O N T E X T S */

/*
  C R E A T E  T O U C H  S E L E C T O R

  This function creates the selector for the weight "picker".
*/
static GtkWidget *createTouchSelector(void)
{
  int weight, inc;
  char weightString[10];
  GtkWidget *selector;
  GtkListStore *model;
  GtkTreeIter iter;
  HildonTouchSelectorColumn *column = NULL;

  selector = hildon_touch_selector_new();
  model = gtk_list_store_new(1, G_TYPE_STRING);
  if (logRoot != NULL) {
    if (weightkg)
      minSelectorWeight = (int)(lastEntry->weight) - 35;
    else
      minSelectorWeight = (int)(lastEntry->weight) - 75;
    if (minSelectorWeight < 5)
      minSelectorWeight = 5;
    if (weightkg)
      maxSelectorWeight = minSelectorWeight + 75;
    else
      maxSelectorWeight = minSelectorWeight + 150;
  } else {
    if (weightkg) {
      minSelectorWeight = (int)(50.0*KG_PER_LB);
      maxSelectorWeight = (int)(250.0*KG_PER_LB);
    } else {
      minSelectorWeight = 50;
      maxSelectorWeight = 250;
    }
  }
  if (weightkg)
    inc = 1;
  else
    inc = 2;
  for (weight = minSelectorWeight*10; weight <= maxSelectorWeight*10; weight += inc) {
    sprintf(weightString, "%5.1f", 0.1*(float)weight);
    gtk_list_store_append(model, &iter);
    gtk_list_store_set(model, &iter, 0, weightString, -1);
  }
  column = hildon_touch_selector_append_text_column(HILDON_TOUCH_SELECTOR(selector),
                                                     GTK_TREE_MODEL(model), TRUE);
  g_object_set(G_OBJECT(column), "text-column", 0, NULL);  
  return selector;
} /* End of  C R E A T E  T O U C H  S E L E C T O R */

/*
  W R I T E  F I L E

  Write the data entries to the file.   The function is called
  twice, to write the main file and its backup.
 */
void writeFile(FILE *dataFile)
{
 int day, month, year, hours, minutes;
  double dayFrac, jD;
  logEntry *ptr;

  ptr = logRoot;
  while (ptr != NULL) {
    dayFrac = ptr->time - (int)ptr->time;
    jD = (double)((int)ptr->time);
    hours = (int)(dayFrac*24.0);
    minutes = (int)((dayFrac - ((double)hours)/24.0)*1440.0 + 0.5);
    if (minutes >= 60)
      minutes = 59;
    else if (minutes < 0)
      minutes = 0;
    tJDToDate(jD, &year, &month, &day);
    if (ptr->comment == NULL)
      fprintf(dataFile, "%02d/%02d/%d %02d:%02d %5.1f\n",
	      day, month, year, hours, minutes, ptr->weight);
    else
      fprintf(dataFile, "%02d/%02d/%d %02d:%02d %5.1f # %s\n",
	      day, month, year, hours, minutes, ptr->weight, ptr->comment);
    ptr = ptr->next;
  }
  fclose(dataFile);
} /* End of  W R I T E  F I L E */

/*
  W R I T E  D A T A  F I L E

  Write the main data file and its backup.
 */
void writeDataFile(void)
{
  FILE *dataFile;

  dataFile = fopen(fileName, "w");
  if (dataFile == NULL)
    fprintf(stderr, "Cannot write weight data file (%s)\n", fileName);
  else
    writeFile(dataFile);
  dataFile = fopen(backupFile, "w");
  if (dataFile == NULL)
    fprintf(stderr, "Cannot write backup data file (%s)\n", backupFile);
  else
    writeFile(dataFile);
} /* End of  W R I T E  D A T A  F I L E */

/*
  I N S E R T  Q U E U E  E N T R Y

  Insert a new value into the data queue, and calculate the
  new data and plot extrema.   Write the data file with the
  full list of data.
 */
void insertQueueEntry(double fJD, float weight, char *comment)
{
  enqueueEntry(fJD, weight, comment);
  if (nDataPoints == 1) {
    /* If this was the first entry, the extrema have never been set. */
    maxJD = minJD = fJD;
    maxWeight = minWeight = weight;
  }
  calcMaxima(0.0, lastEntry->time);
  writeDataFile();
} /* End of  I N S E R T  Q U E U E  E N T R Y */

/*
  D A T A  E N T R Y  A C C E P T  C A L L B A C K

  This fuction is called if the accept button is pushed on the
  Data Entry page.   That is the only way to save the new data.
  The Data Entry page will be destroyed.
 */
void dataEntryAcceptCallback(GtkButton *button, gpointer userData)
{
  accepted = TRUE;
  gtk_widget_destroy(dataEntryStackable);
} /* End of  D A T A  E N T R Y  A C C E P T  C A L L B A C K */

/*
  C H E C K  D A T A

  This function reads the values from the widgets on the Data Entry
  page, to put together the values for a new entry in the weight log.
 */
void checkData(void)
{
  int iJD;
  float weight;
  double fJD;
  char *weightResult;
  char *comment = NULL;
  guint year, month, day, hours, minutes;

  if (accepted) {
    hildon_date_button_get_date((HildonDateButton *)dateButton, &year, &month, &day);
    hildon_time_button_get_time((HildonTimeButton *)timeButton, &hours, &minutes);
    month += 1;
    weightResult = (char *)hildon_button_get_value(HILDON_BUTTON(weightButton));
    sscanf(weightResult, "%f", &weight);
    iJD = calculateJulianDate(year, month, day);
    fJD = (double)iJD + ((double)hours)/24.0 + ((double)minutes)/1440.0;
    comment = (char *)gtk_entry_get_text((GtkEntry *)commentText);
    dprintf("Got %d/%d/%d  %02d:%02d %f %f \"%s\"\n",
	   day, month, year, hours, minutes, weight, fJD, comment);
    /* Discard the comment, if it is just the default comment */
    if (!strcmp(defaultComment, comment))
      comment = NULL;
    insertQueueEntry(fJD, weight, comment);
  }
  redrawScreen();
} /* End of  C H E C K  D A T A */

/*
  D A T A  E N T R Y  B U T T O N  C L I C K E D

  This funtion is called if Data Entry is selected fromt the main menu.
  It builds and displays the widgets on the Data Entry page, and
  loads them with the default values.
 */
void dataEntryButtonClicked(GtkButton *button, gpointer userData)
{
  static GtkWidget *weightSelector, *dataEntryTable;

  dprintf("in dataEntryButtonClicked()\n");
  accepted = FALSE;
  dataEntryTable = gtk_table_new(1, 5, FALSE);

  dateButton = hildon_date_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  gtk_table_attach(GTK_TABLE(dataEntryTable), dateButton, 0, 1, 0, 1,
		   GTK_EXPAND, GTK_EXPAND, 0, 0);
  
  timeButton = hildon_time_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  gtk_table_attach(GTK_TABLE(dataEntryTable), timeButton, 0, 1, 1, 2,
		   GTK_EXPAND, GTK_EXPAND, 0, 0);
  
  dataEntryStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(dataEntryStackable), "destroy",
		   G_CALLBACK(checkData), NULL);
  weightSelector = createTouchSelector();
  weightButton = hildon_picker_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  if (lastEntry == NULL)
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(weightSelector), 0, 500);
  else {
    if (weightkg)
      hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(weightSelector), 0,
				       (int)((10.0*lastEntry->weight)+0.5) - 10*minSelectorWeight);
    else
      hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(weightSelector), 0,
				       (int)((5.0*lastEntry->weight)+0.5) - 5*minSelectorWeight);
  }
  if (weightkg)
    hildon_button_set_title(HILDON_BUTTON(weightButton), "Weight (kg)");
  else
    hildon_button_set_title(HILDON_BUTTON(weightButton), "Weight (lbs)");
  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(weightButton),
				    HILDON_TOUCH_SELECTOR(weightSelector));
  gtk_table_attach(GTK_TABLE(dataEntryTable), weightButton, 0, 1, 2, 3,
		   GTK_EXPAND, GTK_EXPAND, 0, 0);

  commentText = gtk_entry_new();
  gtk_entry_set_text((GtkEntry *)commentText, defaultComment);
  gtk_table_attach(GTK_TABLE(dataEntryTable), commentText, 0, 1, 3, 4,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  dataEntryAccept = gtk_button_new_with_label("Enter This Value");
  g_signal_connect (G_OBJECT(dataEntryAccept), "clicked",
		    G_CALLBACK(dataEntryAcceptCallback), NULL);
  gtk_table_attach(GTK_TABLE(dataEntryTable), dataEntryAccept, 0, 1, 4, 5,
		   GTK_EXPAND, GTK_EXPAND, 0, 0);
  
  gtk_container_add(GTK_CONTAINER(dataEntryStackable), dataEntryTable);
  gtk_widget_show_all(dataEntryStackable);
} /* End of  D A T A  E N T R Y  B U T T O N  C L I C K E D */

/*
  T R E N D  S T A C K A B L E  D E S T R O Y E D

  This function is called when the Trends or Log page is exited.   It causes
the program to re-display the main plot page.
 */
void trendStackableDestroyed(void)
{
  /*
    If the Log page is the one which was displayed, we must do the garbage
    collection for the dynamically allocated structures.
   */
  if (logDisplayed) {
    logCell *ptr, *tptr;
    
    ptr = logCellRoot;
    while (ptr != NULL) {
      tptr = ptr;
      ptr  = ptr->next;
      free(tptr);
    }
    logCellRoot = NULL;
    logDisplayed = FALSE;
  }
  logPageOffset = 0;
  gtk_widget_ref(mainBox);
  gtk_container_remove(GTK_CONTAINER(trendStackable), mainBox);
  gtk_container_add(GTK_CONTAINER(window), mainBox);
  gtk_widget_show(mainBox);
  gtk_widget_unref(mainBox);
  mainBoxInTrendStackable = FALSE;
  gdk_draw_drawable(drawingArea->window,
		    drawingArea->style->fg_gc[GTK_WIDGET_STATE (drawingArea)],
		    pixmap,
		    0,0,0,0,
		    displayWidth, displayHeight);
} /* End of  T R E N D  S T A C K A B L E  D E S T R O Y E D */

/*
  D A Y  O F  W E E K

  Return the day of the week (0->6) for year, month, day
*/
int dayOfWeek(int year, int month, int day)
{
  int century, c, weekDay, yy, y, m, leap;

  century = year / 100;
  yy = year - century*100;
  c = 2*(3 - (century % 4));
  y = yy + yy/4;
  if ((year % 400) == 0)
    leap = TRUE;
  else if ((year % 100) == 0)
    leap = FALSE;
  else if ((year % 4) == 0)
    leap = TRUE;
  else
    leap = FALSE;
  if (leap && (month < 3))
    m = (int)leapYear[month-1];
  else
    m = (int)normalYear[month-1];
  weekDay = (c + y + m + day) % ONE_WEEK;
  return(weekDay);
} /* End of  D A Y  O F  W E E K */

/*
  L O G  C A L L B A C K

  This routine builds the page which displays the weight values for a particular
  month.
 */
void logCallback(GtkButton *button, gpointer userData)
{
  int year, month, tWidth, tHeight, i, nEntries, nPages, titleFont;
  int row = 0;
  int col = 0;
  char logMonthString[100];
  char monthString[10];
  logEntry *ptr;
  logCell *cellPtr;
  GdkPoint box[4];

  logDisplayed = TRUE;
  strcpy(logMonthString, (char *)hildon_button_get_value(HILDON_BUTTON(logSelectorButton)));
  for (i = 0; i < strlen(logMonthString); i++)
    if (logMonthString[i] < 32)
      logMonthString[i] = (char)0;
  sscanf(logMonthString, "%s %d", &monthString[0], &year);
  dprintf("Month Name \"%s\", Year: %d\n", monthString, year);
  for (month = 0; month < 12; month++)
    if (!strcmp(monthString, monthName[month]))
      break;
  month++;
  dprintf("Month number %d\n", month);

  /* Figure out how many pages we need to display all values */
  ptr = logRoot;
  nEntries = 0;
  while (ptr != NULL) {
    int pDay, pMonth, pYear;

    tJDToDate((double)((int)ptr->time), &pYear, &pMonth, &pDay);
    if ((pMonth == month) && (pYear == year))
      nEntries++;
    ptr = ptr->next;
  }
  nPages = nEntries/33 + 1;
  if (nPages > 1) {
    if (logPageOffset > nPages-1)
      logPageOffset = nPages - 1;
    if (logPageOffset < 0)
      logPageOffset = 0;
    sprintf(scratchString, " (page %d of %d)", logPageOffset+1, nPages);
    strcat(logMonthString, scratchString);
    col = -3 * logPageOffset;
  }

  if (!mainBoxInTrendStackable) {
    trendStackable = hildon_stackable_window_new();
    g_signal_connect(G_OBJECT(trendStackable), "destroy",
		     G_CALLBACK(trendStackableDestroyed), NULL);
    gtk_widget_ref(mainBox);
    gtk_widget_hide(mainBox);
    gtk_container_remove(GTK_CONTAINER(window), mainBox);
    gtk_container_add(GTK_CONTAINER(trendStackable), mainBox);
    gtk_widget_unref(mainBox);
    mainBoxInTrendStackable = TRUE;
  }
  gtk_widget_show_all(trendStackable);
  gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
		     TRUE, 0, 0,
		     drawingArea->allocation.width,
		     drawingArea->allocation.height);
  if (nPages == 1)
    titleFont = BIG_PANGO_FONT;
  else
    titleFont = MEDIUM_PANGO_FONT;
  renderPangoText(logMonthString, OR_WHITE, titleFont,
		  &tWidth, &tHeight, pixmap, displayWidth/2, 20, 0.0, TRUE, 0, OR_BLACK);
  renderPangoText(logMonthString, OR_WHITE, titleFont,
		  &tWidth, &tHeight, pixmap, displayWidth/2, 20, 0.0, TRUE, 0, OR_BLACK);

  ptr = logRoot;
  while (ptr != NULL) {
    int day, pDay, pMonth, pYear;

    tJDToDate((double)((int)ptr->time), &pYear, &pMonth, &pDay);
    if ((pMonth == month) && (pYear == year)) {
      int i;
      double trend;
      char shortDayName[4];

      day = dayOfWeek(pYear, pMonth, pDay);
      trend = calculateWeightedAverage(ptr);
      for (i = 0; i < 3; i++)
	shortDayName[i] = dayName[day][i];
      shortDayName[3] = (char)0;
      sprintf(scratchString, "%2d", pDay);
      renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 20+col*displayWidth/3,
		      70+row*30, 0.0, FALSE, 0, OR_BLACK);
      renderPangoText(shortDayName, OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 55+col*displayWidth/3,
		      70+row*30, 0.0, FALSE, 0, OR_BLACK);
      sprintf(scratchString, "%5.1f", ptr->weight);
      if (nonjudgementalColors)
	renderPangoText(scratchString, OR_BLUE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 115+col*displayWidth/3,
			70+row*30, 0.0, FALSE, 0, OR_BLACK);
      else if (ptr->weight <= trend)
	renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 115+col*displayWidth/3,
			70+row*30, 0.0, FALSE, 0, OR_BLACK);
      else
	renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 115+col*displayWidth/3,
			70+row*30, 0.0, FALSE, 0, OR_BLACK);
      sprintf(scratchString, "%5.1f", trend);
      renderPangoText(scratchString, OR_YELLOW, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 185+col*displayWidth/3,
		      70+row*30, 0.0, FALSE, 0, OR_BLACK);
      box[0].x = 17+col*displayWidth/3; box[0].y = 56+row*30;
      box[1].x = box[0].x + 234;        box[1].y = box[0].y;
      box[2].x = box[1].x;              box[2].y = box[1].y + 26;
      box[3].x = box[0].x;              box[3].y = box[2].y;
      gdk_draw_polygon(pixmap, gC[OR_DARK_GREY], FALSE, box, 4);
      box[0].x = 18+col*displayWidth/3; box[0].y = 57+row*30;
      box[1].x = box[0].x + 232;        box[1].y = box[0].y;
      box[2].x = box[1].x;              box[2].y = box[1].y + 24;
      box[3].x = box[0].x;              box[3].y = box[2].y;
      gdk_draw_polygon(pixmap, gC[OR_DARK_GREY], FALSE, box, 4);
      cellPtr = (logCell *)malloc(sizeof(logCell));
      if (cellPtr != NULL) {
	cellPtr->data = DATA_CELL;
	for (i = 0; i < 4; i++) {
	  cellPtr->box[i].x = box[i].x;
	  cellPtr->box[i].y = box[i].y;
	}
	cellPtr->ptr = ptr;
	cellPtr->next = NULL;
	if (logCellRoot == NULL) {
	  logCellRoot = cellPtr;
	} else {
	  lastLogCell->next = cellPtr;
	}
	lastLogCell = cellPtr;
      }
      row++;
      if (row > 10) {
	col++;
	row = 0;
      }
    }
    ptr = ptr->next;
  }
  if (nPages > 1) {
    /* Plot the directional arrows for navigating from page to page */
    if (logPageOffset < (nPages-1)) {
      box[0].x = displayWidth*5/6 - 10; box[0].y = 3;
      box[1].x = box[0].x;         box[1].y = 53;
      box[2].x = displayWidth-2;   box[2].y = (box[0].y + box[1].y) / 2;
      renderPangoText("Next Page", OR_BLUE, SMALL_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth*11/12 - 30, box[2].y - 1, 0.0, TRUE, 0, OR_BLACK);
      gdk_draw_polygon(pixmap, gC[OR_BLUE], FALSE, box, 3);
      cellPtr = (logCell *)malloc(sizeof(logCell));
      if (cellPtr != NULL) {
	cellPtr->box[0].x = box[0].x;     cellPtr->box[0].y = 0;
	cellPtr->box[1].x = box[0].x;     cellPtr->box[1].y = box[1].y;
	cellPtr->box[2].x = displayWidth; cellPtr->box[2].y = box[1].y;
	cellPtr->box[3].x = displayWidth; cellPtr->box[3].y = 0;
	lastLogCell->next = cellPtr;
	lastLogCell = cellPtr;
	cellPtr->next = NULL;
	cellPtr->data = RIGHT_ARROW;
      }
    }
    if (logPageOffset > 0) {
      box[0].x = displayWidth/6 + 10; box[0].y = 3;
      box[1].x = box[0].x;            box[1].y = 53;
      box[2].x = 2;                   box[2].y = (box[0].y + box[1].y) / 2;
      renderPangoText("Last Page", OR_BLUE, SMALL_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth/12 + 33, box[2].y - 1, 0.0, TRUE, 0, OR_BLACK);
      gdk_draw_polygon(pixmap, gC[OR_BLUE], FALSE, box, 3);
      cellPtr = (logCell *)malloc(sizeof(logCell));
      if (cellPtr != NULL) {
	cellPtr->box[0].x = 0;        cellPtr->box[0].y = 0;
	cellPtr->box[1].x = 0;        cellPtr->box[1].y = box[1].y;
	cellPtr->box[2].x = box[0].x; cellPtr->box[2].y = box[1].y;
	cellPtr->box[3].x = box[0].x; cellPtr->box[3].y = 0;
	lastLogCell->next = cellPtr;
	lastLogCell = cellPtr;
	cellPtr->next = NULL;
	cellPtr->data = LEFT_ARROW;
      }
    }
  }
  renderPangoText("Click on an entry, if you wish to edit or delete it.",
		  OR_BLUE, MEDIUM_PANGO_FONT,
		  &tWidth, &tHeight, pixmap, displayWidth/2, displayHeight-20,
		  0.0, TRUE, 0, OR_BLACK);
  renderPangoText("Click on an entry, if you wish to edit or delete it.",
		  OR_BLUE, MEDIUM_PANGO_FONT,
		  &tWidth, &tHeight, pixmap, displayWidth/2, displayHeight-20,
		  0.0, TRUE, 0, OR_BLACK);
} /* End of  L O G  C A L L B A C K */

/*
  L O G  B U T T O N  C L I C K E D

  This function builds and displays a selector which shows every month
  for which data has been entered.
 */
void logButtonClicked(GtkButton *button, gpointer userData)
{
  int lastMonth = -1;
  int lastYear = -1;
  int year, month, day;
  int nMonths = 0;
  char monthString[50];
  logEntry *ptr;
  static GtkWidget *logTable, *monthSelector;
  static GtkListStore *model;
  static GtkTreeIter iter;
  static HildonTouchSelectorColumn *column = NULL;

  dprintf("in logButtonClicked()\n");
  if (nDataPoints > 0) {
    logStackable = hildon_stackable_window_new();
    logTable = gtk_table_new(1, 2, FALSE);
    monthSelector = hildon_touch_selector_new();
    model = gtk_list_store_new(1, G_TYPE_STRING);
    ptr = logRoot;
    while (ptr != NULL) {
      tJDToDate(ptr->time, &year, &month, &day);
      if ((month != lastMonth) || (year != lastYear)) {
	sprintf(monthString, "%s %d\n", monthName[month-1], year);
	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter, 0, monthString, -1);
	nMonths++;
	lastYear = year;
	lastMonth = month;
      }
      ptr = ptr->next;
    }
    column = hildon_touch_selector_append_text_column(HILDON_TOUCH_SELECTOR(monthSelector),
						      GTK_TREE_MODEL(model), TRUE);
    g_object_set(G_OBJECT(column), "text-column", 0, NULL);  
    logSelectorButton = hildon_picker_button_new(HILDON_SIZE_AUTO,
						 HILDON_BUTTON_ARRANGEMENT_VERTICAL);
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(monthSelector), 0, nMonths-1);
    hildon_button_set_title(HILDON_BUTTON(logSelectorButton), "Month to examine");
    hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(logSelectorButton),
				      HILDON_TOUCH_SELECTOR(monthSelector));
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(monthSelector), 0, nMonths);
    gtk_table_attach(GTK_TABLE(logTable), logSelectorButton, 0, 1, 0, 1,
		   GTK_EXPAND, GTK_EXPAND, 0, 0);
    gtk_container_add(GTK_CONTAINER(logStackable), logTable);

    monthAccept = gtk_button_new_with_label("View Selected Month");
    g_signal_connect (G_OBJECT(monthAccept), "clicked",
		      G_CALLBACK(logCallback), NULL);
    gtk_table_attach(GTK_TABLE(logTable), monthAccept, 0, 1, 1, 2,
		     GTK_EXPAND, GTK_EXPAND, 0, 0);
    
    gtk_widget_show_all(logStackable);
  }
} /* End of  L O G  B U T T O N  C L I C K E D */

/*
  T R E N D  B U T T O N  C L I C K E D

  This function builds an displays the page which shows weight trends for
  the last week, fortnight, month etc.
 */
void trendButtonClicked(GtkButton *button, gpointer userData)
{
  int tWidth, tHeight, day, month, year, line;
  int goalYear, goalMonth, goalDay, goalHour, goalMinute;
  int nItems = 0;
  int delta = 20;
  int delta2 = 13;
  int shouldSkipFortnight = FALSE;
  float calCon;
  float startWeightW, endWeightW, maxWeightW, minWeightW, aveWeightW;
  float startWeightF, endWeightF, maxWeightF, minWeightF, aveWeightF;
  float startWeightM, endWeightM, maxWeightM, minWeightM, aveWeightM;
  float startWeightQ, endWeightQ, maxWeightQ, minWeightQ, aveWeightQ;
  float startWeightY, endWeightY, maxWeightY, minWeightY, aveWeightY;
  float startWeightH, endWeightH, maxWeightH, minWeightH, aveWeightH;
  double deltaTime, goalJD;
  GdkPoint box[4];

  dprintf("in trendButtonClicked()\n");
  if (weightkg)
    calCon = 1.0/KG_PER_LB;
  else
    calCon = 1.0;
  if (!mainBoxInTrendStackable) {
    trendStackable = hildon_stackable_window_new();
    g_signal_connect(G_OBJECT(trendStackable), "destroy",
		     G_CALLBACK(trendStackableDestroyed), NULL);
    gtk_widget_ref(mainBox);
    gtk_widget_hide(mainBox);
    gtk_container_remove(GTK_CONTAINER(window), mainBox);
    gtk_container_add(GTK_CONTAINER(trendStackable), mainBox);
    gtk_widget_unref(mainBox);
    mainBoxInTrendStackable = TRUE;
  }
  gtk_widget_show_all(trendStackable);
  gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
		     TRUE, 0, 0,
		     drawingArea->allocation.width,
		     drawingArea->allocation.height);
  renderPangoText("Trend Analysis", OR_WHITE, BIG_PANGO_FONT,
		  &tWidth, &tHeight, pixmap, displayWidth/2, 20, 0.0, TRUE, 0, OR_BLACK);
  renderPangoText("Trend Analysis", OR_WHITE, BIG_PANGO_FONT,
		  &tWidth, &tHeight, pixmap, displayWidth/2, 20, 0.0, TRUE, 0, OR_BLACK);
  if (calcStats(ONE_WEEK, &startWeightW, &endWeightW, &maxWeightW, &minWeightW, &aveWeightW))
    nItems++;
  if (calcStats(ONE_FORTNIGHT, &startWeightF, &endWeightF, &maxWeightF, &minWeightF, &aveWeightF))
    nItems++;
  if (calcStats(ONE_MONTH, &startWeightM, &endWeightM, &maxWeightM, &minWeightM, &aveWeightM))
    nItems++;
  if (calcStats(ONE_QUARTER, &startWeightQ, &endWeightQ, &maxWeightQ, &minWeightQ, &aveWeightQ))
    nItems++;
  if (calcStats(ONE_YEAR, &startWeightY, &endWeightY, &maxWeightY, &minWeightY, &aveWeightY))
    nItems++;
  if (calcStats(-1, &startWeightH, &endWeightH, &maxWeightH, &minWeightH, &aveWeightH))
    nItems++;

  if (haveGoalDate && (nItems >= 6)
      && calculateGoalDate(&goalJD, &goalYear, &goalMonth, &goalDay, &goalHour, &goalMinute,
			   NULL, NULL, NULL))
    shouldSkipFortnight = TRUE;

  box[0].x = 0;            box[0].y = 100-delta;
  box[1].x = displayWidth; box[1].y = 100-delta;
  box[2].x = displayWidth; box[2].y = displayHeight - 15 - (6-(nItems-shouldSkipFortnight))*35 - delta;
  box[3].x = 0;            box[3].y = displayHeight - 15 - (6-(nItems-shouldSkipFortnight))*35 - delta;
  gdk_draw_polygon(pixmap, gC[OR_DARK_GREY], TRUE, box, 4);

  tJDToDate((double)((int)lastEntry->time), &year, &month, &day);
  if (monthFirst == 2) {
    sprintf(scratchString, "Intervals ending %d-%02d-%02d", year, month, day);
  } else {
    if (monthFirst == 1) {
      int temp;
      
      temp = day;
      day = month;
      month = temp;
    }
    sprintf(scratchString, "Intervals ending %d/%d/%d", day, month, year);
  }
  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
		  &tWidth, &tHeight, pixmap, displayWidth/2, 70-delta2, 0.0, TRUE, 0, OR_BLACK);
  renderPangoText("Last...", OR_WHITE, MEDIUM_PANGO_FONT,
		  &tWidth, &tHeight, pixmap, 5, 140, 0.0, FALSE, 0, OR_DARK_GREY); 
  
  if (!nonjudgementalColors) {
    renderPangoText("Gain", badColor, MEDIUM_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, 134, 125-delta, 0.0, FALSE, 0, OR_DARK_GREY); 
    renderPangoText("/", OR_WHITE, MEDIUM_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, 194, 125-delta, 0.0, FALSE, 0, OR_DARK_GREY);      
    renderPangoText("Loss", goodColor, MEDIUM_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, 209, 125-delta, 0.0, FALSE, 0, OR_DARK_GREY);
    if (weightkg)
      renderPangoText("kg / week", OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 144, 155-delta, 0.0, FALSE, 0, OR_DARK_GREY);
    else
      renderPangoText("pounds / week", OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 118, 155-delta, 0.0, FALSE, 0, OR_DARK_GREY);
  } else
    if (weightkg)
      renderPangoText("kg / week", OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 144, 140-delta, 0.0, FALSE, 0, OR_DARK_GREY);
    else
      renderPangoText("pounds / week", OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 118, 140-delta, 0.0, FALSE, 0, OR_DARK_GREY);

  if (!nonjudgementalColors) {
    renderPangoText("Excess", badColor, MEDIUM_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, 336, 125-delta, 0.0, FALSE, 0, OR_DARK_GREY);      
    renderPangoText("/", OR_WHITE, MEDIUM_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, 419, 125-delta, 0.0, FALSE, 0, OR_DARK_GREY);      
    renderPangoText("Deficit", goodColor, MEDIUM_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, 433, 125-delta, 0.0, FALSE, 0, OR_DARK_GREY);      
    renderPangoText("calories / day", OR_WHITE, MEDIUM_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, 348, 155-delta, 0.0, FALSE, 0, OR_DARK_GREY);
  } else
    renderPangoText("calories / day", OR_WHITE, MEDIUM_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, 348, 140-delta, 0.0, FALSE, 0, OR_DARK_GREY);

  renderPangoText("Weight Trend", OR_WHITE, MEDIUM_PANGO_FONT,
		  &tWidth, &tHeight, pixmap, 601, 125-delta, 0.0, FALSE, 0, OR_DARK_GREY);      
  renderPangoText("Min.     Mean    Max.", OR_WHITE, MEDIUM_PANGO_FONT,
		  &tWidth, &tHeight, pixmap, 566, 155-delta, 0.0, FALSE, 0, OR_DARK_GREY);

  for (line = 0; line < nItems; line++) {
    float temp;

    if (line == (nItems-1)) {
      renderPangoText("All History", OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 5, 200+(35*(line-shouldSkipFortnight))-delta,
		      0.0, FALSE, 0, OR_DARK_GREY);
      deltaTime = lastEntry->time - logRoot->time;
      temp = (startWeightH-endWeightH)/(deltaTime/((float)ONE_WEEK));
      if (nonjudgementalColors) {
	sprintf(scratchString, "%4.2f", -temp);
	renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			0.0, FALSE, 0, OR_DARK_GREY);
      } else {
	sprintf(scratchString, "%4.2f", fabs(temp));
	if (temp < 0.0) 
	  renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
	else
	  renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
      }
      temp = calCon*(startWeightH-endWeightH)*(CALORIES_PER_POUND/deltaTime);
      if (nonjudgementalColors) {
	sprintf(scratchString, "%4.0f", -temp);
	renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			0.0, FALSE, 0, OR_DARK_GREY);
      } else {
	sprintf(scratchString, "%4.0f", fabs(temp));
	if (temp < 0.0)
	  renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
	else
	  renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
      }
      sprintf(scratchString, "%5.1f    %5.1f    %5.1f", minWeightH, aveWeightH, maxWeightH);
      renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 561, 200+(35*(line-shouldSkipFortnight))-delta,
		      0.0, FALSE, 0, OR_DARK_GREY);
    } else
      switch (line) {
      case 0:
	renderPangoText("Week", OR_WHITE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 5, 200+(35*line)-delta, 0.0,
			FALSE, 0, OR_DARK_GREY);      
	temp = startWeightW-endWeightW;
	if (nonjudgementalColors) {
	  sprintf(scratchString, "%4.2f", -temp);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 170, 200+(35*line)-delta, 0.0,
			  FALSE, 0, OR_DARK_GREY);
	} else {
	  sprintf(scratchString, "%4.2f", fabs(temp));
	  if (temp < 0.0) 
	    renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 170, 200+(35*line)-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	  else
	    renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 170, 200+(35*line)-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	}
	temp = calCon*(startWeightW-endWeightW)*(CALORIES_PER_POUND/((float)ONE_WEEK));
	if (nonjudgementalColors) {
	  sprintf(scratchString, "%4.0f", -temp);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 380, 200+(35*line)-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
	} else {
	  sprintf(scratchString, "%4.0f", fabs(temp));
	  if (temp < 0.0)
	    renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 380, 200+(35*line)-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	  else
	    renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 380, 200+(35*line)-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	}
	sprintf(scratchString, "%5.1f    %5.1f    %5.1f", minWeightW, aveWeightW, maxWeightW);
	renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 561, 200+(35*line)-delta,
			0.0, FALSE, 0, OR_DARK_GREY);
	break;
      case 1:
	if (!shouldSkipFortnight) {
	  shouldSkipFortnight = FALSE;
	  renderPangoText("Fortnight", OR_WHITE, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 5, 200+(35*line)-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);      
	  temp = (startWeightF-endWeightF)/2.0;
	  if (nonjudgementalColors) {
	    sprintf(scratchString, "%4.2f", -temp);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 170, 200+(35*line)-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	  } else {
	    sprintf(scratchString, "%4.2f", fabs(temp));
	    if (temp < 0.0) 
	      renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			      &tWidth, &tHeight, pixmap, 170, 200+(35*line)-delta,
			      0.0, FALSE, 0, OR_DARK_GREY);
	    else
	      renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			      &tWidth, &tHeight, pixmap, 170, 200+(35*line)-delta,
			      0.0, FALSE, 0, OR_DARK_GREY);
	  }
	  temp = calCon*(startWeightF-endWeightF)*(CALORIES_PER_POUND/((float)ONE_FORTNIGHT));
	  if (nonjudgementalColors) {
	    sprintf(scratchString, "%4.0f", -temp);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 380, 200+(35*line)-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	  } else {
	    sprintf(scratchString, "%4.0f", fabs(temp));
	    if (temp < 0.0)
	      renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			      &tWidth, &tHeight, pixmap, 380, 200+(35*line)-delta,
			      0.0, FALSE, 0, OR_DARK_GREY);
	    else
	      renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			      &tWidth, &tHeight, pixmap, 380, 200+(35*line)-delta,
			      0.0, FALSE, 0, OR_DARK_GREY);
	  }
	  sprintf(scratchString, "%5.1f    %5.1f    %5.1f", minWeightF, aveWeightF, maxWeightF);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 561, 200+(35*line)-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
	}
	break;
      case 2:
	renderPangoText("Month", OR_WHITE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 5, 200+(35*(line-shouldSkipFortnight))-delta,
			0.0, FALSE, 0, OR_DARK_GREY);      
	temp = (startWeightM-endWeightM)/(((float)ONE_MONTH)/((float)ONE_WEEK));
	if (nonjudgementalColors) {
	  sprintf(scratchString, "%4.2f", -temp);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
	} else {
	  sprintf(scratchString, "%4.2f", fabs(temp));
	  if (temp < 0.0) 
	    renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	  else
	    renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	}
	temp = calCon*(startWeightM-endWeightM)*(CALORIES_PER_POUND/((float)ONE_MONTH));
	if (nonjudgementalColors) {
	  sprintf(scratchString, "%4.0f", -temp);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
	} else {
	  sprintf(scratchString, "%4.0f", fabs(temp));
	  if (temp < 0.0)
	    renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	  else
	    renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	}
	sprintf(scratchString, "%5.1f    %5.1f    %5.1f", minWeightM, aveWeightM, maxWeightM);
	renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 561, 200+(35*(line-shouldSkipFortnight))-delta,
			0.0, FALSE, 0, OR_DARK_GREY);
	break;
      case 3:
	renderPangoText("Quarter", OR_WHITE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 5, 200+(35*(line-shouldSkipFortnight))-delta,
			0.0, FALSE, 0, OR_DARK_GREY);      
	temp = (startWeightQ-endWeightQ)/13.0;
	if (nonjudgementalColors) {
	  sprintf(scratchString, "%4.2f", -temp);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
	} else {
	  sprintf(scratchString, "%4.2f", fabs(temp));
	  if (temp < 0.0) 
	    renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	  else
	    renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	}
	temp = calCon*(startWeightQ-endWeightQ)*(CALORIES_PER_POUND/((float)ONE_QUARTER));
	if (nonjudgementalColors) {
	  sprintf(scratchString, "%4.0f", -temp);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
	} else {
	  sprintf(scratchString, "%4.0f", fabs(temp));
	  if (temp < 0.0)
	    renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	  else
	    renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	}
	sprintf(scratchString, "%5.1f    %5.1f    %5.1f", minWeightQ, aveWeightQ, maxWeightQ);
	renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 561, 200+(35*(line-shouldSkipFortnight))-delta,
			0.0, FALSE, 0, OR_DARK_GREY);
	break;
      case 4:
	renderPangoText("Year", OR_WHITE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 5, 200+(35*(line-shouldSkipFortnight))-delta,
			0.0, FALSE, 0, OR_DARK_GREY);      
	temp = (startWeightY-endWeightY)/(((float)ONE_YEAR)/((float)ONE_WEEK));
	if (nonjudgementalColors) {
	  sprintf(scratchString, "%4.2f", -temp);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
	} else {
	  sprintf(scratchString, "%4.2f", fabs(temp));
	  if (temp < 0.0) 
	    renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	  else
	    renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 170, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	}
	temp = calCon*(startWeightY-endWeightY)*(CALORIES_PER_POUND/((float)ONE_YEAR));
	if (nonjudgementalColors) {
	  sprintf(scratchString, "%4.0f", -temp);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			  0.0, FALSE, 0, OR_DARK_GREY);
	} else {
	  sprintf(scratchString, "%4.0f", fabs(temp));
	  if (temp < 0.0)
	    renderPangoText(scratchString, badColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	  else
	    renderPangoText(scratchString, goodColor, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight, pixmap, 380, 200+(35*(line-shouldSkipFortnight))-delta,
			    0.0, FALSE, 0, OR_DARK_GREY);
	}
	sprintf(scratchString, "%5.1f    %5.1f    %5.1f", minWeightY, aveWeightY, maxWeightY);
	renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 561, 200+(35*(line-shouldSkipFortnight))-delta,
			0.0, FALSE, 0, OR_DARK_GREY);
	break;
      }
    if (haveGoalDate) {
      int y;
      double targetTJD, daysLeft;

      targetTJD = (double)calculateJulianDate(targetYear, targetMonth, targetDay);
      daysLeft = targetTJD - lastEntry->time;
      if (daysLeft <= 0.0) {
	sprintf(scratchString, "Your goal date is in the past.");
      } else {
	double weightNow, calsPerDay;
	char unitString[4];

	weightNow = calculateWeightedAverage(lastEntry);
	if (weightkg) {
	  calsPerDay = (weightNow-myTarget)*CALORIES_PER_POUND/(daysLeft*KG_PER_LB);
	  strcpy(unitString, "kg");
	} else {
	  calsPerDay = (weightNow-myTarget)*CALORIES_PER_POUND/daysLeft;
	  strcpy(unitString, "lbs");
	}
	if (monthFirst == 2) {
	  if (calsPerDay < 0.0)
	    sprintf(scratchString, "To reach %0.0f %s by %d-%02d-%02d eat %0.0f calories/day more.",
		    myTarget, unitString, targetYear, targetMonth, targetDay, -calsPerDay);
	  else
	    sprintf(scratchString, "To reach %0.0f %s by %d-%02d-%02d eat %0.0f calories/day less.",
		    myTarget, unitString, targetYear, targetMonth, targetDay, calsPerDay);
	} else {
	  int tTargetMonth, tTargetDay;

	  tTargetMonth = targetMonth; tTargetDay = targetDay;
	  if (monthFirst == 0) {
	    int temp;
	    
	    temp = tTargetDay;
	    tTargetDay = tTargetMonth;
	    tTargetMonth = temp;
	  }
	  if (calsPerDay < 0.0)
	    sprintf(scratchString, "To reach %0.0f %s by %02d/%02d/%4d eat %0.0f calories/day more.",
		    myTarget, unitString, tTargetMonth, tTargetDay, targetYear, -calsPerDay);
	  else
	    sprintf(scratchString, "To reach %0.0f %s by %02d/%02d/%4d eat %0.0f calories/day less.",
		    myTarget, unitString, tTargetMonth, tTargetDay, targetYear, calsPerDay);
	}
      }
      if (calculateGoalDate(&goalJD, &goalYear, &goalMonth, &goalDay, &goalHour, &goalMinute,
			    NULL, NULL, NULL))
	y = displayHeight-51;
      else
	y = displayHeight-18;
      renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth/2, y, 0.0, TRUE, 0, OR_BLACK);
      renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth/2, y, 0.0, TRUE, 0, OR_BLACK);
    }
    if (calculateGoalDate(&goalJD, &goalYear, &goalMonth, &goalDay, &goalHour, &goalMinute,
			  NULL, NULL, NULL)) {
      if (goalJD < lastEntry->time)
	sprintf(scratchString, "The fit indicates you won't reach your target weight; you have passed it.");
      else {
	if (monthFirst == 2) {
	  sprintf(scratchString, "You will reach your target weight at %02d:%02d on %d-%02d-%02d.",
		  goalHour, goalMinute, goalYear, goalMonth, goalDay);
	} else {
	  if (monthFirst == 0) {
	    int temp;
	    
	    temp = goalDay;
	    goalDay = goalMonth;
	    goalMonth = temp;
	  }
	  sprintf(scratchString, "You will reach your target weight at %02d:%02d on %d/%d/%d.",
		  goalHour, goalMinute, goalMonth, goalDay, goalYear);
	}
      }
      renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth/2, displayHeight-18,
		      0.0, TRUE, 0, OR_BLACK);
      renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth/2, displayHeight-18,
		      0.0, TRUE, 0, OR_BLACK);
    }
  }
} /* End of  T R E N D  B U T T O N  C L I C K E D */

/*
  W R I T E  S E T T I N G S

  Write the user-selectable parameters into the settings file.
 */
void writeSettings(void)
{
  FILE *settings;

  settings = fopen(settingsFileName, "w");
  if (settings == NULL) {
    perror("writing settings");
    return;
  }
  fprintf(settings, "MY_HEIGHT %5.1f\n",          myHeight);
  fprintf(settings, "MY_TARGET %5.1f\n",          myTarget);
  fprintf(settings, "WEIGHT_KG %d\n",             weightkg);
  fprintf(settings, "HEIGHT_CM %d\n",             heightcm);
  fprintf(settings, "MONTH_FIRST %d\n",           monthFirst);
  fprintf(settings, "NONJUDGEMENTAL_COLORS %d\n", nonjudgementalColors);
  fprintf(settings, "HACKER_DIET_MODE %d\n",      hackerDietMode);
  fprintf(settings, "SHOW_COMMENTS %d\n",         showComments);
  fprintf(settings, "SHOW_TARGET %d\n",           showTarget);
  fprintf(settings, "PLOT_INTERVAL %d\n",         plotInterval);
  fprintf(settings, "AVE_INTERVAL %d\n",          aveInterval);
  fprintf(settings, "FIT_INTERVAL %d\n",          fitInterval);
  fprintf(settings, "FIT_TYPE %d\n",              fitType);
  fprintf(settings, "LOSING_WEIGHT_IS_GOOD %d\n", losingWeightIsGood);
  fprintf(settings, "START_WITH_DATA_ENTRY %d\n", startWithDataEntry);
  /* fprintf(settings, "INTERPOLATE_FIRST_PAIR %d\n",interpolateFirstPair); */
  fprintf(settings, "PLOT_TREND_LINE %d\n",       plotTrendLine);
  fprintf(settings, "PLOT_DATA_POINTS %d\n",      plotDataPoints);
  fprintf(settings, "PLOT_WINDOW %d\n",           plotWindow);
  fprintf(settings, "FIT_DAY %d\n",               fitDay);
  fprintf(settings, "FIT_MONTH %d\n",             fitMonth);
  fprintf(settings, "FIT_YEAR %d\n",              fitYear);
  fprintf(settings, "PLOT_START_DAY %d\n",        plotStartDay);
  fprintf(settings, "PLOT_START_MONTH %d\n",      plotStartMonth);
  fprintf(settings, "PLOT_START_YEAR %d\n",       plotStartYear);
  fprintf(settings, "PLOT_END_DAY %d\n",          plotEndDay);
  fprintf(settings, "PLOT_END_MONTH %d\n",        plotEndMonth);
  fprintf(settings, "PLOT_END_YEAR %d\n",         plotEndYear);
  fprintf(settings, "PLOT_FROM_FIRST %d\n",       plotFromFirst);
  fprintf(settings, "PLOT_TO_LAST %d\n",          plotToLast);
  fprintf(settings, "HAVE_GOAL_DATE %d\n",        haveGoalDate);
  fprintf(settings, "TARGET_DAY %d\n",            targetDay);
  fprintf(settings, "TARGET_MONTH %d\n",          targetMonth);
  fprintf(settings, "TARGET_YEAR %d\n",           targetYear);
  fclose(settings);
} /* End of  W R I T E  S E T T I N G S */

/*
  C H E C K  F I T  S E T T I N G S  S E T T I N G S

  Read the widgets from the Fit Settings page, and set the user-selectable
  variables.
*/
void checkFitSettings(void)
{
  fitType = FIT_WINDOW;
  if (GTK_TOGGLE_BUTTON(fitFromButton)->active) {
    fitInterval = -1;
    fitType = FIT_FROM_DATE;
  } else if (GTK_TOGGLE_BUTTON(fitNothingButton)->active) {
    fitInterval = -1;
    fitType = DO_NOT_FIT_DATA;
  } else if (GTK_TOGGLE_BUTTON(fitMonthButton)->active)
    fitInterval = ONE_MONTH;
  else if (GTK_TOGGLE_BUTTON(fitQuarterButton)->active)
    fitInterval = ONE_QUARTER;
  else if (GTK_TOGGLE_BUTTON(fit6MonthButton)->active)
    fitInterval =ONE_SIX_MONTHS;
  else if (GTK_TOGGLE_BUTTON(fitYearButton)->active)
    fitInterval = ONE_YEAR;
  else
    fitInterval = 1000000000;

  writeSettings();
  redrawScreen();
} /* End of  C H E C K  F I T  S E T T I N G S  */

/*
  C H E C K  P L O T  S E T T I N G S  S E T T I N G S

  Read the widgets from the Plot Settings page, and set the user-selectable
  variables.
*/
void checkPlotSettings(void)
{
  if (GTK_TOGGLE_BUTTON(showTargetButton)->active)
    showTarget = TRUE;
  else
    showTarget = FALSE;

  if (GTK_TOGGLE_BUTTON(showCommentsButton)->active)
    showComments = TRUE;
  else
    showComments = FALSE;

  if (GTK_TOGGLE_BUTTON(nonjudgementalButton)->active) {
    nonjudgementalColors = TRUE;
    goodColor = badColor = OR_BLUE;
  } else {
    nonjudgementalColors = FALSE;
    if (losingWeightIsGood) {
      goodColor = OR_GREEN;
      badColor = OR_RED;
    } else {
      goodColor = OR_RED;
      badColor = OR_GREEN;
    }
  }

  if (GTK_TOGGLE_BUTTON(plotTrendLineButton)->active)
    plotTrendLine = TRUE;
  else
    plotTrendLine = FALSE;

  if (GTK_TOGGLE_BUTTON(plotDataPointsButton)->active)
    plotDataPoints = TRUE;
  else
    plotDataPoints = FALSE;

  if (GTK_TOGGLE_BUTTON(monthButton)->active)
    monthFirst = 1;
  else if (GTK_TOGGLE_BUTTON(iSOButton)->active)
    monthFirst = 2;
  else
    monthFirst = 0;

  if (GTK_TOGGLE_BUTTON(plotFortnightButton)->active)
    plotInterval = ONE_FORTNIGHT;
  else if (GTK_TOGGLE_BUTTON(plotMonthButton)->active)
    plotInterval = ONE_MONTH;
  else if (GTK_TOGGLE_BUTTON(plotQuarterButton)->active)
    plotInterval = ONE_QUARTER;
  else if (GTK_TOGGLE_BUTTON(plot6MonthButton)->active)
    plotInterval =ONE_SIX_MONTHS;
  else if (GTK_TOGGLE_BUTTON(plotYearButton)->active)
    plotInterval = ONE_YEAR;
  else
    plotInterval = 1000000000;

  if (GTK_TOGGLE_BUTTON(aveWeekButton)->active)
    aveInterval = ONE_WEEK;
  else if (GTK_TOGGLE_BUTTON(aveFortnightButton)->active)
    aveInterval = ONE_FORTNIGHT;
  else if (GTK_TOGGLE_BUTTON(aveMonthButton)->active)
    aveInterval = ONE_MONTH;
  else if (GTK_TOGGLE_BUTTON(aveQuarterButton)->active)
    aveInterval = ONE_QUARTER;
  else if (GTK_TOGGLE_BUTTON(aveYearButton)->active)
    aveInterval = ONE_YEAR;
  else
    aveInterval = -1;

  writeSettings();
  redrawScreen();
} /* End of  C H E C K  P L O T  S E T T I N G S  */

/*
  C H E C K  A B O U T  Y O U

  Read the widgets from the About You page, and set the user-selectable
  variables.
*/
void checkAboutYou(void)
{
  guint year, month, day;
  
  dprintf("In checkAboutYou\n");
  myHeight = gtk_spin_button_get_value((GtkSpinButton *)heightSpin);
  hildon_date_button_get_date((HildonDateButton *)goalDateButton, &year, &month, &day);
  targetYear = (int)year; targetMonth = (int)month + 1; targetDay = (int)day;
  dprintf("Got goal date %d/%d/%d\n", targetMonth, targetDay, targetYear);

  myTarget = gtk_spin_button_get_value((GtkSpinButton *)targetSpin);
  if (GTK_TOGGLE_BUTTON(losingWeightIsGoodButton)->active)
    losingWeightIsGood = TRUE;
  else
    losingWeightIsGood = FALSE;
  
  if (nonjudgementalColors) {
    goodColor = badColor = OR_BLUE;
  } else {
    if (losingWeightIsGood) {
      goodColor = OR_GREEN;
      badColor = OR_RED;
    } else {
      goodColor = OR_RED;
      badColor = OR_GREEN;
    }
  }
  if (GTK_TOGGLE_BUTTON(kgButton)->active)
    weightkg = TRUE;
  else
    weightkg = FALSE;
  if (GTK_TOGGLE_BUTTON(hackerDietButton)->active)
    hackerDietMode = TRUE;
  else
    hackerDietMode = FALSE;
  if (GTK_TOGGLE_BUTTON(sWDEButton)->active)
    startWithDataEntry = TRUE;
  else
    startWithDataEntry = FALSE;
  /*
  if (GTK_TOGGLE_BUTTON(interpolateFirstPairButton)->active)
    interpolateFirstPair = TRUE;
  else
    interpolateFirstPair = FALSE;
  */
  if (GTK_TOGGLE_BUTTON(cmButton)->active)
    heightcm = TRUE;
  else
    heightcm = FALSE;
  writeSettings();
  redrawScreen();
} /* End of  C H E C K  A B O U T  Y O U  */

/*
  I  E  P A G E  D E S T R O Y E D
*/
void iEPageDestroyed(void)
{
}
/* End of  I  E  P A G E  D E S T R O Y E D  */

int setFileFormat(int check)
{
  int i;
  int sawWeight = FALSE;
  int sawDate   = FALSE;

  if (!iEHackerDietFormat) {
    if (GTK_TOGGLE_BUTTON(iECommaButton)->active)
      iECommasAreSeparators = TRUE;
    else
      iECommasAreSeparators = FALSE;
    for (i = 0; i < 4; i++) {
      if (GTK_TOGGLE_BUTTON(iEDayButton[i])->active) {
	iEFieldUse[i] = IE_FIELD_IS_DATE;
	sawDate = TRUE;
      } else if (GTK_TOGGLE_BUTTON(iETimeButton[i])->active)
	iEFieldUse[i] = IE_FIELD_IS_TIME;
      else if (GTK_TOGGLE_BUTTON(iEWeightButton[i])->active) {
	iEFieldUse[i] = IE_FIELD_IS_WEIGHT;
	sawWeight = TRUE;
      } else if (GTK_TOGGLE_BUTTON(iECommentButton[i])->active)
	iEFieldUse[i] = IE_FIELD_IS_COMMENT;
      else if (GTK_TOGGLE_BUTTON(iESkipButton[i])->active)
	iEFieldUse[i] = IE_FIELD_IS_SKIPPED;
      else
	iEFieldUse[i] = IE_FIELD_NOT_USED;
    }
    if (check && ((!sawWeight) || (!sawDate))) {
      hildon_banner_show_information(drawingArea,
				     "ignored", "Your file must have Date and Weight fields - aborting");
      return FALSE;
    }
  }
  return TRUE;
}

/*
  I  E  F I L E  C A L L B A C K

  This routine is the callback for the import/export file name selection.
*/
void iEFileCallback(void)
{
  static int firstCall = TRUE;
  static GtkWindow *window;
  GtkWidget *dialog;

  if (firstCall) {
    window = (GtkWindow *)gtk_window_new(GTK_WINDOW_POPUP);
    firstCall = FALSE;
  }
  dialog = hildon_file_chooser_dialog_new(window, GTK_FILE_CHOOSER_ACTION_SAVE);
  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), IE_DEFAULT_PATH);
  if (iEHackerDietFormat)
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), IE_DEFAULT_HD_FILE);
  else
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), IE_DEFAULT_FILE);
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
    gchar *name;

    name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    sprintf(iEFileName, "%s", name);
    dprintf("Got \"%s\" for a file name\n", iEFileName);
    gtk_button_set_label((GtkButton *)iEFileButton, (gchar *)iEFileName);
    g_free(name);
  }
  gtk_widget_destroy(dialog);
} /* End of  I  E  F I L E  C A L L B A C K */

/*
  R E A D  A  L I N E

  Read in one line from the log file.   Return TRUE if a nonzero number of
  characters are seen.
*/
int readALine(FILE *log, char *line)
{
  int inByte = 0;
  int inPtr = 0;
  int gotSomeData = FALSE;

  while ((inByte != (int)'\n') && (inByte != EOF) && (inPtr < 999)) {
    inByte = fgetc(log);
    if ((inByte != (int)'\n') && (inByte != EOF) && (inByte >= (int)' ')) {
      line[inPtr++] = (char)inByte;
      gotSomeData = TRUE;
    }
  }
  line[inPtr] = (char)0;
  return(gotSomeData);
} /* End of  R E A D  A  L I N E  */

char tokens[5][1000];

int getTokens(char *line)
{
  int i;
  int inQuotes = FALSE;
  int linePtr = 0;
  int nToken = 0;
  char separator;

  if (hackerDietMode || (iECommasAreSeparators))
    separator = ',';
  else
    separator = ' ';
  for (i = 0; i < 5; i++)
    tokens[i][0] = (char)0;

  i = 0;
  while ((nToken < 5) && (line[linePtr] != (char)0)) {
    if (line[linePtr] != '"') {
      if ((line[linePtr] != separator) || inQuotes) {
	tokens[nToken][i++] = line[linePtr];
      } else {
	tokens[nToken][i] = (char)0;
	i = 0;
	nToken++;
      }
    } else if (inQuotes)
      inQuotes = FALSE;
    else
      inQuotes = TRUE;
    linePtr++;
  }
  if (i > 0) {
    tokens[nToken][i] = (char)0;
    nToken++;
  }
  return(nToken);
}

logEntry *newRoot;

/*
  I  E  I M P O R T  C A L L B A C K

  This function reads in a weight log in the specified format.
*/
void iEImportCallback(void)
{
  char *comment= NULL;
  int i, nTokens;
  int lineNumber = 0;
  float weight;
  char inLine[1000];
  logEntry *newEntry, *lastNewEntry;
  FILE *logFile;

  newRoot = lastNewEntry = NULL;
  if (setFileFormat(TRUE)) {
    logFile = fopen(iEFileName, "r");
    if (logFile == NULL) {
      sprintf(scratchString, "Could not open \"%s\" - aborting", iEFileName);
      hildon_banner_show_information(drawingArea, "ignored", scratchString);
      return;
    }
    if (iEHackerDietFormat) {
      while (readALine(logFile, inLine)) {
	int day, month, year;
	int nRead;

	lineNumber++;
	i = 0;
	nTokens = getTokens(inLine);
	if (nTokens >= 3) {
	  int goodEntry = TRUE;
	  nRead = sscanf(tokens[0], "%d-%d-%d", &year, &month, &day);
	  if (nRead == 3) {
	    nRead = sscanf(tokens[1], "%f", &weight);
	    if (nRead == 1) {
	      if (nTokens == 5) {
		if (strlen(tokens[4]) > 0) {
		  comment = malloc(strlen(tokens[4]) + 1);
		  if (comment != NULL)
		    strcpy(comment, tokens[4]);
		}
	      } else
		comment = NULL;
	    } else
	      goodEntry = FALSE;
	  } else
	    goodEntry = FALSE;
	  if (goodEntry) {
	    newEntry = (logEntry *)malloc(sizeof(logEntry));
	    if (newEntry == NULL) {
	      perror("malloc of newEntry");
	      exit(ERROR_EXIT);
	    }
	    newEntry->time = (double)calculateJulianDate(year, month, day);
	    newEntry->weight = weight;
	    newEntry->comment = comment;
	    newEntry->next = NULL;
	    if (lastNewEntry == NULL) {
	      newRoot = newEntry;
	      newEntry->last = NULL;
	    } else {
	      lastNewEntry->next = newEntry;
	      newEntry->last = lastNewEntry;
	    }
	    lastNewEntry = newEntry;
	  }
	} else {
	  char message[200];

	  sprintf(message, "Line %d is not in Hacker Diet format", lineNumber);
	  hildon_banner_show_information(drawingArea, "ignored", message);
	}
      }
    } else {
      /* Not Hacker Diet format */
      while (readALine(logFile, inLine)) {
	int day, month, year, hH, mM;
	int nRead, gotDate, gotWeight, gotComment;
	double newJD;
	char *comment = NULL;
	
	lineNumber++;
	gotDate = gotWeight = gotComment = FALSE;
	newJD = 0.0;
	i = 0;
	nTokens = getTokens(inLine);
	while (i < nTokens) {
	  switch (iEFieldUse[i]) {
	  case IE_FIELD_IS_DATE:
	    nRead = sscanf(tokens[i], "%d/%d/%d", &month, &day, &year);
	    if ((nRead == 3) && (month > 0) && (month < 13)
		&& (day > 0) && (day < 32) && (year > 1900) && (year < 2100)) {
	      newJD += (double)calculateJulianDate(year, month, day);
	      gotDate = TRUE;
	    } else {
	      char message[200];
	      
	      sprintf(message, "Line %d has a bad date field", lineNumber);
	      hildon_banner_show_information(drawingArea, "ignored", message);
	    }
	    break;
	  case IE_FIELD_IS_TIME:
	    nRead = sscanf(tokens[i], "%d:%d", &hH, &mM);
	    if ((nRead == 2) && (hH >= 0) && (hH < 24) && (mM >= 0) && (mM < 60))
	      newJD += ((double)hH)/24.0 + ((double)mM)/1440.0;
	    else {
	      char message[200];
	      
	      sprintf(message, "Line %d has a bad time field", lineNumber);
	      hildon_banner_show_information(drawingArea, "ignored", message);
	    }
	    break;
	  case IE_FIELD_IS_WEIGHT:
	    nRead = sscanf(tokens[i], "%f", &weight);
	    if ((nRead == 1) && (weight > 0.0) && (weight < 1000.0)) {
	      gotWeight = TRUE;
	    } else {
	      char message[200];
	      
	      sprintf(message, "Line %d has a bad weight field (%f)", lineNumber, weight);
	      hildon_banner_show_information(drawingArea, "ignored", message);
	    }
	    break;
	  case IE_FIELD_IS_COMMENT:
	    comment = malloc(strlen(tokens[i])+1);
	    if (comment == NULL) {
	      perror("Malloc on comment in iEImport");
	      exit(ERROR_EXIT);
	    }
	    strcpy(comment, tokens[i]);
	    gotComment = TRUE;
	  }
	  i++;
	}
	if (gotDate && gotWeight) {
	  newEntry = (logEntry *)malloc(sizeof(logEntry));
	  if (newEntry == NULL) {
	    perror("malloc of newEntry");
	    exit(ERROR_EXIT);
	  }
	  newEntry->time = newJD;
	  newEntry->weight = weight;
	  if (gotComment)
	    newEntry->comment = comment;
	  else
	    newEntry->comment = NULL;
	  newEntry->next = NULL;
	  if (lastNewEntry == NULL) {
	    newRoot = newEntry;
	    newEntry->last = NULL;
	  } else {
	    lastNewEntry->next = newEntry;
	    newEntry->last = lastNewEntry;
	  }
	  lastNewEntry = newEntry;
	} else {
	  char message[200];
	  
	  sprintf(message, "Line %d does not contain a date and weight", lineNumber);
	  hildon_banner_show_information(drawingArea, "ignored", message);
	}
      }
    }
    /* The following code is called for any mode */
    if (newRoot != NULL) {
      logEntry *ptr, *lPtr;

      /* We got some usable new data - make it the new data set */
      /* First, delete the old data set */
      ptr = lastEntry;
      while (ptr != NULL) {
	lPtr = ptr->last;
	free(ptr);
	ptr = lPtr;
      }
      /* Now make the temporary data set into the permanent one */
      logRoot = newRoot;
      lastEntry = lastNewEntry;
      writeDataFile();
      redrawScreen();
      hildon_banner_show_information(drawingArea, "ignored", "Data successfully imported");
    }
  }
} /* End of  I E  I M P O R T  C A L L B A C K  */

/*
  I  E  E X P O R T  C A L L B A C K

  This function writes out the weight log into a file with the user-specified format.
 */
void iEExportCallback(void)
{
  logEntry *ptr;
  FILE *logFile;

  if (setFileFormat(TRUE)) {
    logFile = fopen(iEFileName, "w");
    if (logFile == NULL) {
      sprintf(scratchString, "Could not open \"%s\" - aborting", iEFileName);
      hildon_banner_show_information(drawingArea, "ignored", scratchString);
      return;
    }
    ptr = logRoot;
    while (ptr != NULL) {
      int i, somethingPrinted, hH, mM, year, month, day;
      double hours;
      
      somethingPrinted = FALSE;
      if (iEHackerDietFormat) {
	tJDToDate(ptr->time, &year, &month, &day);
	if (ptr->comment == NULL)
	  fprintf(logFile, "%d-%02d-%02d,%4.1f,1,0,\n",
		  year, month, day, ptr->weight);
	else
	  fprintf(logFile, "%d-%02d-%02d,%4.1f,1,0,\"%s\"\n",
		  year, month, day, ptr->weight, ptr->comment);
      } else {
	for (i = 0; i < 4; i++) {
	  switch (iEFieldUse[i]) {
	  case IE_FIELD_IS_DATE:
	    if (somethingPrinted) {
	      if (iECommasAreSeparators)
		fprintf(logFile, ",");
	      else
		fprintf(logFile, " ");
	    }
	    tJDToDate((double)((int)ptr->time), &year, &month, &day);
	    switch (monthFirst) {
	    case 1:
	      fprintf(logFile, "%02d/%02d/%d", month, day, year);
	      break;
	    case 2:
	      fprintf(logFile, "%d-%02d-%02d", year, month, day);
	      break;
	    default:
	      fprintf(logFile, "%02d/%02d/%d", day, month, year);
	      break;
	    }
	    somethingPrinted = TRUE;
	    break;
	  case IE_FIELD_IS_TIME:
	    if (somethingPrinted) {
	      if (iECommasAreSeparators)
		fprintf(logFile, ",");
	      else
		fprintf(logFile, " ");
	    }
	    hours = (ptr->time - (double)((int)ptr->time))*24.0;
	    hH = (int)hours;
	    mM = (int)((hours - (double)hH)*60.0 + 0.5);
	    fprintf(logFile, "%02d:%02d", hH, mM);
	    somethingPrinted = TRUE;
	    break;
	  case IE_FIELD_IS_WEIGHT:
	    if (somethingPrinted) {
	      if (iECommasAreSeparators)
		fprintf(logFile, ",");
	      else
		fprintf(logFile, " ");
	    }
	    fprintf(logFile, "%5.1f", ptr->weight);
	    somethingPrinted = TRUE;
	    break;
	  case IE_FIELD_IS_COMMENT:
	    if (ptr->comment != NULL) {
	      if (somethingPrinted) {
		if (iECommasAreSeparators)
		  fprintf(logFile, ",");
		else
		  fprintf(logFile, " ");
	      }
	      fprintf(logFile, "\"%s\"", ptr->comment);
	      somethingPrinted = TRUE;
	    }
	    break;
	  }
	}
	fprintf(logFile, "\n");
      }
      ptr = ptr->next;
    }
    fclose(logFile);
    hildon_banner_show_information(drawingArea, "ignored", "Export file written");
  }
} /* End of  I  E  E X P O R T  C A L L B A C K */

/*
  S E T  P L O T  S E N S I T I V I T I E S
  
 This routine is called to set the button sensitivities for the buttons on the
 Plot Settings page which select the time range for the main data plot.
*/
void setPlotSensitivities(void)
{
  if (!plotWindow && !plotFromFirst)
    gtk_widget_set_sensitive(plotStartButton,    TRUE);
  else
    gtk_widget_set_sensitive(plotStartButton,    FALSE);
  if (!plotWindow && !plotToLast)
    gtk_widget_set_sensitive(plotEndButton,      TRUE);
  else
    gtk_widget_set_sensitive(plotEndButton,      FALSE);
  gtk_widget_set_sensitive(plotFromFirstButton, !plotWindow);
  gtk_widget_set_sensitive(plotToLastButton,    !plotWindow);
  gtk_widget_set_sensitive(plotFortnightButton,  plotWindow);
  gtk_widget_set_sensitive(plotMonthButton,      plotWindow);
  gtk_widget_set_sensitive(plotQuarterButton,    plotWindow);
  gtk_widget_set_sensitive(plot6MonthButton,     plotWindow);
  gtk_widget_set_sensitive(plotYearButton,       plotWindow);
  gtk_widget_set_sensitive(plotHistoryButton,    plotWindow);
} /* End of  S E T  P L O T  S E N S I T I V I T I E S */

/*
  S E T  F I T  S E N S I T I V I T I E S
  
 This routine is called to set the button sensitivities for the buttons on the
 Fit Settings page which select the time range for the linear least squares fit
 to the weight data.
*/
void setFitSensitivities(void)
{
  gtk_widget_set_sensitive(fitWindowButton,        TRUE);
  gtk_widget_set_sensitive(fitFromButton,          TRUE);
  gtk_widget_set_sensitive(fitNothingButton,       TRUE);
  gtk_widget_set_sensitive(fitDateButton,     fitType == FIT_FROM_DATE);
  gtk_widget_set_sensitive(fitMonthButton,    fitType == FIT_WINDOW);
  gtk_widget_set_sensitive(fitQuarterButton,  fitType == FIT_WINDOW);
  gtk_widget_set_sensitive(fit6MonthButton,   fitType == FIT_WINDOW);
  gtk_widget_set_sensitive(fitYearButton,     fitType == FIT_WINDOW);
  gtk_widget_set_sensitive(fitHistoryButton,  fitType == FIT_WINDOW);
} /* End of  S E T  F I T  S E N S I T I V I T I E S */

/*
  S E T  G O A L  S E N S I T I V I T I E S
  
 This routine is called to set the button sensitivities for the buttons on the
 goal settings page.
*/
void setGoalSensitivities(void)
{
  haveGoalDate = GTK_TOGGLE_BUTTON(setGoalDateButton)->active;
  gtk_widget_set_sensitive(goalDateButton, haveGoalDate);
  writeSettings();
} /* End of  S E T  G O A L  S E N S I T I V I T I E S */

/*
  S E T  I  E  S E N S I T I V I T I E S

  This function sets the format selector buttons sensitive/insensitive depending
  upon whether or not we're in Hacker Diet Format mode.
 */
void setIESensitivities(void)
{
  int i;

  gtk_widget_set_sensitive(iECommaButton,          !iEHackerDietFormat);
  gtk_widget_set_sensitive(iEWhitespaceButton,     !iEHackerDietFormat);
  gtk_widget_set_sensitive(iESeparator,          !iEHackerDietFormat);
  for (i = 0; i < 4; i++) {
    gtk_widget_set_sensitive(iEFieldLabel[i],    !iEHackerDietFormat);
    gtk_widget_set_sensitive(iEDayButton[i],     !iEHackerDietFormat);
    gtk_widget_set_sensitive(iETimeButton[i],    !iEHackerDietFormat);
    gtk_widget_set_sensitive(iEDayButton[i],     !iEHackerDietFormat);
    gtk_widget_set_sensitive(iEWeightButton[i],  !iEHackerDietFormat);
    gtk_widget_set_sensitive(iECommentButton[i], !iEHackerDietFormat);
    gtk_widget_set_sensitive(iESkipButton[i],    !iEHackerDietFormat);
    gtk_widget_set_sensitive(iENoneButton[i],    !iEHackerDietFormat);
  }
  if (iEHackerDietFormat)
    sprintf(iEFileName, "%s%s", IE_DEFAULT_PATH, IE_DEFAULT_HD_FILE);
  else
    sprintf(iEFileName, "%s%s", IE_DEFAULT_PATH, IE_DEFAULT_FILE);
  gtk_button_set_label((GtkButton *)iEFileButton, iEFileName);
} /* End of  S E T  I  E  S E N S I T I V I T I E S */

void iEHackerDietFormatButtonCallback(void)
{
  iEHackerDietFormat = TRUE;
  setIESensitivities();
}

void iECustomFormatButtonCallback(void)
{
  iEHackerDietFormat = FALSE;
  setIESensitivities();
}

void fitWindowButtonCallback(void)
{
  fitType = FIT_WINDOW;
  setFitSensitivities();
}

void fitNothingButtonCallback(void)
{
  fitType = DO_NOT_FIT_DATA;
  setFitSensitivities();
}

void plotWindowButtonCallback(void)
{
  plotWindow = TRUE;
  setPlotSensitivities();
}

void setGoalDateButtonCallback(void)
{
  setGoalSensitivities();
}

void fitFromButtonCallback(void)
{
  fitType = FIT_FROM_DATE;
  setFitSensitivities();
}

void plotFromButtonCallback(void)
{
  plotWindow = FALSE;
  setPlotSensitivities();
}

void plotFromFirstButtonCallback(void)
{
  if (GTK_TOGGLE_BUTTON(plotFromFirstButton)->active)
    plotFromFirst = TRUE;
  else
    plotFromFirst = FALSE;
  setPlotSensitivities();
}

void plotToLastButtonCallback(void)
{
  if (GTK_TOGGLE_BUTTON(plotToLastButton)->active)
    plotToLast = TRUE;
  else
    plotToLast = FALSE;
  setPlotSensitivities();
}

/*
  I  E  B U T T O N  C L I C K E D

  This function builds and displays the page which allows the user to import
  or export their weight data as a comma separated list.
 */
void iEButtonClicked(GtkButton *button, gpointer userData)
{
  static int firstCall = TRUE;
  int i;
  static GtkWidget *iETable;
  static GSList *iEFormatGroup, *iESeparatorGroup, *iEFieldGroup[4];

  dprintf("in iEButtonClicked()\n");
  if (firstCall) {
    if (iEHackerDietFormat)
      sprintf(iEFileName, "%s%s", IE_DEFAULT_PATH, IE_DEFAULT_HD_FILE);
    else
      sprintf(iEFileName, "%s%s", IE_DEFAULT_PATH, IE_DEFAULT_FILE);
    firstCall = FALSE;
  }
  iETable = gtk_table_new(8, 7, FALSE);
  iEHackerDietFormatButton = gtk_radio_button_new_with_label(NULL, "Hacker Diet Format");
  iEFormatGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(iEHackerDietFormatButton));
  iECustomFormatButton = gtk_radio_button_new_with_label(iEFormatGroup, "Custom Format");
  iEFormatGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(iECustomFormatButton));
  if (iEHackerDietFormat)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iEHackerDietFormatButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iECustomFormatButton), TRUE);
  gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iEHackerDietFormatButton, 0, 3, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iECustomFormatButton, 4, 7, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  g_signal_connect (G_OBJECT(iEHackerDietFormatButton), "clicked",
		    G_CALLBACK(iEHackerDietFormatButtonCallback), NULL);
  g_signal_connect (G_OBJECT(iECustomFormatButton), "clicked",
		    G_CALLBACK(iECustomFormatButtonCallback), NULL);

  iESeparator = gtk_label_new("Separator:");
  gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iESeparator, 0, 1, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  iECommaButton = gtk_radio_button_new_with_label(NULL, "comma");
  iESeparatorGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(iECommaButton));
  iEWhitespaceButton = gtk_radio_button_new_with_label(iESeparatorGroup, "space");
  iESeparatorGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(iEWhitespaceButton));
  if (iECommasAreSeparators)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iECommaButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iEWhitespaceButton), TRUE);
  gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iECommaButton, 1, 2, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iEWhitespaceButton, 2, 3, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  
  for (i = 0; i < 4; i++) {
    sprintf(scratchString, "Field %d:", i+1);
    iEFieldLabel[i] = gtk_label_new(scratchString);
    gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iEFieldLabel[i], 0, 1, i+2, i+3,
		     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
   iEDayButton[i] = gtk_radio_button_new_with_label(NULL, "date");
   iEFieldGroup[i] = gtk_radio_button_group(GTK_RADIO_BUTTON(iEDayButton[i]));
   iETimeButton[i] = gtk_radio_button_new_with_label(iEFieldGroup[i], "time");
   iEFieldGroup[i] = gtk_radio_button_group(GTK_RADIO_BUTTON(iETimeButton[i]));
   iEWeightButton[i] = gtk_radio_button_new_with_label(iEFieldGroup[i], "Weight");
   iEFieldGroup[i] = gtk_radio_button_group(GTK_RADIO_BUTTON(iEWeightButton[i]));
   iECommentButton[i] = gtk_radio_button_new_with_label(iEFieldGroup[i], "Comment");
   iEFieldGroup[i] = gtk_radio_button_group(GTK_RADIO_BUTTON(iECommentButton[i]));
   iESkipButton[i] = gtk_radio_button_new_with_label(iEFieldGroup[i], "Skip");
   iEFieldGroup[i] = gtk_radio_button_group(GTK_RADIO_BUTTON(iESkipButton[i]));
   iENoneButton[i] = gtk_radio_button_new_with_label(iEFieldGroup[i], "None");
   iEFieldGroup[i] = gtk_radio_button_group(GTK_RADIO_BUTTON(iENoneButton[i]));
   switch (iEFieldUse[i]) {
   case IE_FIELD_IS_DATE:
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iEDayButton[i]), TRUE);
     break;
   case IE_FIELD_IS_TIME:
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iETimeButton[i]), TRUE);
     break;
   case IE_FIELD_IS_WEIGHT:
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iEWeightButton[i]), TRUE);
     break;
   case IE_FIELD_IS_COMMENT:
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iECommentButton[i]), TRUE);
     break;
   case IE_FIELD_IS_SKIPPED:
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iESkipButton[i]), TRUE);
     break;
   default:
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iENoneButton[i]), TRUE);
   }
    gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iEDayButton[i], 1, 2, i+2, i+3,
		     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iETimeButton[i], 2, 3, i+2, i+3,
		     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iEWeightButton[i], 3, 4, i+2, i+3,
		     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iECommentButton[i], 4, 5, i+2, i+3,
		     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iESkipButton[i], 5, 6, i+2, i+3,
		     GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    if (i > 1)
      gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iENoneButton[i], 6, 7, i+2, i+3,
		       GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  }

  iEFileLabel = gtk_label_new("File:");
  gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iEFileLabel, 0, 1, i+2, i+3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  iEFileButton = gtk_button_new_with_label(iEFileName);
  g_signal_connect (G_OBJECT(iEFileButton), "clicked",
		    G_CALLBACK(iEFileCallback), NULL);
  gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iEFileButton, 1, 7, i+2, i+3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  i++;
  iEImportButton = gtk_button_new_with_label("Import Data");
  g_signal_connect (G_OBJECT(iEImportButton), "clicked",
		    G_CALLBACK(iEImportCallback), NULL);
  gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iEImportButton, 0, 3, i+2, i+3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  iEExportButton = gtk_button_new_with_label("Export Data");
  g_signal_connect (G_OBJECT(iEExportButton), "clicked",
		    G_CALLBACK(iEExportCallback), NULL);
  gtk_table_attach(GTK_TABLE(iETable), (GtkWidget *)iEExportButton, 4, 7, i+2, i+3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  iEStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(iEStackable), "destroy",
		   G_CALLBACK(iEPageDestroyed), NULL);
  gtk_container_add(GTK_CONTAINER(iEStackable), iETable);
  setIESensitivities();
  gtk_widget_show_all(iEStackable);
} /* End of  I  E  B U T T O N  C L I C K E D */

void goalDateButtonCallback(void)
{
  dprintf("I'm in goalDateButtonCallback()\n");
  writeSettings();
}

void plotStartButtonCallback(void)
{
  static int ignore = FALSE;
  guint year, month, day;

  if (ignore) {
    dprintf("Ignoring one call to plotStartButtonCallback\n");
    ignore = FALSE;
  } else {
    dprintf("I'm in plotStartButtonCallback()\n");
    hildon_date_button_get_date((HildonDateButton *)plotStartButton, &year, &month, &day);
    if (logRoot == NULL) {
      hildon_banner_show_information(drawingArea, "ignored", "Please enter some data before plotting");
    } else {
      if (calculateJulianDate((int)year, (int)month, (int)day) < (int)logRoot->time) {
	int firstDay, firstMonth, firstYear;
	char message[200];
	
	tJDToDate(logRoot->time, &firstYear, &firstMonth, &firstDay);
	sprintf(message, "Plot start date set to first entry date (%d/%d/%d)",
		firstMonth, firstDay, firstYear);
	hildon_banner_show_information(drawingArea, "ignored", message);
	plotStartYear = firstYear; plotStartMonth = firstMonth; plotStartDay = firstDay;
	ignore = TRUE;
	hildon_date_button_set_date((HildonDateButton *)plotStartButton,
				    plotStartYear, plotStartMonth-1, plotStartDay);
      } else if (calculateJulianDate((int)year, (int)month, (int)day) > (int)lastEntry->time) {
	int lastDay, lastMonth, lastYear;
	char message[200];
	
	tJDToDate(lastEntry->time, &lastYear, &lastMonth, &lastDay);
	sprintf(message, "Plot start date set to last entry date (%d/%d/%d)",
		lastMonth, lastDay, lastYear);
	hildon_banner_show_information(drawingArea, "ignored", message);
	plotStartYear = lastYear; plotStartMonth = lastMonth; plotStartDay = lastDay;
	ignore = TRUE;
	hildon_date_button_set_date((HildonDateButton *)plotStartButton,
				    plotStartYear, plotStartMonth-1, plotStartDay);
      } else {
	plotStartYear = (int)year; plotStartMonth = (int)month + 1; plotStartDay = (int)day;
	dprintf("Got plot start date %d/%d/%d\n", plotStartMonth, plotStartDay, plotStartYear);
      }
    }
  }
}

void plotEndButtonCallback(void)
{
  static int ignore = FALSE;
  guint year, month, day;

  if (ignore) {
    dprintf("Ignoring one call to plotEndButtonCallback\n");
    ignore = FALSE;
  } else {
    dprintf("I'm in plotEndButtonCallback()\n");
    hildon_date_button_get_date((HildonDateButton *)plotEndButton, &year, &month, &day);
    if (logRoot == NULL) {
      hildon_banner_show_information(drawingArea, "ignored", "Please enter some data before plotting");
    } else {
      if (calculateJulianDate((int)year, (int)month, (int)day) < (int)logRoot->time) {
	int firstDay, firstMonth, firstYear;
	char message[200];
	
	tJDToDate(logRoot->time, &firstYear, &firstMonth, &firstDay);
	sprintf(message, "Plot start date set to first entry date (%d/%d/%d)",
		firstMonth, firstDay, firstYear);
	hildon_banner_show_information(drawingArea, "ignored", message);
	plotEndYear = firstYear; plotEndMonth = firstMonth; plotEndDay = firstDay;
	ignore = TRUE;
	hildon_date_button_set_date((HildonDateButton *)plotEndButton,
				    plotEndYear, plotEndMonth-1, plotEndDay);
      } else if (calculateJulianDate((int)year, (int)month, (int)day) > (int)lastEntry->time) {
	int lastDay, lastMonth, lastYear;
	char message[200];
	
	tJDToDate(lastEntry->time, &lastYear, &lastMonth, &lastDay);
	sprintf(message, "Plot start date set to last entry date (%d/%d/%d)",
		lastMonth, lastDay, lastYear);
	hildon_banner_show_information(drawingArea, "ignored", message);
	plotEndYear = lastYear; plotEndMonth = lastMonth; plotEndDay = lastDay;
	ignore = TRUE;
	hildon_date_button_set_date((HildonDateButton *)plotEndButton,
				    plotEndYear, plotEndMonth-1, plotEndDay);
      } else {
	plotEndYear = (int)year; plotEndMonth = (int)month + 1; plotEndDay = (int)day;
	dprintf("Got plot start date %d/%d/%d\n", plotEndMonth, plotEndDay, plotEndYear);
      }
    }
  }
}

void fitDateButtonCallback(void)
{
  static int ignore = FALSE;
  guint year, month, day;

  if (ignore) {
    dprintf("Ignoring one call to fitDateButtonCallback\n");
    ignore = FALSE;
  } else {
    dprintf("I'm in fitDateButtonCallback()\n");
    hildon_date_button_get_date((HildonDateButton *)fitDateButton, &year, &month, &day);
    if (logRoot == NULL) {
      hildon_banner_show_information(drawingArea, "ignored", "Please enter some data before fitting");
    } else {
      if (calculateJulianDate((int)year, (int)month, (int)day) < (int)logRoot->time) {
	int firstDay, firstMonth, firstYear;
	char message[200];
	
	tJDToDate(logRoot->time, &firstYear, &firstMonth, &firstDay);
	sprintf(message, "Fit start date set to first entry date (%d/%d/%d)",
		firstMonth, firstDay, firstYear);
	hildon_banner_show_information(drawingArea, "ignored", message);
	fitYear = firstYear; fitMonth = firstMonth; fitDay = firstDay;
	ignore = TRUE;
	hildon_date_button_set_date((HildonDateButton *)fitDateButton, fitYear, fitMonth-1, fitDay);
      } else if (calculateJulianDate((int)year, (int)month, (int)day) > (int)lastEntry->time) {
	int lastDay, lastMonth, lastYear;
	char message[200];
	
	tJDToDate(lastEntry->time, &lastYear, &lastMonth, &lastDay);
	sprintf(message, "Fit start date set to last entry date (%d/%d/%d)",
		lastMonth, lastDay, lastYear);
	hildon_banner_show_information(drawingArea, "ignored", message);
	fitYear = lastYear; fitMonth = lastMonth; fitDay = lastDay;
	ignore = TRUE;
	hildon_date_button_set_date((HildonDateButton *)fitDateButton, fitYear, fitMonth-1, fitDay);
      } else {
	fitYear = (int)year; fitMonth = (int)month + 1; fitDay = (int)day;
	dprintf("Got fit date %d/%d/%d\n", fitMonth, fitDay, fitYear);
      }
    }
  }
}

/*
  P L O T  S E T T I N G S  B U T T O N  C L I C K E D

  This function builds and displays the widgets for the Plot Settings
  page.
 */
void plotSettingsButtonClicked(GtkButton *button, gpointer userData)
{
  static GtkWidget *plotSettingsTable, *firstSeparator, *secondSeparator,
    *thirdSeparator, *fourthSeparator;
  static GSList *dateGroup, *plotGroup, *intervalGroup, *aveGroup;
  
  dprintf("in plotSettingsClicked()\n");
  plotSettingsTable = gtk_table_new(9, 17, FALSE);

  showTargetButton = gtk_check_button_new_with_label("Plot Target Weight");
  if (showTarget)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showTargetButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showTargetButton), FALSE);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), showTargetButton, 0, 3, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  showCommentsButton = gtk_check_button_new_with_label("Plot Comments");
  if (showComments)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showCommentsButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showCommentsButton), FALSE);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), showCommentsButton, 4, 7, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  nonjudgementalButton = gtk_check_button_new_with_label("Nonjudgemental Colors");
  if (nonjudgementalColors)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nonjudgementalButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nonjudgementalButton), FALSE);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), nonjudgementalButton, 0, 3, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  plotTrendLineButton = gtk_check_button_new_with_label("Plot Trend Line");
  if (plotTrendLine)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotTrendLineButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotTrendLineButton), FALSE);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotTrendLineButton, 4, 7, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  plotDataPointsButton = gtk_check_button_new_with_label("Plot Data Points");
  if (plotDataPoints)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotDataPointsButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotDataPointsButton), FALSE);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotDataPointsButton, 8, 11, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  firstSeparator = gtk_separator_menu_item_new();
  gtk_table_attach(GTK_TABLE(plotSettingsTable), firstSeparator, 0, 12, 2, 3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  dayButton = gtk_radio_button_new_with_label(NULL, "dd/mm/yyyy");
  dateGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(dayButton));
  monthButton = gtk_radio_button_new_with_label(dateGroup, "mm/dd/yyyy");
  dateGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(monthButton));
  iSOButton = gtk_radio_button_new_with_label(dateGroup, "yyyy-mm-dd");
  dateGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(iSOButton));
  switch (monthFirst) {
  case 1:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(monthButton), TRUE);
    break;
  case 2:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iSOButton), TRUE);
    break;
  default:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dayButton), TRUE);
  }
  gtk_table_attach(GTK_TABLE(plotSettingsTable), (GtkWidget *)dayButton, 0, 3, 3, 4,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), (GtkWidget *)monthButton, 4, 7, 3, 4,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), (GtkWidget *)iSOButton, 8, 12, 3, 4,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  secondSeparator = gtk_separator_menu_item_new();
  gtk_table_attach(GTK_TABLE(plotSettingsTable), secondSeparator, 0, 12, 4, 5,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  /* Set up a radio box button group to select the plot interval */
  plotWindowButton = gtk_radio_button_new_with_label(NULL, "Plot Interval");
  intervalGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(plotWindowButton));
  g_signal_connect(G_OBJECT(plotWindowButton), "clicked",
		   G_CALLBACK(plotWindowButtonCallback), NULL);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotWindowButton, 0, 3, 5, 6,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  plotFortnightButton = gtk_radio_button_new_with_label(NULL, "Plot Last Fortnight");
  plotGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(plotFortnightButton));
  plotMonthButton = gtk_radio_button_new_with_label(plotGroup, "Plot Last Month");
  plotGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(plotMonthButton));
  plotQuarterButton = gtk_radio_button_new_with_label(plotGroup, "Plot Last Quarter");
  plotGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(plotQuarterButton));
  plot6MonthButton = gtk_radio_button_new_with_label(plotGroup, "Plot Last 6 Months");
  plotGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(plot6MonthButton));
  plotYearButton = gtk_radio_button_new_with_label(plotGroup, "Plot Last Year");
  plotGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(plotYearButton));
  plotHistoryButton = gtk_radio_button_new_with_label(plotGroup, "Plot Entire History");
  plotGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(plotHistoryButton));
  switch (plotInterval) {
  case ONE_FORTNIGHT:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotFortnightButton), TRUE);
    break;
  case ONE_MONTH:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotMonthButton), TRUE);
    break;
  case ONE_QUARTER:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotQuarterButton), TRUE);
    break;
  case ONE_SIX_MONTHS:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plot6MonthButton), TRUE);
    break;
  case ONE_YEAR:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotYearButton), TRUE);
    break;
  default:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotHistoryButton), TRUE);
  }
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotFortnightButton, 0, 3, 6, 7,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotMonthButton, 4, 7, 6, 7,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotQuarterButton, 8, 12, 6, 7,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plot6MonthButton, 0, 3, 7, 8,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotYearButton, 4, 7, 7, 8,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotHistoryButton, 8, 12, 7, 8,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  thirdSeparator = gtk_separator_menu_item_new();
  gtk_table_attach(GTK_TABLE(plotSettingsTable), thirdSeparator, 0, 12, 8, 9,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  plotFromButton = gtk_radio_button_new_with_label(intervalGroup, "Plot Date Range");
  intervalGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(plotFromButton));
  g_signal_connect (G_OBJECT(plotFromButton), "clicked",
		    G_CALLBACK(plotFromButtonCallback), NULL);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotFromButton, 0, 3, 9, 10,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  plotFromFirstButton = gtk_check_button_new_with_label("From First Entry");
  if (plotFromFirst)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotFromFirstButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotFromFirstButton), FALSE);
  g_signal_connect (G_OBJECT(plotFromFirstButton), "clicked",
		    G_CALLBACK(plotFromFirstButtonCallback), NULL);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotFromFirstButton, 4, 7, 9, 10,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  plotToLastButton = gtk_check_button_new_with_label("To Last Entry");
  if (plotToLast)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotToLastButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotToLastButton), FALSE);
  g_signal_connect (G_OBJECT(plotToLastButton), "clicked",
		    G_CALLBACK(plotToLastButtonCallback), NULL);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotToLastButton, 4, 7, 10, 11,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  if (logRoot == NULL) {
    plotStartButton = hildon_date_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
    plotEndButton   = hildon_date_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  } else {
    int minYear, maxYear, maxMonth, maxDay, minMonth, minDay;

    tJDToDate(logRoot->time,   &minYear, &minMonth, &minDay);
    tJDToDate(lastEntry->time, &maxYear, &maxMonth, &maxDay);
    plotStartButton = hildon_date_button_new_with_year_range(HILDON_SIZE_AUTO,
							     HILDON_BUTTON_ARRANGEMENT_VERTICAL,
							     minYear, maxYear);
    plotEndButton   = hildon_date_button_new_with_year_range(HILDON_SIZE_AUTO,
							     HILDON_BUTTON_ARRANGEMENT_VERTICAL,
							     minYear, maxYear);
    if ((plotStartYear >= minYear) && (plotStartYear <= maxYear)
	&& (plotStartMonth > 0)    && (plotStartMonth < 13)
	&& (plotStartDay > 0)      && (plotStartDay < 32)) {
      hildon_date_button_set_date((HildonDateButton *)plotStartButton,
				  plotStartYear, plotStartMonth-1, plotStartDay);
    } else {
      dprintf("Trying to set plot start to beginning of data\n");
      hildon_date_button_set_date((HildonDateButton *)plotStartButton,
				  minYear, minMonth-1, minDay);
    }
    if ((plotEndYear >= minYear) && (plotEndYear <= maxYear)
	&& (plotEndMonth > 0)    && (plotEndMonth < 13)
	&& (plotEndDay > 0)      && (plotEndDay < 32)) {
      hildon_date_button_set_date((HildonDateButton *)plotEndButton,
				  plotEndYear, plotEndMonth-1, plotEndDay);
    } else {
      dprintf("Trying to set plot end to end of data\n");
      hildon_date_button_set_date((HildonDateButton *)plotEndButton,
				  maxYear, maxMonth-1, maxDay);
    }
  }
  hildon_button_set_title((HildonButton *)plotStartButton, NULL);
  g_signal_connect(G_OBJECT(plotStartButton), "value-changed",
		   G_CALLBACK(plotStartButtonCallback), NULL);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotStartButton, 8, 11, 9, 10,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  hildon_button_set_title((HildonButton *)plotEndButton, NULL);
  g_signal_connect(G_OBJECT(plotEndButton), "value-changed",
		   G_CALLBACK(plotEndButtonCallback), NULL);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), plotEndButton, 8, 11, 10, 11,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  if (plotWindow)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotWindowButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plotFromButton), TRUE);

  fourthSeparator = gtk_separator_menu_item_new();
  gtk_table_attach(GTK_TABLE(plotSettingsTable), fourthSeparator, 0, 12, 11, 12,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  averageIntervalLabel = gtk_label_new("Calculate Weight Loss Over:");
  gtk_table_attach(GTK_TABLE(plotSettingsTable), averageIntervalLabel, 0, 4, 12, 13,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  aveWeekButton = gtk_radio_button_new_with_label(NULL, "Last Week");
  aveGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(aveWeekButton));
  aveFortnightButton = gtk_radio_button_new_with_label(aveGroup, "Last Fortnight");
  aveGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(aveFortnightButton));
  aveMonthButton = gtk_radio_button_new_with_label(aveGroup, "Last Month");
  aveGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(aveMonthButton));
  aveQuarterButton = gtk_radio_button_new_with_label(aveGroup, "Last Quarter");
  aveGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(aveQuarterButton));
  aveYearButton = gtk_radio_button_new_with_label(aveGroup, "Last Year");
  aveGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(aveYearButton));
  aveHistoryButton = gtk_radio_button_new_with_label(aveGroup, "Entire History");
  aveGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(aveHistoryButton));
  switch (aveInterval) {
  case ONE_WEEK:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(aveWeekButton), TRUE);
    break;
  case ONE_FORTNIGHT:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(aveFortnightButton), TRUE);
    break;
  case ONE_MONTH:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(aveMonthButton), TRUE);
    break;
  case ONE_QUARTER:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(aveQuarterButton), TRUE);
    break;
  case ONE_YEAR:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(aveYearButton), TRUE);
    break;
  default:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(aveHistoryButton), TRUE);
  }
  gtk_table_attach(GTK_TABLE(plotSettingsTable), aveWeekButton, 0, 3, 13, 14,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), aveFortnightButton, 4, 7, 13, 14,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), aveMonthButton, 8, 12, 13, 14,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), aveQuarterButton, 0, 3, 14, 15,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), aveYearButton, 4, 7, 14, 15,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(plotSettingsTable), aveHistoryButton, 8, 12, 14, 15,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  setPlotSensitivities();
  plotSettingsStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(plotSettingsStackable), "destroy",
		   G_CALLBACK(checkPlotSettings), NULL);
  gtk_container_add(GTK_CONTAINER(plotSettingsStackable), plotSettingsTable);
  gtk_widget_show_all(plotSettingsStackable);
} /* End of  P L O T  S E T T I N G S  B U T T O N  C L I C K E D  */

/*
  F I T  S E T T I N G S  B U T T O N  C L I C K E D

  This function builds and displays the widgets for the Goal Settings
  page.
 */
void fitSettingsButtonClicked(GtkButton *button, gpointer userData)
{
  static GtkWidget *fitSettingsTable;
  static GSList *fitGroup, *fitChoiceGroup;
  
  fitSettingsTable = gtk_table_new(9, 17, FALSE);

  /* Set up a radio box button group to select the least squares fit interval */
  fitNothingButton = gtk_radio_button_new_with_label(NULL, "Don't Fit Data");
  g_signal_connect (G_OBJECT(fitNothingButton), "clicked",
		    G_CALLBACK(fitNothingButtonCallback), NULL);
  fitChoiceGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(fitNothingButton));
  fitWindowButton = gtk_radio_button_new_with_label(fitChoiceGroup, "Fit Interval");
  g_signal_connect (G_OBJECT(fitWindowButton), "clicked",
		    G_CALLBACK(fitWindowButtonCallback), NULL);
  fitChoiceGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(fitWindowButton));
  fitFromButton = gtk_radio_button_new_with_label(fitChoiceGroup, "Fit After Date");
  g_signal_connect (G_OBJECT(fitFromButton), "clicked",
		    G_CALLBACK(fitFromButtonCallback), NULL);
  fitChoiceGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(fitFromButton));
  switch (fitType) {
  case DO_NOT_FIT_DATA:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fitNothingButton), TRUE);
    break;
  case FIT_WINDOW:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fitWindowButton), TRUE);
    break;
  default:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fitFromButton), TRUE);
  }
  if (logRoot == NULL)
    fitDateButton = hildon_date_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  else {
    int minYear, maxYear, m, d;

    tJDToDate(logRoot->time,   &minYear, &m, &d);
    tJDToDate(lastEntry->time, &maxYear, &m, &d);
    fitDateButton = hildon_date_button_new_with_year_range(HILDON_SIZE_AUTO,
							   HILDON_BUTTON_ARRANGEMENT_VERTICAL,
							   minYear, maxYear);
    if ((fitYear >= minYear) && (fitYear <= maxYear)
	&& (fitMonth > 0)    && (fitMonth < 13)
	&& (fitDay > 0)      && (fitDay < 32)) {
      hildon_date_button_set_date((HildonDateButton *)fitDateButton, fitYear, fitMonth-1, fitDay);
    } else
      dprintf("NOT trying to set the date to %d/%d/%d\n", fitMonth, fitDay, fitYear);
  }
  hildon_button_set_title((HildonButton *)fitDateButton, NULL);
  g_signal_connect(G_OBJECT(fitDateButton), "value-changed",
		   G_CALLBACK(fitDateButtonCallback), NULL);

  gtk_table_attach(GTK_TABLE(fitSettingsTable), fitNothingButton, 0, 2, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(fitSettingsTable), fitWindowButton, 2, 4, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(fitSettingsTable), fitFromButton, 4, 6, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  gtk_table_attach(GTK_TABLE(fitSettingsTable), fitDateButton, 6, 11, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  fitMonthButton = gtk_radio_button_new_with_label(NULL, "Fit Last Month");
  fitGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(fitMonthButton));
  fitQuarterButton = gtk_radio_button_new_with_label(fitGroup, "Fit Last Quarter");
  fitGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(fitQuarterButton));
  fit6MonthButton = gtk_radio_button_new_with_label(fitGroup, "Fit Last 6 Months");
  fitGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(fit6MonthButton));
  fitYearButton = gtk_radio_button_new_with_label(fitGroup, "Fit Last Year");
  fitGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(fitYearButton));
  fitHistoryButton = gtk_radio_button_new_with_label(fitGroup, "Fit Entire History");
  fitGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(fitHistoryButton));
  switch (fitInterval) {
  case -1:
    break;
  case ONE_MONTH:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fitMonthButton), TRUE);
    break;
  case ONE_QUARTER:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fitQuarterButton), TRUE);
    break;
  case ONE_SIX_MONTHS:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fit6MonthButton), TRUE);
    break;
  case ONE_YEAR:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fitYearButton), TRUE);
    break;
  default:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fitHistoryButton), TRUE);
  }
  gtk_table_attach(GTK_TABLE(fitSettingsTable), fitMonthButton, 0, 3, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(fitSettingsTable), fitQuarterButton, 4, 7, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(fitSettingsTable), fit6MonthButton, 8, 12, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(fitSettingsTable), fitYearButton, 0, 3, 2, 3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(fitSettingsTable), fitHistoryButton, 4, 7, 2, 3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  setFitSensitivities();

  fitSettingsStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(fitSettingsStackable), "destroy",
		   G_CALLBACK(checkFitSettings), NULL);
  gtk_container_add(GTK_CONTAINER(fitSettingsStackable), fitSettingsTable);
  gtk_widget_show_all(fitSettingsStackable);
} /* End of  F I T  S E T T I N G S  B U T T O N  C L I C K E D  */

/*
  A B O U T  Y O U  B U T T O N  C L I C K E D

  This function builds and displays the widgets for the AboutYou page.
 */
void aboutYouButtonClicked(GtkButton *button, gpointer userData)
{
  static GtkWidget *aboutYouTable, *firstSeparator,
    *secondSeparator;
  static GSList *weightGroup, *heightGroup, *judgementGroup;
  static GtkObject *heightAdjustment, *targetAdjustment;

  dprintf("in aboutYouButtonClicked()\n");
  aboutYouTable = gtk_table_new(9, 17, FALSE);

  heightUnitLabel = gtk_label_new("Your Height");
  gtk_table_attach(GTK_TABLE(aboutYouTable), (GtkWidget *)heightUnitLabel, 0, 2, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  inButton = gtk_radio_button_new_with_label(NULL, "in");
  heightGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(inButton));
  cmButton = gtk_radio_button_new_with_label(heightGroup, "cm");
  heightGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(cmButton));
  if (heightcm)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cmButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(inButton), TRUE);
  gtk_table_attach(GTK_TABLE(aboutYouTable), (GtkWidget *)inButton, 4, 7, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(aboutYouTable), (GtkWidget *)cmButton, 8, 12, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  heightAdjustment = gtk_adjustment_new(myHeight, 10.0, 216.0, 0.5, 1.0, 0.0);
  heightSpin = gtk_spin_button_new((GtkAdjustment *)heightAdjustment, 0.5, 1);
  gtk_spin_button_set_numeric((GtkSpinButton *)heightSpin, TRUE);
  gtk_table_attach(GTK_TABLE(aboutYouTable), (GtkWidget *)heightSpin, 2, 4, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  weightUnitLabel = gtk_label_new("Weight Unit:");
  gtk_table_attach(GTK_TABLE(aboutYouTable), (GtkWidget *)weightUnitLabel, 0, 2, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  poundsButton = gtk_radio_button_new_with_label(NULL, "lbs");
  weightGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(poundsButton));
  kgButton = gtk_radio_button_new_with_label(weightGroup, "kg");
  weightGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(kgButton));
  if (weightkg)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(kgButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(poundsButton), TRUE);
  gtk_table_attach(GTK_TABLE(aboutYouTable), (GtkWidget *)poundsButton, 4, 7, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(aboutYouTable), (GtkWidget *)kgButton, 8, 12, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  /* Set up a radio box button group to select whether gaining or losing weight is good */
  goalLabel = gtk_label_new("Weight Goal:");
  gtk_table_attach(GTK_TABLE(aboutYouTable), (GtkWidget *)goalLabel, 0, 2, 2, 3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  losingWeightIsGoodButton = gtk_radio_button_new_with_label(NULL, "Losing Weight");
  judgementGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(losingWeightIsGoodButton));
  gainingWeightIsGoodButton = gtk_radio_button_new_with_label(judgementGroup, "Gaining Weight");
  judgementGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(gainingWeightIsGoodButton));
  if (losingWeightIsGood)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(losingWeightIsGoodButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gainingWeightIsGoodButton), TRUE);
  gtk_table_attach(GTK_TABLE(aboutYouTable), losingWeightIsGoodButton, 4, 7, 2, 3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(aboutYouTable), gainingWeightIsGoodButton, 8, 12, 2, 3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  firstSeparator = gtk_separator_menu_item_new();
  gtk_table_attach(GTK_TABLE(aboutYouTable), firstSeparator, 0, 12, 3, 4,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  targetSpinLabel = gtk_label_new("Target Weight");
  gtk_table_attach(GTK_TABLE(aboutYouTable), (GtkWidget *)targetSpinLabel, 0, 2, 4, 5,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  targetAdjustment = gtk_adjustment_new(myTarget, 60.0, 250.0, 0.5, 1.0, 0.0);
  targetSpin = gtk_spin_button_new((GtkAdjustment *)targetAdjustment, 0.5, 1);
  gtk_spin_button_set_numeric((GtkSpinButton *)targetSpin, TRUE);
  gtk_table_attach(GTK_TABLE(aboutYouTable), (GtkWidget *)targetSpin, 2, 4, 4, 5,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  setGoalDateButton = gtk_check_button_new_with_label("Set Goal Date");
  g_signal_connect (G_OBJECT(setGoalDateButton), "clicked",
		    G_CALLBACK(setGoalDateButtonCallback), NULL);
  if (haveGoalDate)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(setGoalDateButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(setGoalDateButton), FALSE);
  if (logRoot == NULL)
    goalDateButton = hildon_date_button_new(HILDON_SIZE_AUTO,
					    HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  else {
    int minYear, maxYear, m, d;

    tJDToDate(logRoot->time,   &minYear, &m, &d);
    tJDToDate(lastEntry->time, &maxYear, &m, &d);
    goalDateButton = hildon_date_button_new_with_year_range(HILDON_SIZE_AUTO,
							    HILDON_BUTTON_ARRANGEMENT_VERTICAL,
							    minYear, maxYear+5);
    if ((targetYear >= 1900) && (targetYear <= 3000)
	&& (targetMonth > 0)    && (targetMonth < 13)
	&& (targetDay > 0)      && (targetDay < 32)) {
      hildon_date_button_set_date((HildonDateButton *)goalDateButton,
				  targetYear, targetMonth-1, targetDay);
    } else
      dprintf("NOT trying to set the date to %d/%d/%d\n",
	      targetMonth, targetDay, targetYear);
  }
  hildon_button_set_title((HildonButton *)goalDateButton, NULL);
  g_signal_connect(G_OBJECT(goalDateButton), "value-changed",
		   G_CALLBACK(goalDateButtonCallback), NULL);
  if (haveGoalDate)
    gtk_widget_set_sensitive(goalDateButton, TRUE);
  else
    gtk_widget_set_sensitive(goalDateButton, FALSE);

  gtk_table_attach(GTK_TABLE(aboutYouTable), setGoalDateButton, 4, 6, 4, 5,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(aboutYouTable), goalDateButton, 8, 10, 4, 5,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  secondSeparator = gtk_separator_menu_item_new();
  gtk_table_attach(GTK_TABLE(aboutYouTable), secondSeparator, 0, 12, 5, 6,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  hackerDietButton = gtk_check_button_new_with_label("Hacker Diet Mode");
  if (hackerDietMode)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hackerDietButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hackerDietButton), FALSE);
  gtk_table_attach(GTK_TABLE(aboutYouTable), hackerDietButton, 0, 3, 6, 7,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  sWDEButton = gtk_check_button_new_with_label("Start with Data Entry");
  if (startWithDataEntry)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sWDEButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sWDEButton), FALSE);
  gtk_table_attach(GTK_TABLE(aboutYouTable), sWDEButton, 4, 7, 6, 7,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  /*
  interpolateFirstPairButton = gtk_check_button_new_with_label("Interpolate First Pair");
  if (interpolateFirstPair)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(interpolateFirstPairButton), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(interpolateFirstPairButton), FALSE);
  gtk_table_attach(GTK_TABLE(aboutYouTable), interpolateFirstPairButton, 8, 11, 6, 7,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  */

  setGoalSensitivities();
  aboutYouStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(aboutYouStackable), "destroy",
		   G_CALLBACK(checkAboutYou), NULL);
  gtk_container_add(GTK_CONTAINER(aboutYouStackable), aboutYouTable);
  gtk_widget_show_all(aboutYouStackable);
} /* End of  A B O U T  Y O U  B U T T O N  C L I C K E D */

/*
  H E L P  B U T T O N  C L I C K E D

  This fucntion is called if Help is chosen fromt the main menu.
 */
void helpButtonClicked(GtkButton *button, gpointer userData)
{
  dprintf("in helpButtonClicked()\n");
  /* Send a dbus command to open the maeFat wiki page in the default browser */
  system("/usr/bin/dbus-send --system --type=method_call  --dest=\"com.nokia.osso_browser\" /com/nokia/osso_browser/request com.nokia.osso_browser.load_url string:\"wiki.maemo.org/maeFat\"");
} /* End of  H E L P  B U T T O N  C L I C K E D */

/*
  H A C K E R  D I E T  B U T T O N  C L I C K E D

  This fucntion is called if Hacker Diet Help is chosen fromt the main menu.
 */
void hackerDietButtonClicked(GtkButton *button, gpointer userData)
{
  dprintf("in hackerDietButtonClicked()\n");
  /* Send a dbus command to open the Hacke Diet page in the default browser */
  system("/usr/bin/dbus-send --system --type=method_call  --dest=\"com.nokia.osso_browser\" /com/nokia/osso_browser/request com.nokia.osso_browser.load_url string:\"www.fourmilab.ch/hackdiet/e4\"");
} /* End of  H A C K E R  D I E T  B U T T O N  C L I C K E D */

/*
  A B O U T  B U T T O N  C L I C K E D

  This function is called if About is selected from the main menu.
  It just prints a brief message about the program.
 */
void aboutButtonClicked(GtkButton *button, gpointer userData)
{
  static int firstCall = TRUE;
  static GtkTextBuffer *aboutBuffer;
  static GtkWidget *aboutTextWidget, *aboutStackable;
  
  dprintf("in aboutButtonClicked()\n");
  if (firstCall) {
    aboutBuffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_text(aboutBuffer, "maeFat Version 2.1\nCopyright (C) 2012, Ken Young\n\nThis program helps you keep track of your weight.\nThis is free, open source software, released under GPL version 2.\n\nPlease send comments, questions and feature requests to\norrery.moko@gmail.com", -1);
    aboutTextWidget = hildon_text_view_new(); 
    ((GtkTextView *)aboutTextWidget)->editable =
      ((GtkTextView *)aboutTextWidget)->cursor_visible = FALSE;
    gtk_widget_ref(aboutTextWidget);
    firstCall = FALSE;
  }
  hildon_text_view_set_buffer((HildonTextView *)aboutTextWidget, aboutBuffer);
  aboutStackable = hildon_stackable_window_new();
  gtk_container_add(GTK_CONTAINER(aboutStackable), aboutTextWidget);
  gtk_widget_show_all(aboutStackable);
} /* End of  A B O U T  B U T T O N  C L I C K E D */

/*
  M A K E  H I L D O N  B U T T O N S
  
  This function builds the main menu for the app.
*/
static void makeHildonButtons(void)
{
  static HildonAppMenu *hildonMenu;
  HildonSizeType buttonSize = HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH;

  hildonMenu = HILDON_APP_MENU(hildon_app_menu_new());

  /* Data Entry Button */
  dataEntryButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(dataEntryButton), "Data Entry");
  g_signal_connect(G_OBJECT(dataEntryButton), "clicked", G_CALLBACK(dataEntryButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(dataEntryButton));
  
  /* Log  Button */
  logButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(logButton), "Log");
  g_signal_connect(G_OBJECT(logButton), "clicked", G_CALLBACK(logButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(logButton));

  /* Trend Button */
  trendButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(trendButton), "Trend Analysis");
  g_signal_connect(G_OBJECT(trendButton), "clicked", G_CALLBACK(trendButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(trendButton));

  /* About You Button */
  aboutYouButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(aboutYouButton), "About You");
  g_signal_connect(G_OBJECT(aboutYouButton), "clicked", G_CALLBACK(aboutYouButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(aboutYouButton));

  /* Plot Settings Button */
  plotSettingsButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(plotSettingsButton), "Plot Settings");
  g_signal_connect(G_OBJECT(plotSettingsButton), "clicked", G_CALLBACK(plotSettingsButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(plotSettingsButton));

  /* Fit Settings Button */
  fitSettingsButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(fitSettingsButton), "Fit Settings");
  g_signal_connect(G_OBJECT(fitSettingsButton), "clicked", G_CALLBACK(fitSettingsButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(fitSettingsButton));

  /* Import / Export button */
  iEButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(iEButton), "Import/Export Data");
  g_signal_connect(G_OBJECT(iEButton), "clicked", G_CALLBACK(iEButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(iEButton));

  /* Hacker Diet Button */
  hackerDietButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(hackerDietButton), "Hacker Diet Website");
  g_signal_connect(G_OBJECT(hackerDietButton), "clicked", G_CALLBACK(hackerDietButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(hackerDietButton));

  /* Help Button */
  helpButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(helpButton), "maeFat Wiki Page");
  g_signal_connect(G_OBJECT(helpButton), "clicked", G_CALLBACK(helpButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(helpButton));

  /* About Button */
  aboutButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(aboutButton), "About maeFat");
  g_signal_connect(G_OBJECT(aboutButton), "clicked", G_CALLBACK(aboutButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(aboutButton));

  hildon_stackable_window_set_main_menu((HildonStackableWindow *)window, hildonMenu);
  gtk_widget_show_all(GTK_WIDGET(hildonMenu));
} /* End of  M A K E  H I L D O N  B U T T O N S */

/*
  B U T T O N  P R E S S  E V E N T

  This function is called when the screen is pressed
 */
static gboolean buttonPressEvent(GtkWidget *widget, GdkEventButton *event)
{
  dprintf("In buttonPressEvent\n");
  return(TRUE);
} /* End of  B U T T O N  P R E S S  E V E N T */

/*
  D A T A  E D I T  D E L E T E  C A L L B A C K

  This function is called if the delete button is pressed on the log
  edit page.
 */
void dataEditDeleteCallback(GtkButton *button, gpointer userData)
{
  deleted = TRUE;
  gtk_widget_destroy(dataEditStackable);
} /* End of  D A T A  E D I T  D E L E T E  C A L L B A C K */

/*
  E D I T  D A T A

  This function is called when a log entry has been edited.  It gets
  the new values from the widgets on the edit page.
 */
void editData(void)
{
  if (deleted) {
    if (editPtr->last == NULL)
      /* We're deleting the first entry in the list */
      logRoot = editPtr->next;
    else {
      (editPtr->last)->next = editPtr->next;
      if (editPtr->next != NULL)
	/* We're NOT deleting the last entry in the list */
	(editPtr->next)->last = editPtr->last;
      else
	/* We are deleting the last entry in the list */
	lastEntry = editPtr->last;
    }
    free(editPtr);
    nDataPoints--;
    if (nDataPoints == 0)
      logRoot = NULL;
    writeDataFile();
  } else {
    int iJD;
    int commentChanged = FALSE;
    float weight;
    double fJD;
    char *weightResult;
    char *comment = NULL;
    guint year, month, day, hours, minutes;
    
    hildon_date_button_get_date((HildonDateButton *)dateEditButton, &year, &month, &day);
    hildon_time_button_get_time((HildonTimeButton *)timeEditButton, &hours, &minutes);
    month += 1;
    weightResult = (char *)hildon_button_get_value(HILDON_BUTTON(weightButton));
    sscanf(weightResult, "%f", &weight);
    iJD = calculateJulianDate(year, month, day);
    fJD = (double)iJD + ((double)hours)/24.0 + ((double)minutes)/1440.0;
    comment = (char *)gtk_entry_get_text((GtkEntry *)commentText);
    if ((comment != NULL) && (strlen(comment) > 0)) {
      /* Got a comment which is not NULL */
      if (editPtr->comment != NULL) {
	/* The entry already has a comment */
	if (strcmp(editPtr->comment, comment))
	  commentChanged = TRUE;
      } else
	commentChanged = TRUE;
    } else {
	/* Comment is now empty - flag this as a change */
      commentChanged = TRUE;
      comment = NULL;
    }
    dprintf("Got %d/%d/%d  %02d:%02d %f %f \"%s\"\n",
	   day, month, year, hours, minutes, weight, fJD, comment);
    editPtr->time = fJD;
    editPtr->weight = weight;
    if (commentChanged) {
      if (editPtr->comment != NULL)
	free(editPtr->comment);
      if (comment != NULL) {
	editPtr->comment = malloc(strlen(comment)+1);
	if (editPtr->comment != NULL)
	  strcpy(editPtr->comment, comment);
	else
	  perror("malloc of chaged comment");
      } else
	editPtr->comment = comment;
    }
  }
  writeDataFile();
  gtk_widget_destroy(trendStackable);
} /* End of  E D I T  D A T A */

/*
  C R E A T E  E D I T  P A G E

  This function creates and displays the widgets needed for the Edit
  page.
 */
void createEditPage(logEntry *ptr)
{
  double dayFrac;
  int day, month, year, hours, minutes;
  static GtkWidget *weightSelector, *dataEditTable;

  dprintf("in createEditPage\n");
  deleted = FALSE;
  editPtr = ptr;
  dataEditTable = gtk_table_new(1, 5, FALSE);
  dateEditButton = hildon_date_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  tJDToDate((double)((int)ptr->time), &year, &month, &day);
  hildon_date_button_set_date((HildonDateButton *)dateEditButton, year, month-1, day);
  gtk_table_attach(GTK_TABLE(dataEditTable), dateEditButton, 0, 1, 0, 1,
		   GTK_EXPAND, GTK_EXPAND, 0, 0);
  
  timeEditButton = hildon_time_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  dayFrac = ptr->time - (double)((int)ptr->time);
  hours = (int)(dayFrac*24.0);
  minutes = (int)((dayFrac - ((double)hours)/24.0) * 1440.0 + 0.5);
  hildon_time_button_set_time((HildonTimeButton *)timeEditButton, hours, minutes);
  gtk_table_attach(GTK_TABLE(dataEditTable), timeEditButton, 0, 1, 1, 2,
		   GTK_EXPAND, GTK_EXPAND, 0, 0);
  dataEditStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(dataEditStackable), "destroy",
		   G_CALLBACK(editData), NULL);
  weightSelector = createTouchSelector();
  weightButton = hildon_picker_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  if (weightkg)
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(weightSelector), 0,
				     (int)((10.0*ptr->weight)+0.5) - 10*minSelectorWeight);
  else
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(weightSelector), 0,
				     (int)((5.0*ptr->weight)+0.5) - 5*minSelectorWeight);
  if (weightkg)
    hildon_button_set_title(HILDON_BUTTON(weightButton), "Weight (kg)");
  else
    hildon_button_set_title(HILDON_BUTTON(weightButton), "Weight (lbs)");
  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(weightButton),
				    HILDON_TOUCH_SELECTOR(weightSelector));
  gtk_table_attach(GTK_TABLE(dataEditTable), weightButton, 0, 1, 2, 3,
		   GTK_EXPAND, GTK_EXPAND, 0, 0);
  
  commentText = gtk_entry_new();
  if (ptr->comment != NULL)
    gtk_entry_set_text((GtkEntry *)commentText, ptr->comment);
  gtk_table_attach(GTK_TABLE(dataEditTable), commentText, 0, 1, 3, 4,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  dataEditDelete = gtk_button_new_with_label("Delete This Entry");
  g_signal_connect(G_OBJECT(dataEditDelete), "clicked",
		   G_CALLBACK(dataEditDeleteCallback), NULL);
  gtk_table_attach(GTK_TABLE(dataEditTable), dataEditDelete, 0, 1, 4, 5,
		   GTK_EXPAND, GTK_EXPAND, 0, 0);
  gtk_container_add(GTK_CONTAINER(dataEditStackable), dataEditTable);
  gtk_widget_show_all(dataEditStackable);
} /* End of  C R E A T E  E D I T  P A G E */

/*
  B U T T O N  R E L E A S E  E V E N T

  This function is called when a button press in the screen is released.
  It does nothing unless the Log page is displayed.   If the Log page
  is displayed, it checked to see if the press event corredinates
  corespond to one of the weight entry buttons.   If so, it builds an
  edit page, otherwise nothing happens.
 */
static gboolean buttonReleaseEvent(GtkWidget *widget, GdkEventButton *event)
{
  dprintf("In buttonReleaseEvent\n");
  if (logDisplayed) {
    int x, y;
    int found = FALSE;
    logCell *ptr;

    x = event->x; y = event->y;
    dprintf("The log is displayed (%d, %d)\n", x, y);
    /* Loop through all the data cells displayed on the Log page */
    ptr = logCellRoot;
    while ((ptr != NULL) && (!found)) {
      if ((ptr->box[0].x <= x) && (ptr->box[0].y <= y) && (ptr->box[2].x >= x) && (ptr->box[2].y >= y))
	found = TRUE;
      else
	ptr = ptr->next;
    }
    if (found) {
      int day, month, year;
      logCell *tptr, *ttptr;

      switch (ptr->data) {
      case DATA_CELL:
	tJDToDate((double)((int)ptr->ptr->time), &year, &month, &day);
	dprintf("Found it! (%d/%d/%d) (%f)\n", month, day, year, ptr->ptr->weight);
	createEditPage(ptr->ptr);
	break;
      case RIGHT_ARROW:
      case LEFT_ARROW:
	tptr = logCellRoot;
	while (tptr != NULL) {
	  ttptr = tptr;
	  tptr  = tptr->next;
	  free(ttptr);
	}
	logCellRoot = NULL;
	logDisplayed = FALSE;
	if (ptr->data == RIGHT_ARROW) {
	  printf("Right arrow clicked\n");
	  logPageOffset++;
	} else {
	  printf("Left arrow clicked\n");
	  logPageOffset--;
	}
	gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
			   TRUE, 0, 0,
			   drawingArea->allocation.width,
			   drawingArea->allocation.height);
	logCallback(NULL, NULL);
	gdk_draw_drawable(drawingArea->window,
			  drawingArea->style->fg_gc[GTK_WIDGET_STATE (drawingArea)],
			  pixmap,
			  0,0,0,0,
			  displayWidth, displayHeight);
	break;
      default:
	fprintf(stderr, "Unknown cell type (%d) found!\n", ptr->data);
      }
    }
  }
  return(TRUE);
} /* End of  B U T T O N  R E L E A S E  E V E N T */

int main(int argc, char **argv)
{
  osso_context_t *oSSOContext;

  homeDir = getenv("HOME");
  if (homeDir == NULL) {
    homeDir = malloc(strlen("/home/user")+1);
    if (homeDir == NULL) {
      perror("malloc of backup homeDir");
      exit(ERROR_EXIT);
    }
    sprintf(homeDir, "/home/user");
  }
  userDir = malloc(strlen(homeDir)+strlen("/.maeFat")+1);
  if (userDir == NULL) {
    perror("malloc of userDir");
    exit(ERROR_EXIT);
  }
  sprintf(userDir, "%s/.maeFat", homeDir);
  mkdir(userDir, 0777);
  mkdir(backupDir, 0777);

  fileName = malloc(strlen(userDir)+strlen("/data")+1);
  if (fileName == NULL) {
    perror("malloc of fileName");
    exit(ERROR_EXIT);
  }
  sprintf(fileName, "%s/data", userDir);
  dprintf("The data file path is \"%s\"\n", fileName);
  readData(fileName);

  settingsFileName = malloc(strlen(userDir)+strlen("/settings")+1);
  if (settingsFileName == NULL) {
    perror("malloc of settingsFileName");
    exit(ERROR_EXIT);
  }
  sprintf(settingsFileName, "%s/settings", userDir);
  dprintf("The settings file path is \"%s\"\n", settingsFileName);
  readSettings(settingsFileName);

  oSSOContext = osso_initialize("com.nokia.maefat", maeFatVersion, TRUE, NULL);
  if (!oSSOContext) {
    fprintf(stderr, "oss_initialize call failed\n");
    exit(-1);
  }

  hildon_gtk_init(&argc, &argv);
  /* Initialize main window */
  window = hildon_stackable_window_new();
  gtk_widget_set_size_request (GTK_WIDGET (window), 640, 480);
  gtk_window_set_title (GTK_WINDOW (window), "maeFat");
  g_signal_connect (G_OBJECT (window), "delete_event",
		    G_CALLBACK (gtk_main_quit), NULL);
  mainBox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), mainBox);
  g_object_ref(mainBox); /* This keeps mainBox from being destroyed when not displayed */
  gtk_widget_show(mainBox);

  /* Configure Main Menu Buttons */
  makeHildonButtons();

  drawingArea = gtk_drawing_area_new();
  gtk_widget_set_size_request (GTK_WIDGET(drawingArea), 640, 480);
  gtk_box_pack_end(GTK_BOX(mainBox), drawingArea, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(drawingArea), "expose_event",
		   G_CALLBACK(exposeEvent), NULL);
  g_signal_connect(G_OBJECT(drawingArea), "configure_event",
		   G_CALLBACK(configureEvent), NULL);

  g_signal_connect(G_OBJECT(drawingArea), "button_release_event",
		   G_CALLBACK(buttonReleaseEvent), NULL);
  g_signal_connect(G_OBJECT(drawingArea), "button_press_event",
		   G_CALLBACK(buttonPressEvent), NULL);
  gtk_widget_show(window);
  makeGraphicContexts(window);
  gtk_widget_set_events(drawingArea,
			GDK_EXPOSURE_MASK       | GDK_BUTTON_PRESS_MASK  |
			GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  gtk_widget_show(drawingArea);
  gtk_main();
  osso_deinitialize(oSSOContext);
  exit(OK_EXIT);
}
